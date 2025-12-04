#include "BiasShielding.h"
#include <algorithm>

namespace TapeHysteresis
{

// AC Bias Shielding Curve for 30 IPS Tape
// =======================================
// Models the frequency-dependent effectiveness of AC bias (~150kHz)
// at linearizing the magnetic recording process.
//
// At 30 IPS with 150kHz bias:
// - Low frequencies: Bias fully effective, linear response
// - Mid frequencies: Bias still effective
// - High frequencies: Bias wavelength approaches audio wavelength,
//                     effectiveness degrades, saturation increases
//
// Target curve (HFCut, applied BEFORE saturation):
//   20Hz-6kHz:   0dB   (flat - bias fully effective)
//   8kHz:       -1dB   (bias starting to weaken)
//   10kHz:      -2.5dB (noticeable degradation)
//   12kHz:      -5dB   (significant)
//   14kHz:      -7.5dB (bias struggling)
//   16kHz:      -9.5dB
//   18kHz:      -11dB
//   20kHz:      -12dB  (bias mostly ineffective)
//
// This models actual bias physics.
// The curve nulls with HFRestore, so frequencies that get
// cut here experience less saturation (protected by bias).

// ============================================================================
// Biquad design functions
// ============================================================================

static void designHighShelf(Biquad& filter, double fc, double gainDB, double Q, double fs)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = (A + 1.0) - (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha;
    filter.b0 = (A * ((A + 1.0) + (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosOmega)) / a0;
    filter.b2 = (A * ((A + 1.0) + (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosOmega)) / a0;
    filter.a2 = ((A + 1.0) - (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha) / a0;
}

static void designLowShelf(Biquad& filter, double fc, double gainDB, double Q, double fs)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = (A + 1.0) + (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha;
    filter.b0 = (A * ((A + 1.0) - (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cosOmega)) / a0;
    filter.b2 = (A * ((A + 1.0) - (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cosOmega)) / a0;
    filter.a2 = ((A + 1.0) + (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha) / a0;
}

static void designBell(Biquad& filter, double fc, double gainDB, double Q, double fs)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha / A;
    filter.b0 = (1.0 + alpha * A) / a0;
    filter.b1 = (-2.0 * cosOmega) / a0;
    filter.b2 = (1.0 - alpha * A) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha / A) / a0;
}

static void designLowPass(Biquad& filter, double fc, double Q, double fs)
{
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha;
    filter.b0 = ((1.0 - cosOmega) / 2.0) / a0;
    filter.b1 = (1.0 - cosOmega) / a0;
    filter.b2 = ((1.0 - cosOmega) / 2.0) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha) / a0;
}

static void designHighPass(Biquad& filter, double fc, double Q, double fs)
{
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha;
    filter.b0 = ((1.0 + cosOmega) / 2.0) / a0;
    filter.b1 = (-(1.0 + cosOmega)) / a0;
    filter.b2 = ((1.0 + cosOmega) / 2.0) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha) / a0;
}

// ============================================================================
// HFRestore - Restore HF after saturation (inverse of bias shielding curve)
// ============================================================================

HFRestore::HFRestore()
{
    updateCoefficients();
    reset();
}

void HFRestore::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    updateCoefficients();
}

void HFRestore::setMachineMode(bool isAmpex)
{
    if (ampexMode != isAmpex)
    {
        ampexMode = isAmpex;
        updateCoefficients();
    }
}

void HFRestore::reset()
{
    shelf1.reset();
    shelf2.reset();
    bell1.reset();
    bell2.reset();
    bell3.reset();
}

void HFRestore::updateCoefficients()
{
    double nyquist = fs / 2.0;

    double shelf1Freq, shelf1Gain, shelf1Q;
    double shelf2Freq, shelf2Gain, shelf2Q;
    double bell1Freq, bell1Gain, bell1Q;
    double bell2Freq, bell2Gain, bell2Q;
    double bell3Freq, bell3Gain, bell3Q;

    if (ampexMode)
    {
        // AMPEX ATR-102 HF Restore
        // -------------------------
        // Wide 1" head gap - bias loses effectiveness earlier
        // Target: Flat to 6kHz, smooth rise to +12dB at 20kHz
        //
        // The wider head gap means longer wavelengths at the gap,
        // so bias starts struggling at lower frequencies.

        // Shelf 1: Main HF restoration starting at 10kHz (pushed higher)
        shelf1Freq = std::min(10000.0, nyquist * 0.9);
        shelf1Gain = +7.5;
        shelf1Q = 1.0;

        // Shelf 2: Top octave at 16kHz
        shelf2Freq = std::min(16000.0, nyquist * 0.85);
        shelf2Gain = +4.5;
        shelf2Q = 0.85;

        // Bell 1: Gentle transition at 8kHz
        bell1Freq = std::min(8000.0, nyquist * 0.9);
        bell1Gain = +0.5;
        bell1Q = 1.8;

        // Bell 2: Top end push at 19kHz
        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = +1.5;
        bell2Q = 0.7;

        // Bell 3: Compensate spillover at 6kHz to keep it flat
        bell3Freq = 6000.0;
        bell3Gain = -0.3;
        bell3Q = 2.5;
    }
    else
    {
        // STUDER A820 HF Restore
        // -----------------------
        // Narrower head gaps on multitrack - bias effective longer
        // Target: Flat to 7kHz, smooth rise to +10dB at 20kHz
        //
        // The narrower gaps and higher bias oscillator frequency
        // mean bias stays effective to slightly higher frequencies.

        // Shelf 1: Main HF restoration starting at 10kHz
        shelf1Freq = std::min(10000.0, nyquist * 0.9);
        shelf1Gain = +7.0;
        shelf1Q = 1.0;

        // Shelf 2: Top octave at 17kHz
        shelf2Freq = std::min(17000.0, nyquist * 0.85);
        shelf2Gain = +3.0;
        shelf2Q = 0.85;

        // Bell 1: Gentle transition at 8kHz
        bell1Freq = std::min(8000.0, nyquist * 0.9);
        bell1Gain = +0.5;
        bell1Q = 1.8;

        // Bell 2: Top end at 19kHz
        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = +1.0;
        bell2Q = 0.8;

        // Bell 3: Keep 6kHz flat
        bell3Freq = 6000.0;
        bell3Gain = -0.3;
        bell3Q = 2.2;
    }

    TapeHysteresis::designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
    TapeHysteresis::designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
    TapeHysteresis::designBell(bell1, bell1Freq, bell1Gain, bell1Q, fs);
    TapeHysteresis::designBell(bell2, bell2Freq, bell2Gain, bell2Q, fs);
    TapeHysteresis::designBell(bell3, bell3Freq, bell3Gain, bell3Q, fs);
}

double HFRestore::processSample(double input)
{
    double output = shelf1.process(input);
    output = shelf2.process(output);
    output = bell1.process(output);
    output = bell2.process(output);
    output = bell3.process(output);
    return output;
}

// ============================================================================
// HFCut - Cut HF before saturation (models AC bias shielding)
// ============================================================================

HFCut::HFCut()
{
    updateCoefficients();
    reset();
}

void HFCut::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    updateCoefficients();
}

void HFCut::setMachineMode(bool isAmpex)
{
    if (ampexMode != isAmpex)
    {
        ampexMode = isAmpex;
        updateCoefficients();
    }
}

void HFCut::reset()
{
    shelf1.reset();
    shelf2.reset();
    bell1.reset();
    bell2.reset();
    bell3.reset();
}

void HFCut::updateCoefficients()
{
    double nyquist = fs / 2.0;

    double shelf1Freq, shelf1Gain, shelf1Q;
    double shelf2Freq, shelf2Gain, shelf2Q;
    double bell1Freq, bell1Gain, bell1Q;
    double bell2Freq, bell2Gain, bell2Q;
    double bell3Freq, bell3Gain, bell3Q;

    if (ampexMode)
    {
        // AMPEX ATR-102 HF Cut (EXACT INVERSE of HF Restore)
        // ---------------------------------------------------------
        // Target: Flat to 6kHz, -12dB at 20kHz

        shelf1Freq = std::min(10000.0, nyquist * 0.9);
        shelf1Gain = -7.5;
        shelf1Q = 1.0;

        shelf2Freq = std::min(16000.0, nyquist * 0.85);
        shelf2Gain = -4.5;
        shelf2Q = 0.85;

        bell1Freq = std::min(8000.0, nyquist * 0.9);
        bell1Gain = -0.5;
        bell1Q = 1.8;

        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = -1.5;
        bell2Q = 0.7;

        bell3Freq = 6000.0;
        bell3Gain = +0.3;
        bell3Q = 2.5;
    }
    else
    {
        // STUDER A820 HF Cut (EXACT INVERSE of HF Restore)
        // -------------------------------------------------------
        // Target: Flat to 7kHz, -10dB at 20kHz

        shelf1Freq = std::min(10000.0, nyquist * 0.9);
        shelf1Gain = -7.0;
        shelf1Q = 1.0;

        shelf2Freq = std::min(17000.0, nyquist * 0.85);
        shelf2Gain = -3.0;
        shelf2Q = 0.85;

        bell1Freq = std::min(8000.0, nyquist * 0.9);
        bell1Gain = -0.5;
        bell1Q = 1.8;

        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = -1.0;
        bell2Q = 0.8;

        bell3Freq = 6000.0;
        bell3Gain = +0.3;
        bell3Q = 2.2;
    }

    TapeHysteresis::designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
    TapeHysteresis::designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
    TapeHysteresis::designBell(bell1, bell1Freq, bell1Gain, bell1Q, fs);
    TapeHysteresis::designBell(bell2, bell2Freq, bell2Gain, bell2Q, fs);
    TapeHysteresis::designBell(bell3, bell3Freq, bell3Gain, bell3Q, fs);
}

double HFCut::processSample(double input)
{
    double output = shelf1.process(input);
    output = shelf2.process(output);
    output = bell1.process(output);
    output = bell2.process(output);
    output = bell3.process(output);
    return output;
}

} // namespace TapeHysteresis

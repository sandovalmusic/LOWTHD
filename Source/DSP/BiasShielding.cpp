#include "BiasShielding.h"
#include <algorithm>

namespace TapeHysteresis
{

// AC Bias Shielding Curve for 30 IPS Tape
// =======================================
// Models the frequency-dependent effectiveness of AC bias
// at linearizing the magnetic recording process.
//
// BIAS FREQUENCIES (from research):
//   Ampex ATR-102: 432 kHz (exceptionally high - best HF performance)
//   Studer A820:   153.6 kHz (typical professional standard)
//
// Rule of thumb: bias should be 5× highest audio frequency
//   ATR-102: 432/5 = 86 kHz clean range (far beyond 20kHz)
//   A820:    153/5 = 30 kHz clean range (still good, but less margin)
//
// Higher bias frequency = less HF degradation from self-erasure
// Therefore: ATR-102 has FLATTER HF response, A820 rolls off earlier
//
// Target curves (HFCut, applied BEFORE saturation):
//   ATR-102: Flat to 8kHz, gentle roll to -8dB at 20kHz
//   A820:    Flat to 6kHz, steeper roll to -12dB at 20kHz
//
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
        // 432 kHz bias frequency - exceptionally high
        // Best HF performance of any analog tape machine
        // Target: Flat to 8kHz, gentle rise to +8dB at 20kHz
        //
        // The very high bias frequency means minimal self-erasure
        // and excellent HF linearity - less correction needed.

        // Shelf 1: Main HF restoration starting at 12kHz
        shelf1Freq = std::min(12000.0, nyquist * 0.9);
        shelf1Gain = +5.5;
        shelf1Q = 1.0;

        // Shelf 2: Top octave at 18kHz
        shelf2Freq = std::min(18000.0, nyquist * 0.85);
        shelf2Gain = +2.5;
        shelf2Q = 0.85;

        // Bell 1: Gentle transition at 10kHz
        bell1Freq = std::min(10000.0, nyquist * 0.9);
        bell1Gain = +0.3;
        bell1Q = 1.8;

        // Bell 2: Top end at 19kHz
        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = +0.8;
        bell2Q = 0.8;

        // Bell 3: Keep 8kHz flat
        bell3Freq = 8000.0;
        bell3Gain = -0.2;
        bell3Q = 2.5;
    }
    else
    {
        // STUDER A820 HF Restore
        // -----------------------
        // 153.6 kHz bias frequency - typical professional standard
        // Target: Flat to 6kHz, steeper rise to +12dB at 20kHz
        //
        // Lower bias frequency means more self-erasure at HF,
        // requiring more aggressive HF restoration.

        // Shelf 1: Main HF restoration starting at 9kHz
        shelf1Freq = std::min(9000.0, nyquist * 0.9);
        shelf1Gain = +7.5;
        shelf1Q = 1.0;

        // Shelf 2: Top octave at 16kHz
        shelf2Freq = std::min(16000.0, nyquist * 0.85);
        shelf2Gain = +4.5;
        shelf2Q = 0.85;

        // Bell 1: Transition at 7kHz
        bell1Freq = std::min(7000.0, nyquist * 0.9);
        bell1Gain = +0.5;
        bell1Q = 1.8;

        // Bell 2: Top end push at 19kHz
        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = +1.5;
        bell2Q = 0.7;

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
        // 432 kHz bias - minimal HF cut needed
        // Target: Flat to 8kHz, -8dB at 20kHz

        shelf1Freq = std::min(12000.0, nyquist * 0.9);
        shelf1Gain = -5.5;
        shelf1Q = 1.0;

        shelf2Freq = std::min(18000.0, nyquist * 0.85);
        shelf2Gain = -2.5;
        shelf2Q = 0.85;

        bell1Freq = std::min(10000.0, nyquist * 0.9);
        bell1Gain = -0.3;
        bell1Q = 1.8;

        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = -0.8;
        bell2Q = 0.8;

        bell3Freq = 8000.0;
        bell3Gain = +0.2;
        bell3Q = 2.5;
    }
    else
    {
        // STUDER A820 HF Cut (EXACT INVERSE of HF Restore)
        // -------------------------------------------------------
        // 153.6 kHz bias - more HF cut needed
        // Target: Flat to 6kHz, -12dB at 20kHz

        shelf1Freq = std::min(9000.0, nyquist * 0.9);
        shelf1Gain = -7.5;
        shelf1Q = 1.0;

        shelf2Freq = std::min(16000.0, nyquist * 0.85);
        shelf2Gain = -4.5;
        shelf2Q = 0.85;

        bell1Freq = std::min(7000.0, nyquist * 0.9);
        bell1Gain = -0.5;
        bell1Q = 1.8;

        bell2Freq = std::min(19000.0, nyquist * 0.9);
        bell2Gain = -1.5;
        bell2Q = 0.7;

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

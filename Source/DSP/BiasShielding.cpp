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

    if (ampexMode)
    {
        // AMPEX ATR-102 HF Restore (exact inverse of HFCut)
        // ---------------------------------------------------------
        // Matches HFCut: Flat to 8kHz, gentle rise to +8dB at 20kHz

        shelf1Freq = std::min(8000.0, nyquist * 0.9);
        shelf1Gain = +4.0;
        shelf1Q = 0.7;

        shelf2Freq = std::min(14000.0, nyquist * 0.85);
        shelf2Gain = +4.0;
        shelf2Q = 0.7;
    }
    else
    {
        // STUDER A820 HF Restore (exact inverse of HFCut)
        // -------------------------------------------------------
        // Matches HFCut: Flat to 6kHz, moderate rise to +12dB at 20kHz

        shelf1Freq = std::min(6000.0, nyquist * 0.9);
        shelf1Gain = +6.0;
        shelf1Q = 0.7;

        shelf2Freq = std::min(12000.0, nyquist * 0.85);
        shelf2Gain = +6.0;
        shelf2Q = 0.7;
    }

    TapeHysteresis::designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
    TapeHysteresis::designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
    // Bell filters set to unity (no effect)
    bell1.b0 = 1.0; bell1.b1 = 0; bell1.b2 = 0; bell1.a1 = 0; bell1.a2 = 0;
    bell2.b0 = 1.0; bell2.b1 = 0; bell2.b2 = 0; bell2.a1 = 0; bell2.a2 = 0;
    bell3.b0 = 1.0; bell3.b1 = 0; bell3.b2 = 0; bell3.a1 = 0; bell3.a2 = 0;
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

    if (ampexMode)
    {
        // AMPEX ATR-102 HF Cut (30 IPS, AES EQ)
        // ---------------------------------------------------------
        // 432 kHz bias provides excellent HF linearity
        // At 30 IPS with AES EQ: very flat response, minimal pre-emphasis needed
        // Bias ratio: 432kHz / 20kHz = 21.6:1 (well above 5:1 rule)
        // Target: Flat to 8kHz, gentle roll to -8dB at 20kHz

        shelf1Freq = std::min(8000.0, nyquist * 0.9);
        shelf1Gain = -4.0;
        shelf1Q = 0.7;

        shelf2Freq = std::min(14000.0, nyquist * 0.85);
        shelf2Gain = -4.0;
        shelf2Q = 0.7;
    }
    else
    {
        // STUDER A820 HF Cut (30 IPS, AES EQ)
        // -------------------------------------------------------
        // 153.6 kHz bias - still excellent but less margin than ATR-102
        // At 30 IPS with AES EQ: flat response, slightly more pre-emphasis
        // Bias ratio: 153.6kHz / 20kHz = 7.7:1 (above 5:1 but less margin)
        // Target: Flat to 6kHz, moderate roll to -12dB at 20kHz

        shelf1Freq = std::min(6000.0, nyquist * 0.9);
        shelf1Gain = -6.0;
        shelf1Q = 0.7;

        shelf2Freq = std::min(12000.0, nyquist * 0.85);
        shelf2Gain = -6.0;
        shelf2Q = 0.7;
    }

    TapeHysteresis::designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
    TapeHysteresis::designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
    // Bell filters set to unity (no effect)
    bell1.b0 = 1.0; bell1.b1 = 0; bell1.b2 = 0; bell1.a1 = 0; bell1.a2 = 0;
    bell2.b0 = 1.0; bell2.b1 = 0; bell2.b2 = 0; bell2.a1 = 0; bell2.a2 = 0;
    bell3.b0 = 1.0; bell3.b1 = 0; bell3.b2 = 0; bell3.a1 = 0; bell3.a2 = 0;
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

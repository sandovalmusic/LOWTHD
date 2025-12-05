#include "BiasShielding.h"
#include <algorithm>

namespace TapeHysteresis
{

// High shelf filter design
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
}

void HFCut::updateCoefficients()
{
    double nyquist = fs / 2.0;

    if (ampexMode)
    {
        // AMPEX ATR-102: 432 kHz bias
        // Flat to 8kHz, -8dB at 20kHz
        double shelf1Freq = std::min(8000.0, nyquist * 0.9);
        double shelf2Freq = std::min(14000.0, nyquist * 0.85);
        designHighShelf(shelf1, shelf1Freq, -4.0, 0.7, fs);
        designHighShelf(shelf2, shelf2Freq, -4.0, 0.7, fs);
    }
    else
    {
        // STUDER A820: 153.6 kHz bias
        // Flat to 6kHz, -12dB at 20kHz
        double shelf1Freq = std::min(6000.0, nyquist * 0.9);
        double shelf2Freq = std::min(12000.0, nyquist * 0.85);
        designHighShelf(shelf1, shelf1Freq, -6.0, 0.7, fs);
        designHighShelf(shelf2, shelf2Freq, -6.0, 0.7, fs);
    }
}

double HFCut::processSample(double input)
{
    return shelf2.process(shelf1.process(input));
}

} // namespace TapeHysteresis

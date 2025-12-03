#include "PreEmphasis.h"
#include <complex>

namespace TapeHysteresis
{

// Shared filter design functions to avoid code duplication
namespace BiquadDesign
{
    void designHighShelf(double& b0, double& b1, double& b2, double& a1, double& a2,
                        double fc, double gainDB, double Q, double fs)
    {
        // High-shelf filter using RBJ cookbook
        double A = std::pow(10.0, gainDB / 40.0);  // sqrt of gain
        double omega = 2.0 * M_PI * fc / fs;
        double cosOmega = std::cos(omega);
        double sinOmega = std::sin(omega);
        double alpha = sinOmega / (2.0 * Q);

        double a0 = (A + 1.0) - (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha;
        b0 = (A * ((A + 1.0) + (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha)) / a0;
        b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosOmega)) / a0;
        b2 = (A * ((A + 1.0) + (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha)) / a0;
        a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosOmega)) / a0;
        a2 = ((A + 1.0) - (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha) / a0;
    }

    void designBell(double& b0, double& b1, double& b2, double& a1, double& a2,
                   double fc, double gainDB, double Q, double fs)
    {
        // Parametric bell/peaking filter using RBJ cookbook
        double A = std::pow(10.0, gainDB / 40.0);  // sqrt of gain
        double omega = 2.0 * M_PI * fc / fs;
        double cosOmega = std::cos(omega);
        double sinOmega = std::sin(omega);
        double alpha = sinOmega / (2.0 * Q);

        double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * cosOmega) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cosOmega) / a0;
        a2 = (1.0 - alpha / A) / a0;
    }
}


ReEmphasis::ReEmphasis()
{
    updateCoefficients();
    reset();
}

void ReEmphasis::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    updateCoefficients();
}

void ReEmphasis::reset()
{
    shelf1.reset();
    shelf2.reset();
    shelf3.reset();
    bell1.reset();
    bell2.reset();
}

void ReEmphasis::updateCoefficients()
{
    // Design using Pro-Q 4 style shelves to match CCIR curve precisely
    // Target: +0.2dB @ 1.5k, +0.5dB @ 3k, +1.3dB @ 5k, +2.0dB @ 7k,
    //         +3.5dB @ 10k, +5.5dB @ 15k, +7.0dB @ 20k, +9.0dB @ 25k
    // Tolerance: ±0.05 dB at each frequency
    //
    // Strategy: Use three shelves + two bells for ultimate precision

    // Shelf 1: Primary rise starting at 9095 Hz, +5.19 dB (increased for 10k/25k)
    designHighShelf(shelf1, 9095.0, 5.19, 0.42);

    // Shelf 2: Secondary rise at 12500 Hz, +1.30 dB (reduced to lower 3k/7k)
    designHighShelf(shelf2, 12500.0, 1.30, 0.48);

    // Shelf 3: Tertiary rise at 17000 Hz, +2.66 dB (increased for 25k)
    designHighShelf(shelf3, 17000.0, 2.66, 0.52);

    // Bell 1: Cut at 5.5 kHz to reduce mid-frequency overshoot (-0.38 dB)
    designBell(bell1, 5500.0, -0.38, 1.2);

    // Bell 2: Narrow cut at 20.0 kHz to precisely target 20k (-1.06 dB)
    designBell(bell2, 20000.0, -1.06, 0.90);
}

void ReEmphasis::designHighShelf(Biquad& filter, double fc, double gainDB, double Q)
{
    BiquadDesign::designHighShelf(filter.b0, filter.b1, filter.b2, filter.a1, filter.a2, fc, gainDB, Q, fs);
}

void ReEmphasis::designBell(Biquad& filter, double fc, double gainDB, double Q)
{
    BiquadDesign::designBell(filter.b0, filter.b1, filter.b2, filter.a1, filter.a2, fc, gainDB, Q, fs);
}

double ReEmphasis::processSample(double input)
{
    double output = shelf1.process(input);
    output = shelf2.process(output);
    output = shelf3.process(output);
    output = bell1.process(output);
    output = bell2.process(output);
    return output;
}

double ReEmphasis::getMagnitudeDB(double frequency) const
{
    // Calculate magnitude response at given frequency
    double omega = 2.0 * M_PI * frequency / fs;
    std::complex<double> z = std::exp(std::complex<double>(0.0, omega));
    std::complex<double> z2 = z * z;

    // Calculate H(z) for each shelf
    auto calcMagnitude = [](const Biquad& f, std::complex<double> z, std::complex<double> z2) {
        std::complex<double> num = f.b0 + f.b1 / z + f.b2 / z2;
        std::complex<double> den = 1.0 + f.a1 / z + f.a2 / z2;
        return std::abs(num / den);
    };

    double mag1 = calcMagnitude(shelf1, z, z2);
    double mag2 = calcMagnitude(shelf2, z, z2);
    double mag3 = calcMagnitude(shelf3, z, z2);
    double mag4 = calcMagnitude(bell1, z, z2);
    double mag5 = calcMagnitude(bell2, z, z2);

    double totalMag = mag1 * mag2 * mag3 * mag4 * mag5;
    return 20.0 * std::log10(totalMag);
}

// De-Emphasis implementation (applied BEFORE saturation to cut highs)

DeEmphasis::DeEmphasis()
{
    updateCoefficients();
    reset();
}

void DeEmphasis::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    updateCoefficients();
}

void DeEmphasis::reset()
{
    shelf1.reset();
    shelf2.reset();
    shelf3.reset();
    bell1.reset();
    bell2.reset();
}

void DeEmphasis::updateCoefficients()
{
    // Inverse of re-emphasis: negative gains (must match ReEmphasis!)

    // Shelf 1: Inverse of primary rise
    designHighShelf(shelf1, 9095.0, -5.19, 0.42);

    // Shelf 2: Inverse of secondary rise
    designHighShelf(shelf2, 12500.0, -1.30, 0.48);

    // Shelf 3: Inverse of tertiary rise
    designHighShelf(shelf3, 17000.0, -2.66, 0.52);

    // Bell 1: Inverse of mid cut (becomes boost)
    designBell(bell1, 5500.0, +0.38, 1.2);

    // Bell 2: Inverse of 20.0k cut (becomes boost)
    designBell(bell2, 20000.0, +1.06, 0.90);
}

void DeEmphasis::designHighShelf(Biquad& filter, double fc, double gainDB, double Q)
{
    BiquadDesign::designHighShelf(filter.b0, filter.b1, filter.b2, filter.a1, filter.a2, fc, gainDB, Q, fs);
}

void DeEmphasis::designBell(Biquad& filter, double fc, double gainDB, double Q)
{
    BiquadDesign::designBell(filter.b0, filter.b1, filter.b2, filter.a1, filter.a2, fc, gainDB, Q, fs);
}

double DeEmphasis::processSample(double input)
{
    double output = shelf1.process(input);
    output = shelf2.process(output);
    output = shelf3.process(output);
    output = bell1.process(output);
    output = bell2.process(output);
    return output;
}

} // namespace TapeHysteresis

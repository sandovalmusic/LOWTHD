#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TapeHysteresis
{

// Biquad filter (Direct Form II Transposed)
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() { z1 = z2 = 0.0; }

    double process(double input)
    {
        double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }
};

// CCIR 30 IPS Standard Time Constants
// ====================================
// 30 IPS uses τ = 35 μs
// Turnover frequency: f_t = 1/(2πτ) = 4547 Hz
//
// Reference CCIR curve: G(f) = sqrt(1 + (f/4547)²)
// In dB: 10 * log10(1 + (f/4547)²)
//
// Target values:
//   1 kHz:   +0.21 dB
//   4.5 kHz: +3.01 dB (turnover)
//   10 kHz:  +7.66 dB
//   15 kHz:  +10.75 dB
//   20 kHz:  +13.08 dB

// 30 IPS CCIR Re-Emphasis (applied AFTER saturation)
// Uses cascaded shelves + bells to match CCIR curve accurately
class ReEmphasis
{
public:
    ReEmphasis();
    void setSampleRate(double sampleRate);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    Biquad shelf1;      // First high shelf
    Biquad shelf2;      // Second high shelf
    Biquad bell1;       // Correction bell 1
    Biquad bell2;       // Correction bell 2
    Biquad bell3;       // HF shaping

    void updateCoefficients();
};

// 30 IPS CCIR De-Emphasis (applied BEFORE saturation)
// Exact inverse of re-emphasis
class DeEmphasis
{
public:
    DeEmphasis();
    void setSampleRate(double sampleRate);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    Biquad shelf1;
    Biquad shelf2;
    Biquad bell1;
    Biquad bell2;
    Biquad bell3;

    void updateCoefficients();
};

} // namespace TapeHysteresis

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

// HFCut - Cut HF before saturation (models AC bias shielding)
//
// Models the frequency-dependent effectiveness of AC bias at linearizing
// the magnetic recording process. Used with parallel clean HF path:
//   cleanHF = input - HFCut(input)
//   output = saturate(HFCut(input)) + cleanHF
//
// BIAS FREQUENCIES:
//   Ampex ATR-102: 432 kHz (excellent HF linearity)
//   Studer A820:   153.6 kHz (good HF linearity)
//
// Target curves:
//   ATR-102: Flat to 8kHz, -8dB at 20kHz
//   A820:    Flat to 6kHz, -12dB at 20kHz
class HFCut
{
public:
    HFCut();
    void setSampleRate(double sampleRate);
    void setMachineMode(bool isAmpex);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    bool ampexMode = true;
    Biquad shelf1;
    Biquad shelf2;

    void updateCoefficients();
};

} // namespace TapeHysteresis

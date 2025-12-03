#pragma once

#include <cmath>

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

// 30 IPS CCIR Re-Emphasis (applied AFTER saturation)
class ReEmphasis
{
public:
    ReEmphasis();
    void setSampleRate(double sampleRate);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    Biquad shelf1, shelf2, shelf3, bell1, bell2;

    void updateCoefficients();
    void designHighShelf(Biquad& filter, double fc, double gainDB, double Q);
    void designBell(Biquad& filter, double fc, double gainDB, double Q);
};

// De-Emphasis (applied BEFORE saturation - inverse of re-emphasis)
class DeEmphasis
{
public:
    DeEmphasis();
    void setSampleRate(double sampleRate);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    Biquad shelf1, shelf2, shelf3, bell1, bell2;

    void updateCoefficients();
    void designHighShelf(Biquad& filter, double fc, double gainDB, double Q);
    void designBell(Biquad& filter, double fc, double gainDB, double Q);
};

} // namespace TapeHysteresis

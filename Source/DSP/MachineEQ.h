#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TapeHysteresis
{

// Biquad filter using Audio EQ Cookbook formulas
struct EQBiquad
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

    // Bell/Peaking EQ (Audio EQ Cookbook)
    void setBell(double fc, double Q, double gainDB, double sampleRate)
    {
        double A = std::pow(10.0, gainDB / 40.0);
        double w0 = 2.0 * M_PI * fc / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        double a0 = 1.0 + alpha / A;
        b0 = (1.0 + alpha * A) / a0;
        b1 = (-2.0 * cosw0) / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = (-2.0 * cosw0) / a0;
        a2 = (1.0 - alpha / A) / a0;
    }

    // High-pass filter (2nd order, 12 dB/oct)
    void setHighPass(double fc, double Q, double sampleRate)
    {
        double w0 = 2.0 * M_PI * fc / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        double a0 = 1.0 + alpha;
        b0 = ((1.0 + cosw0) / 2.0) / a0;
        b1 = (-(1.0 + cosw0)) / a0;
        b2 = ((1.0 + cosw0) / 2.0) / a0;
        a1 = (-2.0 * cosw0) / a0;
        a2 = (1.0 - alpha) / a0;
    }
};

// 1st order filter for 6 dB/oct slopes
struct FirstOrderFilter
{
    double b0 = 1.0, b1 = 0.0;
    double a1 = 0.0;
    double z1 = 0.0;

    void reset() { z1 = 0.0; }

    double process(double input)
    {
        double output = b0 * input + z1;
        z1 = b1 * input - a1 * output;
        return output;
    }

    // 1st order high-pass (6 dB/oct)
    void setHighPass(double fc, double sampleRate)
    {
        double K = std::tan(M_PI * fc / sampleRate);
        double a0 = 1.0 + K;
        b0 = 1.0 / a0;
        b1 = -1.0 / a0;
        a1 = (K - 1.0) / a0;
    }
};

/**
 * Machine-specific EQ curves from Jack Endino's measurements
 * Applied AFTER saturation to capture the total frequency response.
 *
 * Note: 30kHz bands are omitted as they're above Nyquist at 48kHz
 * and the gains are too small to matter even when oversampled.
 */
class MachineEQ
{
public:
    enum class Machine { Ampex, Studer };

    MachineEQ();

    void setSampleRate(double sampleRate);
    void setMachine(Machine machine);
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    Machine currentMachine = Machine::Ampex;

    // Ampex ATR-102 "Master" EQ
    EQBiquad ampexHP;           // 20 Hz, 12 dB/oct
    EQBiquad ampexBell1;        // 40 Hz, Q 1.58, +1.4 dB
    EQBiquad ampexBell2;        // 65 Hz, Q 1.265, -2.0 dB
    EQBiquad ampexBell3;        // 75 Hz, Q 0.8, +2.0 dB
    EQBiquad ampexBell4;        // 230 Hz, Q 0.6, -0.8 dB
    EQBiquad ampexBell5;        // 6000 Hz, Q 0.4, -0.6 dB

    // Studer A820 "Tracks" EQ
    EQBiquad studerHP1;         // 30 Hz, 12 dB/oct
    FirstOrderFilter studerHP2; // 30 Hz, 6 dB/oct (cascaded = 18 dB/oct total)
    EQBiquad studerBell1;       // 32 Hz, Q 1.5, +0.4 dB
    EQBiquad studerBell2;       // 72 Hz, Q 2.07, -2.7 dB
    EQBiquad studerBell3;       // 85 Hz, Q 1.0, +3.2 dB
    EQBiquad studerBell4;       // 180 Hz, Q 1.0, -0.8 dB
    EQBiquad studerBell5;       // 600 Hz, Q 0.8, +0.2 dB
    EQBiquad studerBell6;       // 2000 Hz, Q 1.0, +0.1 dB
    EQBiquad studerBell7;       // 5000 Hz, Q 1.0, +0.1 dB
    EQBiquad studerBell8;       // 10000 Hz, Q 1.0, -0.1 dB

    void updateCoefficients();
};

} // namespace TapeHysteresis

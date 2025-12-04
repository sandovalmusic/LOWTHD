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

// AC Bias Shielding Curve for 30 IPS Tape
// =======================================
// Models the frequency-dependent effectiveness of AC bias (~150kHz)
// at linearizing the magnetic recording process.
//
// Different machines have slightly different bias characteristics:
//
// STUDER A820 (Tracks Mode):
//   - Higher bias frequency oscillator
//   - Bias stays effective slightly higher in frequency
//   - Target: Flat to 7kHz, then -10dB at 20kHz
//
// AMPEX ATR-102 (Master Mode):
//   - Wide 1" head gap, different bias behavior
//   - Bias loses effectiveness slightly earlier
//   - Target: Flat to 6kHz, then -12dB at 20kHz
//
// Both curves null with their inverse (HFRestore),
// so frequencies cut here experience less saturation.

// HFRestore - Restore HF after saturation (inverse of bias shielding)
// Applied AFTER saturation to restore the high frequencies
// Exact inverse of HFCut for perfect null when cascaded
class HFRestore
{
public:
    HFRestore();
    void setSampleRate(double sampleRate);
    void setMachineMode(bool isAmpex);  // Switch between machine curves
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    bool ampexMode = true;  // true = Ampex ATR-102, false = Studer A820
    Biquad shelf1;      // Main HF restoration
    Biquad shelf2;      // Top octave
    Biquad bell1;       // Transition smoothing
    Biquad bell2;       // Final push
    Biquad bell3;       // Mid compensation

    void updateCoefficients();
};

// HFCut - Cut HF before saturation (models AC bias shielding)
// Applied BEFORE saturation to protect HF from over-saturation
// Frequencies that are cut here experience less saturation
// because AC bias would be protecting them on real tape
class HFCut
{
public:
    HFCut();
    void setSampleRate(double sampleRate);
    void setMachineMode(bool isAmpex);  // Switch between machine curves
    void reset();
    double processSample(double input);

private:
    double fs = 48000.0;
    bool ampexMode = true;  // true = Ampex ATR-102, false = Studer A820
    Biquad shelf1;
    Biquad shelf2;
    Biquad bell1;
    Biquad bell2;
    Biquad bell3;

    void updateCoefficients();
};

} // namespace TapeHysteresis

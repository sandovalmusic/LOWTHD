#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "PreEmphasis.h"
#include "JilesAthertonCore.h"
#include "MachineEQ.h"

namespace TapeHysteresis
{

/**
 * Hybrid Tape Saturation Processor
 *
 * Combines asymmetric tanh saturation with Jiles-Atherton hysteresis
 * to accurately model analog tape characteristics.
 *
 * Two machine modes with distinct saturation characteristics:
 *
 * MASTER MODE (Ampex ATR-102):
 *   - Ultra-linear 1" mastering machine
 *   - Extended headroom, clean saturation
 *   - MOL (3% THD) at +12 dB
 *   - E/O asymmetry: 0.503 (odd-dominant)
 *
 * TRACKS MODE (Studer A820):
 *   - 24-track workhorse
 *   - Warm, punchy, musical saturation
 *   - MOL (3% THD) at +9 dB
 *   - E/O asymmetry: 1.122 (even-dominant)
 *
 * Signal Flow:
 *   1. Input gain
 *   2. De-emphasis (CCIR 30 IPS) - cut highs before saturation
 *   3. J-A hysteresis path (magnetic memory)
 *   4. Asymmetric tanh saturation path (THD curve + E/O ratio)
 *   5. Level-dependent parallel blend of J-A and tanh paths
 *   6. Re-emphasis (CCIR 30 IPS) - restore highs after saturation
 *   7. Machine EQ (Jack Endino measurements) - optional "Tape Bump"
 *   8. HF dispersive allpass (tape head phase smear)
 *   9. DC blocking (4th-order @ 5Hz)
 *
 * The hybrid approach provides:
 *   - Correct THD vs level curve (from tanh)
 *   - Correct even/odd harmonic balance (from asymmetry)
 *   - History-dependent "tape memory" (from J-A)
 *   - Frequency-dependent saturation (from pre/de-emphasis)
 *   - HF phase smear / "soft focus" (from dispersive allpass)
 */

class HybridTapeProcessor
{
public:
    HybridTapeProcessor();
    ~HybridTapeProcessor() = default;

    void setSampleRate(double sampleRate);
    void reset();

    /**
     * Set tape parameters
     * @param biasStrength - Machine mode selector (0.0 to 1.0)
     *                       < 0.74 = Master (Ampex ATR-102)
     *                       >= 0.74 = Tracks (Studer A820)
     * @param inputGain - Input gain scaling
     * @param tapeBumpEnabled - Enable machine-specific EQ curve
     */
    void setParameters(double biasStrength, double inputGain, bool tapeBumpEnabled = true);

    /**
     * Process a single sample through the tape saturation model
     */
    double processSample(double input);

    /**
     * Process right channel with azimuth delay
     * Master (Ampex): 8μs delay
     * Tracks (Studer): 12μs delay
     */
    double processRightChannel(double input);

private:
    // Azimuth delay buffer for right channel
    static constexpr int DELAY_BUFFER_SIZE = 4;
    double delayBuffer[DELAY_BUFFER_SIZE] = {0.0};
    int delayWriteIndex = 0;
    double cachedDelaySamples = 0.0;

    // Parameters
    double currentBiasStrength = 0.65;
    double currentInputGain = 1.0;
    bool isAmpexMode = true;
    bool tapeBumpEnabled = true;

    // Sample rate
    double fs = 48000.0;

    // Tanh saturation parameters
    double tanhDrive = 0.11;      // Controls saturation intensity
    double tanhAsymmetry = 0.80;  // Controls even/odd harmonic balance
    double tanhBias = 0.0;        // Cached: tanhAsymmetry - 1.0
    double tanhDcOffset = 0.0;    // Cached: tanh(tanhDrive * tanhBias)
    double tanhNormFactor = 1.0;  // Cached: 1.0 / (tanhDrive * (1 - dcOffset^2))

    // Atan saturation parameters (level-dependent series blend after tanh)
    // Adds extra knee steepness at high levels without affecting low-level THD
    double atanDrive = 0.5;       // Atan saturation intensity
    double atanMixMax = 0.0;      // Maximum atan blend at high levels
    double atanThreshold = 1.5;   // Level where atan starts blending in (~+3.5dB)
    double atanWidth = 1.0;       // Crossfade width
    double atanAsymmetry = 1.0;   // 1.0 = symmetric (Ampex), >1.0 = asymmetric (Studer)
    bool useAsymmetricAtan = false; // false = symmetric atan, true = asymmetric atan
    double atanBias = 0.0;        // Cached: atanAsymmetry - 1.0
    double atanDcOffset = 0.0;    // Cached: atan(atanDrive * atanBias)
    double atanNormFactor = 1.0;  // Cached normalization factor

    // Helper functions
    void updateCachedValues();
    double asymmetricTanh(double x);
    double softAtan(double x);           // Normalized atan saturation (symmetric, odd harmonics)
    double asymmetricAtan(double x);     // Asymmetric atan (even harmonics for Studer)

    // Level-dependent J-A blend parameters
    // At low levels: J-A is silent (clean response)
    // At high levels: J-A fades in for magnetic hysteresis character
    double jaBlendMax = 0.50;     // Maximum J-A blend at high levels
    double jaBlendThreshold = 0.3; // Level where J-A starts blending in
    double jaBlendWidth = 0.4;    // Crossfade width
    double jaEnvelope = 0.0;      // Envelope follower for smooth blend

    // DC blocking filter (4th-order Butterworth @ 5Hz)
    struct Biquad {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        void reset() { z1 = z2 = 0.0; }

        double process(double input) {
            double output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }
    };

    Biquad dcBlocker1, dcBlocker2;

    // Re-emphasis and de-emphasis (CCIR 30 IPS)
    // Applied before/after the saturation blend (both paths see same EQ)
    ReEmphasis reEmphasis;
    DeEmphasis deEmphasis;

    // HF dispersive allpass - creates frequency-dependent phase shift
    // Emulates tape head phase smear ("soft focus" effect on transients)
    // Higher frequencies get more phase shift, creating the tape "air"
    struct AllpassFilter {
        double coefficient = 0.0;
        double z1 = 0.0;

        void setFrequency(double freq, double sampleRate) {
            // First-order allpass: H(z) = (a + z^-1) / (1 + a*z^-1)
            // Phase shift is 180° at DC, 0° at Nyquist, 90° at the tuning frequency
            double w0 = 2.0 * M_PI * freq / sampleRate;
            double tanHalf = std::tan(w0 / 2.0);
            coefficient = (1.0 - tanHalf) / (1.0 + tanHalf);
        }

        void reset() { z1 = 0.0; }

        double process(double input) {
            double output = coefficient * input + z1;
            z1 = input - coefficient * output;
            return output;
        }
    };

    static constexpr int NUM_DISPERSIVE_STAGES = 4;
    AllpassFilter dispersiveAllpass[NUM_DISPERSIVE_STAGES];
    double dispersiveCornerFreq = 4000.0;  // Base corner frequency

    // Jiles-Atherton hysteresis core - adds magnetic tape character
    JilesAthertonCore jaCore;
    double jaInputScale = 3.0;    // Input scaling to J-A working range
    double jaOutputScale = 1.0;   // Output scaling back to audio range

    // Machine-specific EQ (Jack Endino measurements)
    MachineEQ machineEQ;

    // Note: PluginProcessor handles 2x oversampling externally via JUCE's Oversampling class
};

} // namespace TapeHysteresis

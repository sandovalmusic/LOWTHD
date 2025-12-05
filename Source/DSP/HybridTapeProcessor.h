#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "BiasShielding.h"
#include "JilesAthertonCore.h"
#include "MachineEQ.h"

namespace TapeHysteresis
{

/**
 * Hybrid Tape Saturation Processor
 *
 * Three parallel paths with level-dependent blending:
 *   1. Tanh → Atan (primary saturation with soft knee at high levels)
 *   2. Jiles-Atherton (physics-based hysteresis, blends in at higher levels)
 *   3. Clean HF (bypasses saturation entirely for AC-bias-shielded frequencies)
 *
 * MASTER MODE (Ampex ATR-102):
 *   - MOL (3% THD) at +12dB, E/O = 0.45 (odd-dominant)
 *   - THD @ 0dB: ~0.32%
 *
 * TRACKS MODE (Studer A820):
 *   - MOL (3% THD) at +9dB, E/O = 1.06 (even-dominant)
 *   - THD @ 0dB: ~0.95%
 */
class HybridTapeProcessor
{
public:
    HybridTapeProcessor();
    ~HybridTapeProcessor() = default;

    void setSampleRate(double sampleRate);
    void reset();

    /**
     * @param biasStrength - < 0.74 = Master (Ampex), >= 0.74 = Tracks (Studer)
     * @param inputGain - Input gain scaling
     */
    void setParameters(double biasStrength, double inputGain);

    double processSample(double input);
    double processRightChannel(double input);  // With azimuth delay

private:
    // Azimuth delay buffer (supports up to 384kHz)
    static constexpr int DELAY_BUFFER_SIZE = 8;
    double delayBuffer[DELAY_BUFFER_SIZE] = {0.0};
    int delayWriteIndex = 0;
    double cachedDelaySamples = 0.0;

    // Parameters
    double currentBiasStrength = 0.5;
    double currentInputGain = 1.0;
    bool isAmpexMode = true;
    double fs = 48000.0;

    // Tanh saturation
    double tanhDrive = 0.175;
    double tanhAsymmetry = 1.15;
    double tanhBias = 0.0;
    double tanhDcOffset = 0.0;
    double tanhNormFactor = 1.0;

    // Atan saturation (level-dependent, in series after tanh)
    double atanDrive = 4.0;
    double atanMixMax = 0.60;
    double atanThreshold = 2.5;
    double atanWidth = 3.0;
    double atanAsymmetry = 1.0;
    bool useAsymmetricAtan = false;
    double atanBias = 0.0;
    double atanDcOffset = 0.0;
    double atanNormFactor = 1.0;

    // J-A blend parameters
    double jaBlendMax = 0.70;
    double jaBlendThreshold = 1.0;
    double jaBlendWidth = 2.5;
    double jaEnvelope = 0.0;

    // DC blocking (4th-order Butterworth @ 5Hz)
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

    // AC Bias Shielding (parallel clean HF path)
    HFCut hfCut;
    double cleanHfBlend = 1.0;

    // Dispersive allpass (HF phase smear)
    struct AllpassFilter {
        double coefficient = 0.0;
        double z1 = 0.0;
        void setFrequency(double freq, double sampleRate) {
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
    double dispersiveCornerFreq = 10000.0;

    // Jiles-Atherton hysteresis
    JilesAthertonCore jaCore;
    double jaInputScale = 1.0;
    double jaOutputScale = 80.0;

    // Machine EQ
    MachineEQ machineEQ;

    void updateCachedValues();
    double asymmetricTanh(double x);
    double softAtan(double x);
    double asymmetricAtan(double x);
};

} // namespace TapeHysteresis

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
 * Architecture with global DC bias and level-dependent blending:
 *   1. Global Input Bias - Controls E/O ratio (physically accurate DC coupling)
 *   2. J-A Hysteresis - Physics-based magnetic domain model for tape compression
 *   3. Symmetric Atan - Smooth cubic saturation at higher levels
 *   4. Clean HF Path - Bypasses saturation (AC bias shielding)
 *
 * MASTER MODE (Ampex ATR-102):
 *   - THD: -12dB=0.005%, -6dB=0.02%, 0dB=0.08%, +6dB=0.40%
 *   - E/O ratio ~0.54 (odd-dominant), inputBias=0.06
 *
 * TRACKS MODE (Studer A820):
 *   - THD: -12dB=0.02%, -6dB=0.07%, 0dB=0.28%, +6dB=1.13%
 *   - E/O ratio ~1.17 (even-dominant), inputBias=0.22
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

    // Global input bias for E/O ratio (even harmonics)
    // Physically accurate: tape machines are DC-coupled
    double inputBias = 0.0;

    // Atan saturation (symmetric - bias applied globally)
    double atanDrive = 2.5;
    double atanMix = 0.0;          // Max blend amount
    double atanThreshold = 0.5;    // Engage around -6dB
    double atanWidth = 1.0;        // Transition width

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
    double softAtan(double x);
};

} // namespace TapeHysteresis

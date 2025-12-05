#include "HybridTapeProcessor.h"
#include <algorithm>

namespace TapeHysteresis
{

HybridTapeProcessor::HybridTapeProcessor()
{
    updateCachedValues();
    reset();
}

void HybridTapeProcessor::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    hfCut.setSampleRate(sampleRate);
    jaCore.setSampleRate(sampleRate);
    machineEQ.setSampleRate(sampleRate);

    // Configure dispersive allpass cascade for HF phase smear
    for (int i = 0; i < NUM_DISPERSIVE_STAGES; ++i) {
        double freq = dispersiveCornerFreq * std::pow(2.0, i * 0.5);
        dispersiveAllpass[i].setFrequency(freq, sampleRate);
    }

    // Design 4th-order Butterworth high-pass at 5 Hz for DC blocking
    double fc = 5.0;
    double w0 = 2.0 * M_PI * fc / sampleRate;
    double cosw0 = std::cos(w0);
    double sinw0 = std::sin(w0);
    double alpha = sinw0 / (2.0 * 0.7071);

    double b0 = (1.0 + cosw0) / 2.0;
    double b1 = -(1.0 + cosw0);
    double b2 = (1.0 + cosw0) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosw0;
    double a2 = 1.0 - alpha;

    dcBlocker1.b0 = b0 / a0;
    dcBlocker1.b1 = b1 / a0;
    dcBlocker1.b2 = b2 / a0;
    dcBlocker1.a1 = a1 / a0;
    dcBlocker1.a2 = a2 / a0;

    dcBlocker2.b0 = dcBlocker1.b0;
    dcBlocker2.b1 = dcBlocker1.b1;
    dcBlocker2.b2 = dcBlocker1.b2;
    dcBlocker2.a1 = dcBlocker1.a1;
    dcBlocker2.a2 = dcBlocker1.a2;
}

void HybridTapeProcessor::reset()
{
    dcBlocker1.reset();
    dcBlocker2.reset();
    hfCut.reset();
    jaCore.reset();
    machineEQ.reset();

    for (int i = 0; i < NUM_DISPERSIVE_STAGES; ++i) {
        dispersiveAllpass[i].reset();
    }

    for (int i = 0; i < DELAY_BUFFER_SIZE; ++i) {
        delayBuffer[i] = 0.0;
    }
    delayWriteIndex = 0;
    jaEnvelope = 0.0;
}

void HybridTapeProcessor::setParameters(double biasStrength, double inputGain)
{
    double clampedBias = std::clamp(biasStrength, 0.0, 1.0);
    bool newIsAmpexMode = (clampedBias < 0.74);

    // Only update if machine mode or input gain changed
    if (newIsAmpexMode != isAmpexMode || inputGain != currentInputGain)
    {
        currentBiasStrength = clampedBias;
        currentInputGain = inputGain;
        updateCachedValues();
    }
}

void HybridTapeProcessor::updateCachedValues()
{
    // Master (Ampex ATR-102): bias < 0.74
    // Tracks (Studer A820): bias >= 0.74
    isAmpexMode = (currentBiasStrength < 0.74);

    JilesAthertonCore::Parameters jaParams;

    if (isAmpexMode) {
        // AMPEX ATR-102 (MASTER MODE)
        // THD targets: -6dB=0.02%, 0dB=0.08%, +6dB=0.40%, MOL(3%)=+12dB
        // E/O ratio ~0.5 (odd-dominant)

        // === LAYER 1: J-A (hysteresis feel) ===
        jaParams.M_s = 1.0;
        jaParams.a = 50.0;
        jaParams.k = 0.005;
        jaParams.c = 0.96;
        jaParams.alpha = 2.0e-7;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 50.0;
        jaBlendMax = 0.0045;       // Very small - tune for -6dB ~0.02%
        jaBlendThreshold = 0.06;
        jaBlendWidth = 0.44;

        // === LAYER 2: Atan (symmetric now - bias is global) ===
        atanMix = 0.25;
        atanThreshold = 0.18;
        atanWidth = 2.2;
        atanDrive = 0.6;

        // Global input bias for E/O ~0.5 (odd-dominant)
        inputBias = 0.055;          // Small bias for Ampex

        // Hermite spline for cubic THD scaling
        // Equivalent to smoothstep when M0=M1=0
        hermiteP0 = 0.0;
        hermiteP1 = 1.0;
        hermiteM0 = 0.0;
        hermiteM1 = 0.0;

        dispersiveCornerFreq = 10000.0;
    } else {
        // STUDER A820 (TRACKS MODE)
        // THD targets: -6dB=0.07%, 0dB=0.25%, +6dB=1.25%, MOL(3%)=+9dB
        // E/O ratio ~1.12 (even-dominant)

        // === LAYER 1: J-A (hysteresis feel) ===
        jaParams.M_s = 1.0;
        jaParams.a = 45.0;
        jaParams.k = 0.008;
        jaParams.c = 0.92;
        jaParams.alpha = 5.0e-6;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 50.0;
        jaBlendMax = 0.0115;       // More than Ampex - tune for -6dB ~0.07%
        jaBlendThreshold = 0.032;
        jaBlendWidth = 0.468;

        // === LAYER 2: Atan (symmetric now - bias is global) ===
        atanMix = 0.35;
        atanThreshold = 0.20;
        atanWidth = 1.8;
        atanDrive = 0.95;

        // Global input bias for E/O ~1.12 (even-dominant)
        inputBias = 0.21;           // Larger bias for Studer's even-dominant character

        // Hermite spline for cubic THD scaling
        // Equivalent to smoothstep when M0=M1=0
        hermiteP0 = 0.0;
        hermiteP1 = 1.0;
        hermiteM0 = 0.0;
        hermiteM1 = 0.0;

        dispersiveCornerFreq = 2800.0;
    }

    // Azimuth delay: Ampex 8μs, Studer 12μs
    double delayMicroseconds = isAmpexMode ? 8.0 : 12.0;
    cachedDelaySamples = delayMicroseconds * 1e-6 * fs;

    // Reconfigure allpass filters
    for (int i = 0; i < NUM_DISPERSIVE_STAGES; ++i) {
        double freq = dispersiveCornerFreq * std::pow(2.0, i * 0.5);
        dispersiveAllpass[i].setFrequency(freq, fs);
    }

    // Update machine EQ
    machineEQ.setMachine(isAmpexMode ? MachineEQ::Machine::Ampex : MachineEQ::Machine::Studer);

    // Update AC bias shielding curve for selected machine
    hfCut.setMachineMode(isAmpexMode);
}

double HybridTapeProcessor::processSample(double input)
{
    double gained = input * currentInputGain;

    // Envelope follower for level-dependent blend
    double absGained = std::abs(gained);
    if (absGained > jaEnvelope) {
        jaEnvelope += 0.002 * (absGained - jaEnvelope);
    } else {
        jaEnvelope += 0.020 * (absGained - jaEnvelope);
    }

    // J-A blend - Hermite spline for precise THD curve control
    double jaBlend;
    if (jaBlendWidth > 0.0) {
        double blendRatio = std::clamp((jaEnvelope - jaBlendThreshold) / jaBlendWidth, 0.0, 1.0);
        jaBlend = jaBlendMax * hermiteBlend(blendRatio);
    } else {
        jaBlend = jaBlendMax;  // Constant blend when width = 0
    }

    // === PARALLEL PATH PROCESSING (AC Bias Shielding) ===
    // The high bias frequency linearizes HF recording, so HF bypasses saturation

    // Path 1: HFCut output goes to saturation (LF/mid content)
    double hfCutSignal = hfCut.processSample(gained);

    // Path 2: The "shielded" HF (what was cut) bypasses saturation entirely
    double cleanHF = gained - hfCutSignal;

    // === GLOBAL INPUT BIAS FOR EVEN HARMONICS ===
    // Apply asymmetric bias BEFORE all saturation stages
    // This makes both J-A and atan see an asymmetric signal
    double biasedSignal = hfCutSignal + inputBias;

    // === SATURATION ARCHITECTURE ===
    // Layer 1: J-A (hysteresis character, lower levels)
    // Layer 2: Atan (cubic character, higher levels)

    // 1. J-A for hysteresis feel - processes biased signal
    double jaPath = jaCore.process(biasedSignal * jaInputScale) * jaOutputScale;

    // 2. Atan for cubic character - processes biased signal (symmetric atan now)
    double atanOut = softAtan(biasedSignal);

    // Blend J-A into signal
    double mainPath = hfCutSignal * (1.0 - jaBlend) + jaPath * jaBlend;

    // Level-dependent atan blend (engages at higher levels where J-A drops off)
    // Uses Hermite spline for consistent THD curve shaping
    double atanBlendRatio = std::clamp((jaEnvelope - atanThreshold) / atanWidth, 0.0, 1.0);
    double atanBlend = atanMix * hermiteBlend(atanBlendRatio);
    double saturatedPath = mainPath * (1.0 - atanBlend) + atanOut * atanBlend;

    // === COMBINE PATHS ===
    // Sum saturated signal (with HF removed) + clean HF (bypassed saturation)
    // cleanHfBlend controls how much of the shielded HF is clean vs saturated
    double output = saturatedPath + cleanHF * cleanHfBlend;

    // Machine-specific EQ (always on)
    output = machineEQ.processSample(output);

    // HF dispersive allpass (tape head phase smear)
    for (int i = 0; i < NUM_DISPERSIVE_STAGES; ++i) {
        output = dispersiveAllpass[i].process(output);
    }

    // DC blocking
    output = dcBlocker1.process(output);
    output = dcBlocker2.process(output);

    return output;
}

double HybridTapeProcessor::softAtan(double x)
{
    if (atanDrive < 0.001) return x;
    return std::atan(atanDrive * x) / atanDrive;
}

double HybridTapeProcessor::hermiteBlend(double t)
{
    // Hermite spline interpolation for THD curve shaping
    // Allows fine control over the saturation blend curve slope
    // For cubic behavior (2x THD per 3dB), we need specific tangent values
    double t2 = t * t;
    double t3 = t2 * t;

    // Hermite basis functions
    double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;  // (1 - t)^2 * (1 + 2t)
    double h10 = t3 - 2.0 * t2 + t;           // t * (1 - t)^2
    double h01 = -2.0 * t3 + 3.0 * t2;        // t^2 * (3 - 2t)
    double h11 = t3 - t2;                      // t^2 * (t - 1)

    return h00 * hermiteP0 + h10 * hermiteM0 + h01 * hermiteP1 + h11 * hermiteM1;
}

double HybridTapeProcessor::processRightChannel(double input)
{
    double processed = processSample(input);

    // Azimuth delay
    delayBuffer[delayWriteIndex] = processed;

    double readPos = static_cast<double>(delayWriteIndex) - cachedDelaySamples;
    if (readPos < 0.0) readPos += DELAY_BUFFER_SIZE;

    int readIndex0 = static_cast<int>(readPos);
    int readIndex1 = (readIndex0 + 1) % DELAY_BUFFER_SIZE;
    double frac = readPos - static_cast<double>(readIndex0);

    double delayed = delayBuffer[readIndex0] * (1.0 - frac) + delayBuffer[readIndex1] * frac;
    delayWriteIndex = (delayWriteIndex + 1) % DELAY_BUFFER_SIZE;

    return delayed;
}

} // namespace TapeHysteresis

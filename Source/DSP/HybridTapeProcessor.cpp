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
    hfRestore.setSampleRate(sampleRate);
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
    hfRestore.reset();
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
        // Target: MOL @ +12dB (3% THD), E/O ratio 0.503 (odd-dominant)
        jaParams.M_s = 1.0;
        jaParams.a = 50.0;
        jaParams.k = 0.005;
        jaParams.c = 0.95;
        jaParams.alpha = 1.0e-6;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 150.0;

        // Tuned for MOL @ +12dB with E/O ~0.5
        // Strategy: tanh for level + atan steepening for curve shape
        tanhDrive = 0.20;          // Target 3% THD at +12dB
        tanhAsymmetry = 1.12;      // Reduced for E/O closer to 0.5 at operating levels
        tanhBias = tanhAsymmetry - 1.0;
        tanhDcOffset = std::tanh(tanhDrive * tanhBias);
        double tanhNorm = tanhDrive * (1.0 - tanhDcOffset * tanhDcOffset);
        tanhNormFactor = (tanhNorm > 0.001) ? (1.0 / tanhNorm) : 1.0;

        jaBlendMax = 1.00;         // Full J-A blend
        jaBlendThreshold = 0.50;   // Lower to engage J-A earlier
        jaBlendWidth = 2.0;        // Wider blend for smoother transition

        atanDrive = 6.5;           // Increased for steeper curve at high levels
        atanMixMax = 0.75;         // More atan contribution
        atanThreshold = 0.35;      // Engage earlier for cubic shape
        atanWidth = 2.0;           // Tighter blend
        atanAsymmetry = 1.0;
        useAsymmetricAtan = false;

        // ATR-102: 0.25μm ceramic head gap = negligible gap-induced phase smear
        // Transformerless config - minimal electronics contribution
        // Only EQ circuits contribute - very subtle, high-frequency-only smear
        dispersiveCornerFreq = 10000.0;
    } else {
        // STUDER A820 (TRACKS MODE)
        // Target: MOL @ +9dB (3% THD), E/O ratio 1.122 (even-dominant)
        jaParams.M_s = 1.0;
        jaParams.a = 35.0;
        jaParams.k = 0.01;
        jaParams.c = 0.92;
        jaParams.alpha = 1.0e-5;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 105.0;

        // Tuned for MOL @ +9dB with E/O ~1.12
        // Strategy: tanhAsymmetry for E/O, atan for curve steepening
        tanhDrive = 0.12;          // Fine-tuned for MOL @ +9dB
        tanhAsymmetry = 1.38;      // Tuned for E/O = 1.122
        tanhBias = tanhAsymmetry - 1.0;
        tanhDcOffset = std::tanh(tanhDrive * tanhBias);
        double tanhNorm = tanhDrive * (1.0 - tanhDcOffset * tanhDcOffset);
        tanhNormFactor = (tanhNorm > 0.001) ? (1.0 / tanhNorm) : 1.0;

        jaBlendMax = 1.00;         // Full J-A blend
        jaBlendThreshold = 0.45;   // Engage J-A earlier for smoother curve
        jaBlendWidth = 2.5;        // Wider blend for gradual transition

        atanDrive = 5.5;           // Moderate curve steepening
        atanMixMax = 0.72;         // Moderate atan contribution
        atanThreshold = 0.35;      // Earlier engagement for smoother curve
        atanWidth = 2.5;           // Wider blend for smoother transition
        atanAsymmetry = 1.42;      // For Studer's even-harmonic character (E/O 1.122)
        useAsymmetricAtan = true;
        atanBias = atanAsymmetry - 1.0;
        atanDcOffset = std::atan(atanDrive * atanBias);
        double driveBias = atanDrive * atanBias;
        double atanNorm = atanDrive / (1.0 + driveBias * driveBias);
        atanNormFactor = (atanNorm > 0.001) ? (1.0 / atanNorm) : 1.0;

        // A820: 3μm head gap = phase smear onset ~25kHz from head + electronics
        // More pronounced smear starting lower in frequency
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

    // Update AC bias shielding curves for selected machine
    hfRestore.setMachineMode(isAmpexMode);
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

    // Level-dependent J-A blend with cubic smoothstep
    double blendRatio = std::clamp((jaEnvelope - jaBlendThreshold) / jaBlendWidth, 0.0, 1.0);
    double jaBlend = jaBlendMax * blendRatio * blendRatio * (3.0 - 2.0 * blendRatio);

    // AC bias shielding (cut highs before saturation)
    double hfCutSignal = hfCut.processSample(gained);

    // J-A path (physics-based hysteresis)
    double jaPath = jaCore.process(hfCutSignal * jaInputScale) * jaOutputScale;

    // Tanh path (asymmetric saturation)
    double tanhOut = asymmetricTanh(hfCutSignal);

    // Level-dependent atan in series
    double atanBlendRatio = std::clamp((jaEnvelope - atanThreshold) / atanWidth, 0.0, 1.0);
    double atanAmount = atanMixMax * atanBlendRatio * atanBlendRatio * (3.0 - 2.0 * atanBlendRatio);
    double atanOut = useAsymmetricAtan ? asymmetricAtan(tanhOut) : softAtan(tanhOut);
    double tanhPath = tanhOut * (1.0 - atanAmount) + atanOut * atanAmount;

    // Parallel blend
    double blended = jaPath * jaBlend + tanhPath * (1.0 - jaBlend);

    // HF restore (restore highs after saturation)
    double output = hfRestore.processSample(blended);

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

double HybridTapeProcessor::asymmetricTanh(double x)
{
    double biased = x + tanhBias;
    double saturated = std::tanh(tanhDrive * biased);
    return (saturated - tanhDcOffset) * tanhNormFactor;
}

double HybridTapeProcessor::softAtan(double x)
{
    if (atanDrive < 0.001) return x;
    return std::atan(atanDrive * x) / atanDrive;
}

double HybridTapeProcessor::asymmetricAtan(double x)
{
    if (atanDrive < 0.001) return x;
    double biased = x + atanBias;
    double saturated = std::atan(atanDrive * biased);
    return (saturated - atanDcOffset) * atanNormFactor;
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

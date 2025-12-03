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

    // Note: PluginProcessor passes us the OVERSAMPLED rate (2x actual sample rate)
    // So we just use it directly - no internal oversampling needed
    reEmphasis.setSampleRate(sampleRate);
    deEmphasis.setSampleRate(sampleRate);
    jaCore.setSampleRate(sampleRate);

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
    reEmphasis.reset();
    deEmphasis.reset();
    jaCore.reset();

    // Clear azimuth delay buffer
    for (int i = 0; i < DELAY_BUFFER_SIZE; ++i) {
        delayBuffer[i] = 0.0;
    }
    delayWriteIndex = 0;

    // Reset J-A blend envelope follower
    jaEnvelope = 0.0;
}

void HybridTapeProcessor::setParameters(double biasStrength, double inputGain)
{
    currentBiasStrength = std::clamp(biasStrength, 0.0, 1.0);
    currentInputGain = inputGain;

    updateCachedValues();
}

void HybridTapeProcessor::updateCachedValues()
{
    // Determine machine mode
    // Master (Ampex ATR-102): bias < 0.74
    // Tracks (Studer A820): bias >= 0.74
    isAmpexMode = (currentBiasStrength < 0.74);

    // =========================================================================
    // HYBRID SATURATION: Asymmetric Tanh + J-A Hysteresis
    // =========================================================================
    //
    // The saturation model combines two complementary approaches:
    //
    // 1. ASYMMETRIC TANH: Provides the THD vs level curve
    //    - Steep saturation curve matching tape characteristics
    //    - Asymmetry parameter controls even/odd harmonic balance
    //    - Drive parameter controls where saturation begins
    //
    // 2. JILES-ATHERTON: Adds subtle hysteresis character
    //    - History-dependent behavior (tape "remembers")
    //    - Slight phase shift and timing effects
    //    - Very subtle to avoid masking the tanh saturation
    //
    // THD Targets:
    // -------------------------------------------------------------------------
    // Level      | Ampex ATR-102 (Master) | Studer A820 (Tracks)
    // -------------------------------------------------------------------------
    // -6 dB      | ~0.02%                 | 0.06-0.08%
    // 0 dB (0VU) | 0.07-0.09%             | 0.2-0.3%
    // +3 dB      | 0.15-0.20%             | 0.5-0.7%
    // +6 dB      | 0.35-0.45%             | 1.0-1.5%
    // +9 dB      | 0.8-1.2%               | 2.5-3.0%
    // MOL (3%)   | ~+12 dB                | ~+9 dB
    // -------------------------------------------------------------------------
    // Asymmetry  | 0.503 (odd-dominant)   | 1.122 (even-dominant)
    // -------------------------------------------------------------------------

    if (isAmpexMode) {
        // =====================================================================
        // AMPEX ATR-102 (MASTER MODE)
        // =====================================================================
        // Ultra-linear 1" mastering machine
        // Known for: Transparency, extended headroom, clean saturation
        // Character: Subtle warmth without obvious coloration
        //
        // J-A: Very subtle hysteresis - clean with slight "air"
        // Tanh: Low drive for extended headroom, slight asymmetry for E/O ≈ 0.5

        // =====================================================================
        // LAYER 1: TANH ONLY - Establish baseline THD curve
        // =====================================================================
        // Disable J-A and HLS to isolate tanh behavior
        // Goal: Get as close to targets as possible with tanh alone

        // J-A disabled (blend max = 0)
        JilesAthertonCore::Parameters jaParams;
        jaParams.M_s = 1.0;
        jaParams.a = 50.0;
        jaParams.k = 0.005;
        jaParams.c = 0.95;
        jaParams.alpha = 1.0e-6;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 150.0;

        // J-A blend disabled
        jaBlendMax = 0.0;        // DISABLED - tanh only
        jaBlendThreshold = 0.5;
        jaBlendWidth = 0.5;

        // =====================================================================
        // LAYER 2: TANH + J-A (slowly introduce hysteresis)
        // =====================================================================
        // Layer 1 baseline: tanhDrive=0.11 hits all 7 targets
        // Now slowly introduce J-A while keeping THD in range

        // Tanh - sets low-level THD (-12dB, -6dB targets)
        // Asymmetry adds THD, so reduce drive to compensate
        tanhDrive = 0.095;       // Tuned for low-level targets
        // Asymmetry controls E/O ratio (target 0.503 for Ampex - odd-dominant)
        tanhAsymmetry = 1.08;    // Tuned for E/O ~0.503

        // J-A - 100% blend at high levels
        // Threshold 0.75 ≈ -2.5dB
        jaBlendMax = 1.00;       // 100% J-A at high levels
        jaBlendThreshold = 0.77; // Start blending at ~-2.3dB
        jaBlendWidth = 1.5;      // Full blend reached around +3.5dB

        // Atan kicks in gradually to steepen the knee for upper targets
        // Tuned to hit ~0.9% at +9dB
        atanDrive = 5.0;         // High drive for harmonics
        atanMixMax = 0.65;       // 65% max blend
        atanThreshold = 0.5;     // ~-6dB - start blending
        atanWidth = 2.5;         // Reach full blend around +2dB
        atanAsymmetry = 1.0;     // Symmetric (odd harmonics for Ampex)
        useAsymmetricAtan = false;

    } else {
        // =====================================================================
        // STUDER A820 (TRACKS MODE)
        // =====================================================================
        // 24-track workhorse machine
        // Known for: Warmth, punch, musical saturation
        // Character: Rich harmonics, earlier saturation onset, "fat" sound
        //
        // J-A: Subtle hysteresis for "sticky" bass and punch
        // Tanh: Higher drive for earlier MOL, asymmetry for E/O ≈ 1.1

        // =====================================================================
        // LAYER 1: TANH ONLY - Establish baseline THD curve
        // =====================================================================
        // Disable J-A and HLS to isolate tanh behavior

        // J-A disabled (blend max = 0)
        JilesAthertonCore::Parameters jaParams;
        jaParams.M_s = 1.0;
        jaParams.a = 35.0;
        jaParams.k = 0.01;
        jaParams.c = 0.92;
        jaParams.alpha = 1.0e-5;
        jaCore.setParameters(jaParams);
        jaInputScale = 1.0;
        jaOutputScale = 105.0;

        // J-A blend disabled
        jaBlendMax = 0.0;        // DISABLED - tanh only
        jaBlendThreshold = 0.2;
        jaBlendWidth = 0.3;

        // =====================================================================
        // LAYER 2: TANH + J-A (slowly introduce hysteresis)
        // =====================================================================
        // Layer 1 baseline: tanhDrive=0.21 hits all 7 targets
        // Now slowly introduce J-A while keeping THD in range

        // Tanh - sets low-level THD (-12dB, -6dB targets)
        // Need balance: lower drive for THD, higher asymmetry for E/O
        tanhDrive = 0.14;        // Balanced for all THD targets
        // Asymmetry controls E/O ratio (target 1.122 for Studer - even-dominant)
        tanhAsymmetry = 1.18;    // Higher asymmetry for E/O ~1.1

        // J-A - 100% blend at high levels
        // Lower threshold than Ampex so J-A kicks in earlier for more mid-level THD
        jaBlendMax = 1.00;       // 100% J-A at high levels
        jaBlendThreshold = 0.60; // Start blending earlier at ~-4.4dB
        jaBlendWidth = 1.2;      // Faster crossfade

        // Asymmetric atan for Studer - preserves E/O ratio while adding high-level THD
        atanDrive = 5.5;         // Slightly higher drive to hit +9dB target
        atanMixMax = 0.72;       // 72% max blend at high levels
        atanThreshold = 0.4;     // ~-8dB - start blending earlier
        atanWidth = 2.5;         // Full blend around +2dB
        atanAsymmetry = 1.25;    // Asymmetric (even harmonics for Studer E/O ~1.1)
        useAsymmetricAtan = true;
    }

    // Calculate azimuth delay in samples
    // Master/Ampex: 8 microseconds
    // Tracks/Studer: 12 microseconds
    double delayMicroseconds = isAmpexMode ? 8.0 : 12.0;
    cachedDelaySamples = delayMicroseconds * 1e-6 * fs;
}

double HybridTapeProcessor::processSample(double input)
{
    // =========================================================================
    // LEVEL-DEPENDENT HYBRID SATURATION
    // =========================================================================
    //
    // TANH PATH: Always active, carries the main THD curve
    //   - Frequency-shaped saturation (bass > treble via CCIR emphasis)
    //
    // J-A PATH: Level-dependent blend
    //   - Silent at low levels (clean response, no THD contribution)
    //   - Fades in at high levels for magnetic hysteresis character
    //   - When J-A comes in, tanh is ducked slightly to maintain unity gain
    //
    // HLS (ATAN): Simple final limiter
    //   - Fixed drive, just prevents harsh clipping at extreme levels
    //
    // =========================================================================

    // Step 1: Apply input gain
    double gained = input * currentInputGain;

    // Step 2: Update envelope follower for level-dependent blend
    double absGained = std::abs(gained);
    const double envAttack = 0.002;   // Fast attack
    const double envRelease = 0.020;  // Moderate release
    if (absGained > jaEnvelope) {
        jaEnvelope += envAttack * (absGained - jaEnvelope);
    } else {
        jaEnvelope += envRelease * (absGained - jaEnvelope);
    }

    // Step 3: Calculate level-dependent J-A blend
    // Below threshold: jaBlend = 0 (all tanh)
    // Above threshold + width: jaBlend = jaBlendMax
    double blendRatio = (jaEnvelope - jaBlendThreshold) / jaBlendWidth;
    blendRatio = std::clamp(blendRatio, 0.0, 1.0);
    // Smooth cubic curve for natural crossfade
    double jaBlend = jaBlendMax * blendRatio * blendRatio * (3.0 - 2.0 * blendRatio);

    // Step 4: De-emphasis (cut highs before saturation)
    double deEmphasized = deEmphasis.processSample(gained);

    // Step 5: J-A PATH (physics-based hysteresis)
    double jaInput = deEmphasized * jaInputScale;
    double jaOutput = jaCore.process(jaInput);
    double jaPath = jaOutput * jaOutputScale;

    // Step 6: TANH PATH (asymmetric saturation + level-dependent series atan)
    double tanhOut = asymmetricTanh(deEmphasized);

    // Atan applied IN SERIES after tanh - adds extra harmonics at high levels
    // Level-dependent: atan effect fades in at high levels
    double atanBlendRatio = (jaEnvelope - atanThreshold) / atanWidth;
    atanBlendRatio = std::clamp(atanBlendRatio, 0.0, 1.0);
    double atanAmount = atanMixMax * atanBlendRatio * atanBlendRatio * (3.0 - 2.0 * atanBlendRatio);

    // Series atan: tanhOut passes through atan, then blend with original
    // Use symmetric atan for Ampex (odd harmonics), asymmetric for Studer (even harmonics)
    double atanOut = useAsymmetricAtan ? asymmetricAtan(tanhOut) : softAtan(tanhOut);
    double tanhPath = tanhOut * (1.0 - atanAmount) + atanOut * atanAmount;

    // Step 7: Level-dependent parallel blend
    double blended = jaPath * jaBlend + tanhPath * (1.0 - jaBlend);

    // Step 8: Re-emphasis (restore highs after saturation)
    double output = reEmphasis.processSample(blended);

    // Step 9: DC blocking filter (4th-order Butterworth @ 5Hz)
    output = dcBlocker1.process(output);
    output = dcBlocker2.process(output);

    return output;
}

double HybridTapeProcessor::asymmetricTanh(double x)
{
    // Asymmetric tanh saturation with DC-bias approach
    //
    // Instead of different scaling for positive/negative, we add a small DC bias
    // before saturation, then remove it after. This creates even harmonics without
    // DC offset in the output.
    //
    // bias > 0: positive peaks hit saturation sooner → more even harmonics
    // bias = 0: symmetric → pure odd harmonics
    //
    // E/O ratio is approximately proportional to bias * drive

    // The bias is expressed in terms of input amplitude
    // asymmetry = 1.0 → bias = 0 (symmetric)
    // asymmetry = 1.1 → bias = +0.1 (positive peaks clip sooner)

    double bias = (tanhAsymmetry - 1.0);  // How much to bias

    // Apply DC bias
    double biased = x + bias;

    // Saturate with tanh
    double saturated = std::tanh(tanhDrive * biased);

    // Remove the DC component introduced by the bias
    double dcOffset = std::tanh(tanhDrive * bias);
    double output = saturated - dcOffset;

    // Normalize for approximately unity gain at low levels
    // The derivative of (tanh(drive*(x+bias)) - tanh(drive*bias)) at x=0
    // is drive * sech²(drive*bias) = drive * (1 - tanh²(drive*bias))
    double normFactor = tanhDrive * (1.0 - dcOffset * dcOffset);
    if (normFactor > 0.001) {
        output /= normFactor;
    }

    return output;
}

double HybridTapeProcessor::softAtan(double x)
{
    // Normalized atan saturation (symmetric - odd harmonics only)
    // Used for Ampex mode
    //
    // At low levels: atan(drive*x) ≈ drive*x, so output ≈ x (unity gain)
    // At high levels: atan approaches ±pi/2, giving soft limiting

    // Guard against zero drive (bypass)
    if (atanDrive < 0.001) {
        return x;
    }

    double driven = atanDrive * x;
    double saturated = std::atan(driven);

    // Normalize for unity gain at low levels
    double output = saturated / atanDrive;

    return output;
}

double HybridTapeProcessor::asymmetricAtan(double x)
{
    // Asymmetric atan saturation (generates even harmonics)
    // Used for Studer mode to preserve E/O ratio
    //
    // Same DC-bias approach as asymmetricTanh:
    // Add bias before saturation, remove DC after
    // This creates even harmonics while maintaining DC-free output

    // Guard against zero drive (bypass)
    if (atanDrive < 0.001) {
        return x;
    }

    double bias = (atanAsymmetry - 1.0);

    // Apply DC bias and saturate
    double biased = x + bias;
    double driven = atanDrive * biased;
    double saturated = std::atan(driven);

    // Remove DC component
    double dcOffset = std::atan(atanDrive * bias);
    double output = saturated - dcOffset;

    // Normalize for unity gain at low levels
    // Derivative of atan(drive*(x+bias)) at x=0 is drive / (1 + (drive*bias)²)
    double driveBias = atanDrive * bias;
    double normFactor = atanDrive / (1.0 + driveBias * driveBias);
    if (normFactor > 0.001) {
        output /= normFactor;
    }

    return output;
}

double HybridTapeProcessor::processRightChannel(double input)
{
    // Process through normal saturation path
    double processed = processSample(input);

    // Apply azimuth delay for stereo imaging
    // Master/Ampex: 8μs, Tracks/Studer: 12μs
    delayBuffer[delayWriteIndex] = processed;

    double readPos = static_cast<double>(delayWriteIndex) - cachedDelaySamples;
    if (readPos < 0.0) {
        readPos += DELAY_BUFFER_SIZE;
    }

    int readIndex0 = static_cast<int>(readPos);
    int readIndex1 = (readIndex0 + 1) % DELAY_BUFFER_SIZE;
    double frac = readPos - static_cast<double>(readIndex0);

    double delayed = delayBuffer[readIndex0] * (1.0 - frac) + delayBuffer[readIndex1] * frac;

    delayWriteIndex = (delayWriteIndex + 1) % DELAY_BUFFER_SIZE;

    return delayed;
}

} // namespace TapeHysteresis

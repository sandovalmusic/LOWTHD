/**
 * Test_PhaseCoherence.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Phase Coherence Validation
 *
 * CRITICAL TEST: Detects phase cancellation issues in parallel signal paths
 *
 * The processor uses parallel J-A and Tanh paths that are blended together.
 * If these paths have phase differences, they can cause:
 * 1. Frequency-dependent cancellation (comb filtering)
 * 2. Gain loss at certain frequencies
 * 3. Hollow or thin sound character
 *
 * This test validates:
 * 1. Phase response across frequency range
 * 2. No destructive cancellation between paths
 * 3. Group delay consistency
 * 4. Impulse response integrity
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;
constexpr int NUM_CYCLES = 100;
constexpr int SKIP_CYCLES = 20;

struct PhaseAnalysis {
    double magnitude;  // Output magnitude relative to input
    double phase;      // Phase in degrees
    double gainDb;     // Gain in dB
};

PhaseAnalysis measurePhase(TapeHysteresis::HybridTapeProcessor& processor,
                           double amplitude,
                           double frequency) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = NUM_CYCLES * samplesPerCycle;
    int skipSamples = SKIP_CYCLES * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    // Measure fundamental via DFT
    double real = 0.0, imag = 0.0;
    for (int i = skipSamples; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        real += output[i] * std::cos(phase);
        imag += output[i] * std::sin(phase);
    }

    real /= analysisSamples;
    imag /= analysisSamples;

    PhaseAnalysis result;
    result.magnitude = 2.0 * std::sqrt(real * real + imag * imag);
    result.phase = std::atan2(-imag, real) * 180.0 / M_PI;  // Sine reference
    result.gainDb = 20.0 * std::log10(result.magnitude / amplitude);

    return result;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   LOW THD TAPE SIMULATOR v1.0 - PHASE COHERENCE TEST     ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    double frequencies[] = { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 };
    int numFreqs = 8;

    // =========================================================================
    // TEST 1: GAIN CONSISTENCY ACROSS FREQUENCY
    // =========================================================================
    // At a fixed level, gain should be reasonably consistent across frequencies
    // Some variation is expected due to saturation and de-emphasis/re-emphasis

    std::cout << "\n=== GAIN CONSISTENCY TEST (0dB input) ===\n";
    std::cout << "Expected: Gain variation < 2.0dB across 50Hz-10kHz\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = 1.0;  // 0dB

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)    Gain(dB)    Phase(deg)\n";
        std::cout << "------------------------------------\n";

        double minGain = 100.0, maxGain = -100.0;
        for (int i = 0; i < numFreqs; i++) {
            auto result = measurePhase(processor, amplitude, frequencies[i]);
            minGain = std::min(minGain, result.gainDb);
            maxGain = std::max(maxGain, result.gainDb);

            std::cout << std::setw(7) << frequencies[i] << "     "
                      << std::fixed << std::setprecision(2) << std::showpos
                      << std::setw(6) << result.gainDb << "      "
                      << std::setw(7) << result.phase << "\n";
        }

        double variation = maxGain - minGain;
        bool consistent = (variation < 2.0);
        std::cout << std::noshowpos << "Gain variation: " << variation << " dB  "
                  << (consistent ? "PASS" : "FAIL") << "\n\n";

        if (!consistent) allPassed = false;
    }

    // =========================================================================
    // TEST 2: LOW-LEVEL PHASE LINEARITY
    // =========================================================================
    // At low levels (where J-A blend is minimal), phase should be smooth

    std::cout << "\n=== LOW-LEVEL PHASE LINEARITY (-20dB input) ===\n";
    std::cout << "Expected: No sudden phase jumps between adjacent frequencies\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = std::pow(10.0, -20.0 / 20.0);

        std::cout << modeName << ": ";

        double prevPhase = 0.0;
        bool smooth = true;
        int jumps = 0;

        for (int i = 0; i < numFreqs; i++) {
            auto result = measurePhase(processor, amplitude, frequencies[i]);

            if (i > 0) {
                double phaseDiff = std::abs(result.phase - prevPhase);
                // Account for phase wrapping
                if (phaseDiff > 180.0) phaseDiff = 360.0 - phaseDiff;

                // Phase shouldn't jump more than 45 degrees between octaves
                if (phaseDiff > 45.0) {
                    jumps++;
                }
            }
            prevPhase = result.phase;
        }

        smooth = (jumps == 0);
        std::cout << (smooth ? "PASS (smooth phase)" : "FAIL (phase jumps detected)") << "\n";

        if (!smooth) allPassed = false;
    }

    // =========================================================================
    // TEST 3: IMPULSE RESPONSE INTEGRITY
    // =========================================================================
    // Impulse response should be clean with no extended ringing
    // Note: 5Hz DC blocker takes longer to settle

    std::cout << "\n=== IMPULSE RESPONSE TEST ===\n";
    std::cout << "Expected: Impulse settles to < 1e-3 within 2000 samples\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        // Send impulse
        double peakResponse = std::abs(processor.processSample(0.5));

        // Check decay (allow time for 5Hz HPF)
        double maxAfter = 0.0;
        for (int i = 1; i < 5000; ++i) {
            double out = processor.processSample(0.0);
            if (i >= 2000) {
                maxAfter = std::max(maxAfter, std::abs(out));
            }
        }

        bool settled = (maxAfter < 1e-3);
        std::cout << modeName << ": Peak=" << std::fixed << std::setprecision(4) << peakResponse
                  << ", After 2000 samples=" << std::scientific << maxAfter
                  << "  " << std::fixed << (settled ? "PASS" : "FAIL") << "\n";

        if (!settled) allPassed = false;
    }

    // =========================================================================
    // TEST 4: PARALLEL PATH CANCELLATION CHECK
    // =========================================================================
    // Compare gain at low level (mostly tanh path) vs high level (blended paths)
    // If paths are out of phase, high-level gain will be unexpectedly LOW

    std::cout << "\n=== PARALLEL PATH CANCELLATION CHECK ===\n";
    std::cout << "Expected: Gain at high levels >= gain at low levels (no cancellation)\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)   Gain@-20dB   Gain@+3dB    Diff     Status\n";
        std::cout << "------------------------------------------------------\n";

        bool noCancel = true;
        for (int i = 0; i < numFreqs; i++) {
            double ampLow = std::pow(10.0, -20.0 / 20.0);
            double ampHigh = std::pow(10.0, 3.0 / 20.0);

            auto resultLow = measurePhase(processor, ampLow, frequencies[i]);
            auto resultHigh = measurePhase(processor, ampHigh, frequencies[i]);

            // High level gain should be similar or higher (due to compression)
            // If it's much LOWER, that indicates cancellation
            double diff = resultHigh.gainDb - resultLow.gainDb;

            // At high levels we expect some compression, so gain may drop 1-2dB
            // But if it drops more than 3dB, that's suspicious
            bool ok = (diff > -3.0);
            if (!ok) noCancel = false;

            std::cout << std::setw(7) << frequencies[i] << "    "
                      << std::fixed << std::setprecision(2) << std::showpos
                      << std::setw(7) << resultLow.gainDb << "      "
                      << std::setw(7) << resultHigh.gainDb << "     "
                      << std::setw(6) << diff << "     "
                      << std::noshowpos << (ok ? "OK" : "CANCEL?") << "\n";
        }

        std::cout << "Result: " << (noCancel ? "PASS" : "FAIL - possible phase cancellation") << "\n\n";
        if (!noCancel) allPassed = false;
    }

    // =========================================================================
    // TEST 5: FREQUENCY SWEEP SMOOTHNESS
    // =========================================================================
    // Sweep through frequencies and check for comb filter notches

    std::cout << "\n=== FREQUENCY SWEEP SMOOTHNESS ===\n";
    std::cout << "Expected: No notches (> 3dB dips) in frequency response\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = 0.5;  // -6dB

        // Sweep from 100Hz to 10kHz in 1/6 octave steps
        double prevGain = 0.0;
        bool hasNotch = false;
        int notchCount = 0;

        std::cout << modeName << ": ";

        for (double freq = 100.0; freq <= 10000.0; freq *= std::pow(2.0, 1.0/6.0)) {
            auto result = measurePhase(processor, amplitude, freq);

            if (freq > 100.0) {
                double drop = prevGain - result.gainDb;
                if (drop > 3.0) {
                    hasNotch = true;
                    notchCount++;
                }
            }
            prevGain = result.gainDb;
        }

        std::cout << (hasNotch ? "FAIL (" + std::to_string(notchCount) + " notches detected)"
                               : "PASS (smooth response)") << "\n";
        if (hasNotch) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "PHASE COHERENCE TEST: ALL PASSED\n";
    } else {
        std::cout << "PHASE COHERENCE TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

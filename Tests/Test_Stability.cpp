/**
 * Test_Stability.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Stability and Edge Case Validation
 *
 * Validates processor stability under extreme conditions:
 * 1. DC blocking effectiveness
 * 2. Numerical stability (no NaN, Inf)
 * 3. Extreme input handling
 * 4. Sample rate changes
 * 5. Parameter changes during processing
 * 6. Long-term stability (no drift)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <limits>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;

bool isValidSample(double sample) {
    return std::isfinite(sample) && !std::isnan(sample);
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║      LOW THD TAPE SIMULATOR v1.0 - STABILITY TEST        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // =========================================================================
    // TEST 1: DC BLOCKING
    // =========================================================================
    // DC input should result in zero (or near-zero) DC output

    std::cout << "\n=== DC BLOCKING TEST ===\n";
    std::cout << "Expected: DC input produces < 0.001 DC output after settling\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        // Apply DC input for 1 second
        double dcInput = 0.5;
        double lastOutput = 0.0;

        for (int i = 0; i < static_cast<int>(SAMPLE_RATE); ++i) {
            lastOutput = processor.processSample(dcInput);
        }

        // After settling, output should be near zero
        bool dcBlocked = std::abs(lastOutput) < 0.001;

        std::cout << modeName << ": DC output = " << std::scientific << std::setprecision(3)
                  << lastOutput << "  " << std::fixed << (dcBlocked ? "PASS" : "FAIL") << "\n";

        if (!dcBlocked) allPassed = false;
    }

    // =========================================================================
    // TEST 2: NUMERICAL STABILITY - EXTREME INPUTS
    // =========================================================================
    // Processor should handle extreme inputs without producing NaN or Inf

    std::cout << "\n=== EXTREME INPUT TEST ===\n";
    std::cout << "Expected: No NaN or Inf outputs for extreme inputs\n\n";

    double extremeInputs[] = { 0.0, 1e-100, 1e-10, 10.0, 100.0, 1000.0 };
    const char* inputNames[] = { "Zero", "Tiny (1e-100)", "Small (1e-10)",
                                  "Large (10)", "Very Large (100)", "Extreme (1000)" };
    int numExtremes = 6;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        std::cout << modeName << ":\n";

        int passed = 0;
        for (int e = 0; e < numExtremes; e++) {
            TapeHysteresis::HybridTapeProcessor processor;
            processor.setSampleRate(SAMPLE_RATE);
            processor.setParameters(bias, 1.0);

            bool valid = true;
            for (int i = 0; i < 1000; ++i) {
                double phase = 2.0 * M_PI * 1000.0 * i / SAMPLE_RATE;
                double input = extremeInputs[e] * std::sin(phase);
                double output = processor.processSample(input);

                if (!isValidSample(output)) {
                    valid = false;
                    break;
                }
            }

            if (valid) passed++;
            std::cout << "  " << std::setw(20) << inputNames[e] << ": "
                      << (valid ? "PASS" : "FAIL (NaN/Inf)") << "\n";
        }

        std::cout << "  Result: " << passed << "/" << numExtremes << "\n\n";
        if (passed != numExtremes) allPassed = false;
    }

    // =========================================================================
    // TEST 3: IMPULSE STABILITY
    // =========================================================================
    // Single impulse should decay, not cause instability
    // Note: 5Hz DC blocker has ~200ms time constant, so settling takes time

    std::cout << "\n=== IMPULSE STABILITY TEST ===\n";
    std::cout << "Expected: Impulse decays to < 1e-4 within 50000 samples\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        // Apply impulse
        processor.processSample(1.0);

        // Track decay (allow longer for 5Hz high-pass to settle)
        double maxAfter = 0.0;
        bool stable = true;

        for (int i = 1; i < 60000; ++i) {
            double output = processor.processSample(0.0);

            if (!isValidSample(output)) {
                stable = false;
                break;
            }

            if (i > 50000) {
                maxAfter = std::max(maxAfter, std::abs(output));
            }
        }

        // 1e-4 threshold is appropriate for 5Hz HPF with ~0.5 second settling
        stable = stable && (maxAfter < 1e-4);

        std::cout << modeName << ": " << (stable ? "PASS" : "FAIL")
                  << " (residual: " << std::scientific << maxAfter << ")\n";

        if (!stable) allPassed = false;
    }

    // =========================================================================
    // TEST 4: PARAMETER CHANGE STABILITY
    // =========================================================================
    // Changing parameters during processing shouldn't cause glitches

    std::cout << "\n=== PARAMETER CHANGE STABILITY ===\n";
    std::cout << "Expected: No discontinuities when switching modes\n\n";

    for (int startMode = 0; startMode < 2; startMode++) {
        const char* fromName = (startMode == 0) ? "Ampex" : "Studer";
        const char* toName = (startMode == 0) ? "Studer" : "Ampex";
        double fromBias = (startMode == 0) ? 0.65 : 0.82;
        double toBias = (startMode == 0) ? 0.82 : 0.65;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(fromBias, 1.0);

        // Process some signal
        double amplitude = 0.5;
        double freq = 1000.0;
        double maxJump = 0.0;
        double prevOutput = 0.0;

        for (int i = 0; i < 10000; ++i) {
            double phase = 2.0 * M_PI * freq * i / SAMPLE_RATE;
            double input = amplitude * std::sin(phase);

            // Switch parameters mid-stream
            if (i == 5000) {
                processor.setParameters(toBias, 1.0);
            }

            double output = processor.processSample(input);

            if (i > 0) {
                double jump = std::abs(output - prevOutput);
                maxJump = std::max(maxJump, jump);
            }
            prevOutput = output;
        }

        // Max sample-to-sample jump should be reasonable (< 0.5 for 1kHz sine)
        bool smooth = (maxJump < 0.5);

        std::cout << fromName << " -> " << toName << ": max jump = "
                  << std::fixed << std::setprecision(4) << maxJump << "  "
                  << (smooth ? "PASS" : "FAIL") << "\n";

        if (!smooth) allPassed = false;
    }

    // =========================================================================
    // TEST 5: SAMPLE RATE STABILITY
    // =========================================================================
    // Processor should work at various sample rates

    std::cout << "\n=== SAMPLE RATE STABILITY ===\n";
    std::cout << "Expected: Valid output at all common sample rates\n\n";

    double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    int numRates = 6;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        std::cout << modeName << ":\n";

        int passed = 0;
        for (int r = 0; r < numRates; r++) {
            TapeHysteresis::HybridTapeProcessor processor;
            processor.setSampleRate(sampleRates[r]);
            processor.setParameters(bias, 1.0);

            bool valid = true;
            double amplitude = 0.5;
            double freq = 1000.0;

            for (int i = 0; i < 1000; ++i) {
                double phase = 2.0 * M_PI * freq * i / sampleRates[r];
                double input = amplitude * std::sin(phase);
                double output = processor.processSample(input);

                if (!isValidSample(output)) {
                    valid = false;
                    break;
                }
            }

            if (valid) passed++;
            std::cout << "  " << std::setw(8) << sampleRates[r] << " Hz: "
                      << (valid ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "  Result: " << passed << "/" << numRates << "\n\n";
        if (passed != numRates) allPassed = false;
    }

    // =========================================================================
    // TEST 6: LONG-TERM STABILITY (NO DRIFT)
    // =========================================================================
    // After extended processing, output should still be stable

    std::cout << "\n=== LONG-TERM STABILITY TEST ===\n";
    std::cout << "Expected: Stable output after 10 seconds of processing\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = 0.5;
        double freq = 1000.0;
        int totalSamples = static_cast<int>(SAMPLE_RATE * 10);  // 10 seconds

        double minOutput = 1e10, maxOutput = -1e10;
        bool valid = true;

        for (int i = 0; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * freq * i / SAMPLE_RATE;
            double input = amplitude * std::sin(phase);
            double output = processor.processSample(input);

            if (!isValidSample(output)) {
                valid = false;
                break;
            }

            // Track output range in the last second
            if (i > totalSamples - static_cast<int>(SAMPLE_RATE)) {
                minOutput = std::min(minOutput, output);
                maxOutput = std::max(maxOutput, output);
            }
        }

        // Output should still be oscillating in a reasonable range
        bool stable = valid && (maxOutput - minOutput > 0.1) && (maxOutput < 10.0);

        std::cout << modeName << ": range=[" << std::fixed << std::setprecision(3)
                  << minOutput << ", " << maxOutput << "]  "
                  << (stable ? "PASS" : "FAIL") << "\n";

        if (!stable) allPassed = false;
    }

    // =========================================================================
    // TEST 7: RESET FUNCTIONALITY
    // =========================================================================
    // After reset, processor should behave identically to fresh instance

    std::cout << "\n=== RESET FUNCTIONALITY TEST ===\n";
    std::cout << "Expected: Reset produces identical output to fresh instance\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        // Fresh instance
        TapeHysteresis::HybridTapeProcessor fresh;
        fresh.setSampleRate(SAMPLE_RATE);
        fresh.setParameters(bias, 1.0);

        // Used and reset instance
        TapeHysteresis::HybridTapeProcessor used;
        used.setSampleRate(SAMPLE_RATE);
        used.setParameters(bias, 1.0);

        // Process some signal
        for (int i = 0; i < 10000; ++i) {
            used.processSample(0.5 * std::sin(2.0 * M_PI * 1000.0 * i / SAMPLE_RATE));
        }

        // Reset
        used.reset();

        // Compare outputs
        double maxDiff = 0.0;
        for (int i = 0; i < 1000; ++i) {
            double phase = 2.0 * M_PI * 1000.0 * i / SAMPLE_RATE;
            double input = 0.3 * std::sin(phase);

            double freshOut = fresh.processSample(input);
            double usedOut = used.processSample(input);

            maxDiff = std::max(maxDiff, std::abs(freshOut - usedOut));
        }

        bool identical = (maxDiff < 1e-10);

        std::cout << modeName << ": max difference = " << std::scientific << maxDiff
                  << "  " << std::fixed << (identical ? "PASS" : "FAIL") << "\n";

        if (!identical) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "STABILITY TEST: ALL PASSED\n";
    } else {
        std::cout << "STABILITY TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

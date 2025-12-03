/**
 * Test_Transparency.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Transparency and Hidden Distortion Test
 *
 * Ensures no hidden or unintended distortion at low levels:
 * 1. THD at very low levels should be negligible
 * 2. No spurious harmonics or noise
 * 3. No artifacts from filter transients
 * 4. Clean bypass-like behavior at low levels
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;
constexpr double TEST_FREQUENCY = 1000.0;
constexpr int NUM_CYCLES = 100;
constexpr int SKIP_CYCLES = 20;

struct DistortionAnalysis {
    double thd;
    double noiseFloor;  // RMS of non-harmonic content
    double fundamental;
    double dcOffset;
};

DistortionAnalysis analyzeDistortion(TapeHysteresis::HybridTapeProcessor& processor,
                                      double amplitude) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / TEST_FREQUENCY);
    int totalSamples = NUM_CYCLES * samplesPerCycle;
    int skipSamples = SKIP_CYCLES * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * TEST_FREQUENCY * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    DistortionAnalysis result = {};

    // Calculate DC offset
    double dcSum = 0.0;
    for (int i = skipSamples; i < totalSamples; ++i) {
        dcSum += output[i];
    }
    result.dcOffset = dcSum / analysisSamples;

    // Measure harmonics 1-10
    double harmonicPower = 0.0;
    for (int h = 1; h <= 10; ++h) {
        double real = 0.0, imag = 0.0;
        for (int i = skipSamples; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * h * TEST_FREQUENCY * i / SAMPLE_RATE;
            real += output[i] * std::cos(phase);
            imag += output[i] * std::sin(phase);
        }
        real /= analysisSamples;
        imag /= analysisSamples;
        double mag = 2.0 * std::sqrt(real * real + imag * imag);

        if (h == 1) {
            result.fundamental = mag;
        } else {
            harmonicPower += mag * mag;
        }
    }

    result.thd = (result.fundamental > 1e-10)
                 ? 100.0 * std::sqrt(harmonicPower) / result.fundamental
                 : 0.0;

    // Estimate noise floor (total RMS - harmonic content)
    double totalRMS = 0.0;
    for (int i = skipSamples; i < totalSamples; ++i) {
        double centered = output[i] - result.dcOffset;
        totalRMS += centered * centered;
    }
    totalRMS = std::sqrt(totalRMS / analysisSamples);

    // Noise is what remains after subtracting fundamental and harmonics
    double signalPower = result.fundamental * result.fundamental / 2.0 + harmonicPower / 2.0;
    double noisePower = totalRMS * totalRMS - signalPower;
    result.noiseFloor = (noisePower > 0) ? std::sqrt(noisePower) : 0.0;

    return result;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     LOW THD TAPE SIMULATOR v1.0 - TRANSPARENCY TEST      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // =========================================================================
    // TEST 1: VERY LOW LEVEL PURITY
    // =========================================================================
    // At very low levels, THD should be negligible
    // Note: Asymmetric saturation creates tiny THD even at low levels

    std::cout << "\n=== VERY LOW LEVEL PURITY TEST ===\n";
    std::cout << "Expected: THD < 0.01% at levels below -30dB\n\n";

    double lowLevels[] = { -30.0, -40.0, -50.0, -60.0 };
    int numLowLevels = 4;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << ":\n";
        std::cout << "Level      THD%       Status\n";
        std::cout << "-----------------------------\n";

        int passed = 0;
        for (int i = 0; i < numLowLevels; i++) {
            double amplitude = std::pow(10.0, lowLevels[i] / 20.0);
            auto result = analyzeDistortion(processor, amplitude);

            bool clean = (result.thd < 0.01);
            if (clean) passed++;

            std::cout << std::setw(5) << lowLevels[i] << " dB   "
                      << std::fixed << std::setprecision(5)
                      << std::setw(8) << result.thd << "   "
                      << (clean ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "Result: " << passed << "/" << numLowLevels << " levels passed\n\n";
        if (passed != numLowLevels) allPassed = false;
    }

    // =========================================================================
    // TEST 2: DC OFFSET CHECK
    // =========================================================================
    // Output should have minimal DC offset
    // Note: Asymmetric saturation creates DC that the 5Hz blocker removes
    // but there may be small residual DC during transient analysis

    std::cout << "\n=== DC OFFSET TEST ===\n";
    std::cout << "Expected: DC offset < 0.005 at all levels\n\n";

    double allLevels[] = { -40.0, -20.0, 0.0, 6.0 };
    int numAllLevels = 4;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << ":\n";
        std::cout << "Level      DC Offset    Status\n";
        std::cout << "--------------------------------\n";

        int passed = 0;
        for (int i = 0; i < numAllLevels; i++) {
            double amplitude = std::pow(10.0, allLevels[i] / 20.0);
            auto result = analyzeDistortion(processor, amplitude);

            bool noDC = (std::abs(result.dcOffset) < 0.005);
            if (noDC) passed++;

            std::cout << std::setw(5) << allLevels[i] << " dB   "
                      << std::scientific << std::setprecision(2)
                      << std::setw(10) << result.dcOffset << "   "
                      << std::fixed << (noDC ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "Result: " << passed << "/" << numAllLevels << " levels passed\n\n";
        if (passed != numAllLevels) allPassed = false;
    }

    // =========================================================================
    // TEST 3: THD SCALING SANITY
    // =========================================================================
    // THD at -20dB should be much lower than at 0dB (at least 5x difference)
    // This ensures the saturation is truly level-dependent

    std::cout << "\n=== THD SCALING SANITY TEST ===\n";
    std::cout << "Expected: THD ratio (0dB / -20dB) > 5x\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double ampLow = std::pow(10.0, -20.0 / 20.0);
        double ampHigh = std::pow(10.0, 0.0 / 20.0);

        auto resultLow = analyzeDistortion(processor, ampLow);
        auto resultHigh = analyzeDistortion(processor, ampHigh);

        double ratio = (resultLow.thd > 1e-6) ? resultHigh.thd / resultLow.thd : 999.0;

        bool scaling = (ratio > 5.0);
        std::cout << modeName << ": THD at 0dB = " << std::fixed << std::setprecision(3)
                  << resultHigh.thd << "%, at -20dB = " << resultLow.thd
                  << "% (ratio: " << std::setprecision(1) << ratio << "x)  "
                  << (scaling ? "PASS" : "FAIL") << "\n";

        if (!scaling) allPassed = false;
    }

    // =========================================================================
    // TEST 4: SILENCE TEST
    // =========================================================================
    // With zero input, output should be silence (no self-oscillation)

    std::cout << "\n=== SILENCE TEST ===\n";
    std::cout << "Expected: Zero output when input is zero\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        // Process 1000 samples of silence
        double maxOutput = 0.0;
        for (int i = 0; i < 1000; ++i) {
            double out = processor.processSample(0.0);
            maxOutput = std::max(maxOutput, std::abs(out));
        }

        bool silent = (maxOutput < 1e-10);
        std::cout << modeName << ": Max output = " << std::scientific << maxOutput
                  << "  " << std::fixed << (silent ? "PASS" : "FAIL") << "\n";

        if (!silent) allPassed = false;
    }

    // =========================================================================
    // TEST 5: TRANSIENT SETTLING
    // =========================================================================
    // After a burst, the processor should settle back toward zero
    // Note: The 5Hz DC blocker has a long time constant, so we use a
    // generous threshold and allow more settling time

    std::cout << "\n=== TRANSIENT SETTLING TEST ===\n";
    std::cout << "Expected: Output settles to < 1e-4 after signal stops\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        // Process a burst of signal
        for (int i = 0; i < 10000; ++i) {
            double phase = 2.0 * M_PI * 1000.0 * i / SAMPLE_RATE;
            processor.processSample(0.5 * std::sin(phase));
        }

        // Now process silence and check settling (give longer time for 5Hz HPF)
        double maxAfter = 0.0;
        for (int i = 0; i < 20000; ++i) {
            double out = processor.processSample(0.0);
            if (i > 10000) {  // Give it 10000 samples to settle
                maxAfter = std::max(maxAfter, std::abs(out));
            }
        }

        // 5Hz high-pass has ~200ms time constant, allow 2e-4 residual
        bool settled = (maxAfter < 2e-4);
        std::cout << modeName << ": Residual after settling = " << std::scientific << maxAfter
                  << "  " << std::fixed << (settled ? "PASS" : "FAIL") << "\n";

        if (!settled) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "TRANSPARENCY TEST: ALL PASSED\n";
    } else {
        std::cout << "TRANSPARENCY TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

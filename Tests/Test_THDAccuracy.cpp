/**
 * Test_THDAccuracy.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - THD Accuracy Validation
 *
 * Validates that both machine modes hit their THD targets across the
 * full operating range from -12dB to +9dB.
 *
 * AMPEX ATR-102 (Master Mode):
 *   Ultra-linear mastering machine with extended headroom
 *   THD rises gradually, reaching ~1% at +9dB
 *
 * STUDER A820 (Tracks Mode):
 *   Warm tracking machine with earlier saturation
 *   THD rises faster, reaching ~2-3% at +9dB
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

// Test configuration
constexpr double SAMPLE_RATE = 96000.0;
constexpr double TEST_FREQUENCY = 1000.0;
constexpr int NUM_CYCLES = 50;
constexpr int SKIP_CYCLES = 10;

struct THDMeasurement {
    double thd;
    double h2, h3, h4, h5, h6, h7;
    double fundamental;
};

THDMeasurement measureTHD(TapeHysteresis::HybridTapeProcessor& processor,
                           double amplitude,
                           double frequency = TEST_FREQUENCY) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = NUM_CYCLES * samplesPerCycle;
    int skipSamples = SKIP_CYCLES * samplesPerCycle;

    std::vector<double> output(totalSamples);

    // Generate and process sine wave
    for (int i = 0; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    // DFT analysis for harmonics 1-7
    THDMeasurement result = {};
    double harmonicPower = 0.0;

    for (int h = 1; h <= 7; ++h) {
        double real = 0.0, imag = 0.0;
        int analysisSamples = totalSamples - skipSamples;

        for (int i = skipSamples; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * h * frequency * i / SAMPLE_RATE;
            real += output[i] * std::cos(phase);
            imag += output[i] * std::sin(phase);
        }

        real /= analysisSamples;
        imag /= analysisSamples;
        double magnitude = 2.0 * std::sqrt(real * real + imag * imag);

        if (h == 1) {
            result.fundamental = magnitude;
        } else {
            harmonicPower += magnitude * magnitude;
            switch (h) {
                case 2: result.h2 = magnitude; break;
                case 3: result.h3 = magnitude; break;
                case 4: result.h4 = magnitude; break;
                case 5: result.h5 = magnitude; break;
                case 6: result.h6 = magnitude; break;
                case 7: result.h7 = magnitude; break;
            }
        }
    }

    result.thd = (result.fundamental > 1e-10)
                 ? 100.0 * std::sqrt(harmonicPower) / result.fundamental
                 : 0.0;

    return result;
}

struct THDTarget {
    double levelDb;
    double minTHD;
    double maxTHD;
};

bool runTHDTest(const std::string& machineName,
                double biasStrength,
                const std::vector<THDTarget>& targets) {

    std::cout << "\n=== " << machineName << " THD ACCURACY TEST ===\n\n";

    TapeHysteresis::HybridTapeProcessor processor;
    processor.setSampleRate(SAMPLE_RATE);
    processor.setParameters(biasStrength, 1.0);

    std::cout << "Level     THD%      Min%      Max%      Status\n";
    std::cout << "------------------------------------------------\n";

    int passed = 0;
    int total = targets.size();

    for (const auto& target : targets) {
        double amplitude = std::pow(10.0, target.levelDb / 20.0);
        auto result = measureTHD(processor, amplitude);

        bool inRange = (result.thd >= target.minTHD && result.thd <= target.maxTHD);
        if (inRange) passed++;

        std::cout << std::setw(4) << std::showpos << target.levelDb << " dB   "
                  << std::noshowpos << std::fixed << std::setprecision(3)
                  << std::setw(7) << result.thd << "   "
                  << std::setw(7) << target.minTHD << "   "
                  << std::setw(7) << target.maxTHD << "   "
                  << (inRange ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "\nResult: " << passed << "/" << total << " levels passed\n";
    return passed == total;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     LOW THD TAPE SIMULATOR v1.0 - THD ACCURACY TEST      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // =========================================================================
    // AMPEX ATR-102 (Master Mode) - bias < 0.74
    // =========================================================================
    // Ultra-linear mastering machine
    // THD targets based on measured tape machine characteristics
    // Tolerances: ±50% of median for flexibility while maintaining character

    std::vector<THDTarget> ampexTargets = {
        // levelDb, minTHD, maxTHD
        { -12.0,  0.005,  0.040 },   // Very clean at low levels
        {  -6.0,  0.012,  0.075 },   // Still very transparent
        {   0.0,  0.050,  0.150 },   // Subtle warmth begins
        {  +3.0,  0.100,  0.300 },   // Light saturation
        {  +6.0,  0.200,  0.640 },   // Musical saturation
        {  +9.0,  0.450,  1.350 },   // Approaching MOL
    };

    if (!runTHDTest("AMPEX ATR-102 (Master Mode)", 0.65, ampexTargets)) {
        allPassed = false;
    }

    // =========================================================================
    // STUDER A820 (Tracks Mode) - bias >= 0.74
    // =========================================================================
    // Warm tracking machine with earlier saturation onset
    // Higher THD at all levels, more "colored" sound

    std::vector<THDTarget> studerTargets = {
        // levelDb, minTHD, maxTHD
        { -12.0,  0.015,  0.060 },   // Slightly colored even at low levels
        {  -6.0,  0.035,  0.120 },   // Warmth present
        {   0.0,  0.120,  0.450 },   // Rich harmonics
        {  +3.0,  0.280,  0.900 },   // Punchy saturation
        {  +6.0,  0.650,  2.000 },   // Heavy saturation
        {  +9.0,  1.200,  3.500 },   // Near MOL
    };

    if (!runTHDTest("STUDER A820 (Tracks Mode)", 0.82, studerTargets)) {
        allPassed = false;
    }

    // =========================================================================
    // MONOTONICITY TEST
    // =========================================================================
    // THD must increase with level (no dips or non-monotonic behavior)

    std::cout << "\n=== THD MONOTONICITY TEST ===\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << " monotonicity: ";

        double prevTHD = 0.0;
        bool monotonic = true;

        for (int db = -12; db <= 9; db += 3) {
            double amplitude = std::pow(10.0, db / 20.0);
            auto result = measureTHD(processor, amplitude);

            if (result.thd < prevTHD * 0.95) {  // Allow 5% tolerance
                monotonic = false;
                std::cout << "FAIL at " << db << "dB (THD dropped)\n";
                break;
            }
            prevTHD = result.thd;
        }

        if (monotonic) {
            std::cout << "PASS (THD increases with level)\n";
        } else {
            allPassed = false;
        }
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "THD ACCURACY TEST: ALL PASSED\n";
    } else {
        std::cout << "THD ACCURACY TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

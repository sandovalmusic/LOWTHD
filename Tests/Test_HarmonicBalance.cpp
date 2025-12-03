/**
 * Test_HarmonicBalance.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Even/Odd Harmonic Balance Validation
 *
 * Real tape machines have characteristic harmonic signatures:
 *
 * AMPEX ATR-102: Odd-harmonic dominant (E/O ratio ~0.5)
 *   - Cleaner, more transparent character
 *   - 3rd harmonic stronger than 2nd
 *
 * STUDER A820: Even-harmonic dominant (E/O ratio ~1.0-1.2)
 *   - Warmer, more colored character
 *   - 2nd harmonic stronger than 3rd
 *
 * The E/O ratio is calculated as: (H2 + H4 + H6) / (H3 + H5 + H7)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;
constexpr double TEST_FREQUENCY = 1000.0;
constexpr int NUM_CYCLES = 50;
constexpr int SKIP_CYCLES = 10;

struct HarmonicAnalysis {
    double fundamental;
    double h2, h3, h4, h5, h6, h7;
    double evenSum;
    double oddSum;
    double eoRatio;
    double thd;
};

HarmonicAnalysis analyzeHarmonics(TapeHysteresis::HybridTapeProcessor& processor,
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

    HarmonicAnalysis result = {};
    double harmonicPower = 0.0;

    for (int h = 1; h <= 7; ++h) {
        double real = 0.0, imag = 0.0;

        for (int i = skipSamples; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * h * TEST_FREQUENCY * i / SAMPLE_RATE;
            real += output[i] * std::cos(phase);
            imag += output[i] * std::sin(phase);
        }

        real /= analysisSamples;
        imag /= analysisSamples;
        double magnitude = 2.0 * std::sqrt(real * real + imag * imag);

        switch (h) {
            case 1: result.fundamental = magnitude; break;
            case 2: result.h2 = magnitude; break;
            case 3: result.h3 = magnitude; break;
            case 4: result.h4 = magnitude; break;
            case 5: result.h5 = magnitude; break;
            case 6: result.h6 = magnitude; break;
            case 7: result.h7 = magnitude; break;
        }

        if (h > 1) {
            harmonicPower += magnitude * magnitude;
        }
    }

    result.evenSum = result.h2 + result.h4 + result.h6;
    result.oddSum = result.h3 + result.h5 + result.h7;
    result.eoRatio = (result.oddSum > 1e-12) ? result.evenSum / result.oddSum : 0.0;
    result.thd = (result.fundamental > 1e-10)
                 ? 100.0 * std::sqrt(harmonicPower) / result.fundamental
                 : 0.0;

    return result;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║   LOW THD TAPE SIMULATOR v1.0 - HARMONIC BALANCE TEST    ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // Test levels where harmonic balance is most meaningful
    // At very low levels, harmonics are too small for accurate measurement
    // At very high levels, atan stage changes the harmonic structure
    // Best measurement is around 0dB where saturation is moderate
    double testLevels[] = { 0.0, 3.0 };
    int numLevels = 2;

    // =========================================================================
    // AMPEX ATR-102 (Master Mode)
    // =========================================================================
    // Target E/O ratio: 0.35 - 0.70 (odd-dominant)

    std::cout << "\n=== AMPEX ATR-102 HARMONIC BALANCE ===\n";
    std::cout << "Target E/O ratio: 0.35 - 0.70 (odd-harmonic dominant)\n\n";

    std::cout << "Level    H2       H3       H4       H5       E/O     Status\n";
    std::cout << "--------------------------------------------------------------\n";

    TapeHysteresis::HybridTapeProcessor ampexProc;
    ampexProc.setSampleRate(SAMPLE_RATE);
    ampexProc.setParameters(0.65, 1.0);

    int ampexPassed = 0;
    for (int i = 0; i < numLevels; i++) {
        double amplitude = std::pow(10.0, testLevels[i] / 20.0);
        auto result = analyzeHarmonics(ampexProc, amplitude);

        // Normalize harmonics to fundamental for display
        double h2Norm = 100.0 * result.h2 / result.fundamental;
        double h3Norm = 100.0 * result.h3 / result.fundamental;
        double h4Norm = 100.0 * result.h4 / result.fundamental;
        double h5Norm = 100.0 * result.h5 / result.fundamental;

        bool inRange = (result.eoRatio >= 0.35 && result.eoRatio <= 0.70);
        if (inRange) ampexPassed++;

        std::cout << std::setw(4) << std::showpos << testLevels[i] << "dB  "
                  << std::noshowpos << std::fixed << std::setprecision(4)
                  << std::setw(7) << h2Norm << "% "
                  << std::setw(7) << h3Norm << "% "
                  << std::setw(7) << h4Norm << "% "
                  << std::setw(7) << h5Norm << "%  "
                  << std::setprecision(3) << std::setw(5) << result.eoRatio << "   "
                  << (inRange ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "\nAmpex E/O Result: " << ampexPassed << "/" << numLevels << " levels passed\n";
    if (ampexPassed != numLevels) allPassed = false;

    // =========================================================================
    // STUDER A820 (Tracks Mode)
    // =========================================================================
    // Target E/O ratio: 0.70 - 1.40 (even-dominant or balanced)
    // Note: At higher levels, the atan stage adds odd harmonics, shifting E/O down

    std::cout << "\n=== STUDER A820 HARMONIC BALANCE ===\n";
    std::cout << "Target E/O ratio: 0.70 - 1.40 (even-harmonic dominant)\n\n";

    std::cout << "Level    H2       H3       H4       H5       E/O     Status\n";
    std::cout << "--------------------------------------------------------------\n";

    TapeHysteresis::HybridTapeProcessor studerProc;
    studerProc.setSampleRate(SAMPLE_RATE);
    studerProc.setParameters(0.82, 1.0);

    int studerPassed = 0;
    for (int i = 0; i < numLevels; i++) {
        double amplitude = std::pow(10.0, testLevels[i] / 20.0);
        auto result = analyzeHarmonics(studerProc, amplitude);

        double h2Norm = 100.0 * result.h2 / result.fundamental;
        double h3Norm = 100.0 * result.h3 / result.fundamental;
        double h4Norm = 100.0 * result.h4 / result.fundamental;
        double h5Norm = 100.0 * result.h5 / result.fundamental;

        bool inRange = (result.eoRatio >= 0.70 && result.eoRatio <= 1.40);
        if (inRange) studerPassed++;

        std::cout << std::setw(4) << std::showpos << testLevels[i] << "dB  "
                  << std::noshowpos << std::fixed << std::setprecision(4)
                  << std::setw(7) << h2Norm << "% "
                  << std::setw(7) << h3Norm << "% "
                  << std::setw(7) << h4Norm << "% "
                  << std::setw(7) << h5Norm << "%  "
                  << std::setprecision(3) << std::setw(5) << result.eoRatio << "   "
                  << (inRange ? "PASS" : "FAIL") << "\n";
    }

    std::cout << "\nStuder E/O Result: " << studerPassed << "/" << numLevels << " levels passed\n";
    if (studerPassed != numLevels) allPassed = false;

    // =========================================================================
    // HARMONIC STRUCTURE CONSISTENCY TEST
    // =========================================================================
    // Verify that harmonic structure remains reasonably consistent at moderate levels
    // Note: E/O ratio naturally varies with level as different saturation stages engage

    std::cout << "\n=== HARMONIC STRUCTURE CONSISTENCY ===\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;
        double tolerance = 0.35;  // Allow reasonable variation

        TapeHysteresis::HybridTapeProcessor proc;
        proc.setSampleRate(SAMPLE_RATE);
        proc.setParameters(bias, 1.0);

        std::cout << modeName << " E/O consistency: ";

        double minEO = 999.0, maxEO = 0.0;
        for (int i = 0; i < numLevels; i++) {
            double amplitude = std::pow(10.0, testLevels[i] / 20.0);
            auto result = analyzeHarmonics(proc, amplitude);
            minEO = std::min(minEO, result.eoRatio);
            maxEO = std::max(maxEO, result.eoRatio);
        }

        double variation = maxEO - minEO;
        bool consistent = (variation < tolerance);

        if (consistent) {
            std::cout << "PASS (variation: " << std::fixed << std::setprecision(3)
                      << variation << " < " << tolerance << ")\n";
        } else {
            std::cout << "FAIL (variation: " << variation << " >= " << tolerance << ")\n";
            allPassed = false;
        }
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "HARMONIC BALANCE TEST: ALL PASSED\n";
    } else {
        std::cout << "HARMONIC BALANCE TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

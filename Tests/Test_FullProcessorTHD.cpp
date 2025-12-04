/**
 * Test_FullProcessorTHD.cpp
 *
 * Comprehensive THD validation for the complete HybridTapeProcessor
 * Tests the full signal chain including:
 *   - AC Bias Shielding (HF Cut / HF Restore)
 *   - Jiles-Atherton Hysteresis
 *   - Asymmetric Tanh + Atan Saturation
 *   - Machine EQ
 *   - Dispersive Allpass
 *   - DC Blocking
 *
 * Target MOL (Maximum Output Level @ 3% THD):
 *   - Ampex ATR-102: +12 dB (level = 3.98)
 *   - Studer A820: +9 dB (level = 2.83)
 *
 * E/O Harmonic Ratio Targets:
 *   - Ampex: 0.503 (odd-dominant, "cleaner" sound)
 *   - Studer: 1.122 (even-dominant, "warmer" sound)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

// Include the actual DSP files
#include "../Source/DSP/BiasShielding.h"
#include "../Source/DSP/JilesAthertonCore.h"
#include "../Source/DSP/MachineEQ.h"
#include "../Source/DSP/HybridTapeProcessor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace TapeHysteresis;

// ============================================================================
// TEST UTILITIES
// ============================================================================

struct THDResult {
    double thd;           // Total Harmonic Distortion (%)
    double fundamental;   // Fundamental magnitude
    double h2, h3, h4, h5; // Individual harmonic magnitudes
    double eoRatio;       // Even/Odd ratio (H2+H4)/(H3+H5)
};

THDResult measureFullProcessorTHD(HybridTapeProcessor& processor, double inputLevel,
                                   double sampleRate, double testFreq = 1000.0)
{
    processor.reset();

    int numCycles = 300;
    int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    int totalSamples = numCycles * samplesPerCycle;
    int warmupSamples = 50 * samplesPerCycle;  // Let J-A and envelope settle
    int measureSamples = totalSamples - warmupSamples;

    std::vector<double> output(totalSamples);

    // Process through full chain
    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = inputLevel * std::sin(2.0 * M_PI * testFreq * t);
        output[i] = processor.processSample(input);
    }

    // DFT at specific harmonic frequencies
    THDResult result;
    double harmonicMags[6] = {0};  // H1 through H5 (index 0 unused)

    for (int h = 1; h <= 5; ++h)
    {
        double freq = testFreq * h;
        double sumCos = 0.0, sumSin = 0.0;

        for (int i = warmupSamples; i < totalSamples; ++i)
        {
            double t = static_cast<double>(i) / sampleRate;
            sumCos += output[i] * std::cos(2.0 * M_PI * freq * t);
            sumSin += output[i] * std::sin(2.0 * M_PI * freq * t);
        }

        harmonicMags[h] = 2.0 * std::sqrt(sumCos * sumCos + sumSin * sumSin) / measureSamples;
    }

    result.fundamental = harmonicMags[1];
    result.h2 = harmonicMags[2];
    result.h3 = harmonicMags[3];
    result.h4 = harmonicMags[4];
    result.h5 = harmonicMags[5];

    // THD = sqrt(H2^2 + H3^2 + H4^2 + H5^2) / H1 * 100
    double harmonicSum = result.h2 * result.h2 + result.h3 * result.h3 +
                         result.h4 * result.h4 + result.h5 * result.h5;
    result.thd = 100.0 * std::sqrt(harmonicSum) / result.fundamental;

    // E/O ratio
    double evenSum = result.h2 + result.h4;
    double oddSum = result.h3 + result.h5;
    result.eoRatio = (oddSum > 1e-12) ? evenSum / oddSum : 0.0;

    return result;
}

// Find the level that produces target THD using binary search
double findMOL(HybridTapeProcessor& processor, double targetTHD, double sampleRate,
               double minLevel = 0.5, double maxLevel = 10.0, double tolerance = 0.1)
{
    double low = minLevel;
    double high = maxLevel;

    for (int iter = 0; iter < 20; ++iter)
    {
        double mid = (low + high) / 2.0;
        THDResult result = measureFullProcessorTHD(processor, mid, sampleRate);

        if (std::abs(result.thd - targetTHD) < tolerance)
            return mid;

        if (result.thd < targetTHD)
            low = mid;
        else
            high = mid;
    }

    return (low + high) / 2.0;
}

// Convert level to dB
double levelToDb(double level)
{
    return 20.0 * std::log10(level);
}

// Convert dB to level
double dbToLevel(double db)
{
    return std::pow(10.0, db / 20.0);
}

// ============================================================================
// TEST RESULTS
// ============================================================================
struct TestResult
{
    std::string name;
    bool passed;
    std::string details;
};

std::vector<TestResult> allResults;

void reportTest(const std::string& name, bool passed, const std::string& details = "")
{
    allResults.push_back({name, passed, details});
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name;
    if (!details.empty()) std::cout << " - " << details;
    std::cout << "\n";
}

// ============================================================================
// TEST 1: THD vs LEVEL CURVE (Full Processor)
// ============================================================================
void testTHDvsLevel()
{
    std::cout << "\n=== TEST 1: Full Processor THD vs Level ===\n";

    double sampleRate = 96000.0;

    // Test levels from -12dB to +15dB
    double testLevels[] = {0.25, 0.5, 1.0, 1.414, 2.0, 2.828, 3.98, 5.62};
    std::string levelNames[] = {"-12 dB", "-6 dB", "0 dB", "+3 dB", "+6 dB", "+9 dB", "+12 dB", "+15 dB"};
    int numLevels = 8;

    // Test Ampex
    std::cout << "\n  AMPEX ATR-102 (Master Mode):\n";
    std::cout << "  Level     THD%      H2/H3     E/O Ratio\n";
    std::cout << "  ----------------------------------------\n";

    HybridTapeProcessor ampex;
    ampex.setSampleRate(sampleRate);
    ampex.setParameters(0.5, 1.0);  // bias < 0.74 = Ampex mode

    for (int i = 0; i < numLevels; ++i)
    {
        THDResult result = measureFullProcessorTHD(ampex, testLevels[i], sampleRate);
        double h2h3 = (result.h3 > 1e-12) ? result.h2 / result.h3 : 0.0;

        std::cout << "  " << std::setw(7) << levelNames[i] << "   "
                  << std::fixed << std::setprecision(3) << std::setw(6) << result.thd << "%   "
                  << std::setprecision(2) << std::setw(6) << h2h3 << "    "
                  << std::setprecision(3) << result.eoRatio << "\n";
    }

    // Test Studer
    std::cout << "\n  STUDER A820 (Tracks Mode):\n";
    std::cout << "  Level     THD%      H2/H3     E/O Ratio\n";
    std::cout << "  ----------------------------------------\n";

    HybridTapeProcessor studer;
    studer.setSampleRate(sampleRate);
    studer.setParameters(0.8, 1.0);  // bias >= 0.74 = Studer mode

    for (int i = 0; i < numLevels; ++i)
    {
        THDResult result = measureFullProcessorTHD(studer, testLevels[i], sampleRate);
        double h2h3 = (result.h3 > 1e-12) ? result.h2 / result.h3 : 0.0;

        std::cout << "  " << std::setw(7) << levelNames[i] << "   "
                  << std::fixed << std::setprecision(3) << std::setw(6) << result.thd << "%   "
                  << std::setprecision(2) << std::setw(6) << h2h3 << "    "
                  << std::setprecision(3) << result.eoRatio << "\n";
    }

    // Verify Studer has higher THD than Ampex at same level
    THDResult ampex0dB = measureFullProcessorTHD(ampex, 1.0, sampleRate);
    THDResult studer0dB = measureFullProcessorTHD(studer, 1.0, sampleRate);

    reportTest("Studer THD > Ampex THD @ 0dB", studer0dB.thd > ampex0dB.thd,
               "Studer " + std::to_string(studer0dB.thd).substr(0,5) + "% > Ampex " +
               std::to_string(ampex0dB.thd).substr(0,5) + "%");
}

// ============================================================================
// TEST 2: MOL (Maximum Output Level @ 3% THD)
// ============================================================================
void testMOL()
{
    std::cout << "\n=== TEST 2: MOL (3% THD Point) ===\n";

    double sampleRate = 96000.0;
    double targetTHD = 3.0;

    // Ampex target: +12 dB (level = 3.98)
    HybridTapeProcessor ampex;
    ampex.setSampleRate(sampleRate);
    ampex.setParameters(0.5, 1.0);

    double ampexMOL = findMOL(ampex, targetTHD, sampleRate, 1.0, 8.0);
    double ampexMOLdB = levelToDb(ampexMOL);

    std::cout << "  Ampex ATR-102 MOL: " << std::fixed << std::setprecision(1)
              << ampexMOLdB << " dB (target: +12 dB)\n";

    // Studer target: +9 dB (level = 2.83)
    HybridTapeProcessor studer;
    studer.setSampleRate(sampleRate);
    studer.setParameters(0.8, 1.0);

    double studerMOL = findMOL(studer, targetTHD, sampleRate, 0.5, 6.0);
    double studerMOLdB = levelToDb(studerMOL);

    std::cout << "  Studer A820 MOL: " << std::fixed << std::setprecision(1)
              << studerMOLdB << " dB (target: +9 dB)\n";

    // Ampex should have higher headroom than Studer
    reportTest("Ampex MOL > Studer MOL", ampexMOL > studerMOL,
               "Ampex +" + std::to_string(ampexMOLdB).substr(0,4) + "dB > Studer +" +
               std::to_string(studerMOLdB).substr(0,4) + "dB");

    // Check if MOL is in reasonable range (within 3dB of target)
    reportTest("Ampex MOL within 3dB of +12dB target", std::abs(ampexMOLdB - 12.0) < 3.0,
               std::to_string(ampexMOLdB).substr(0,4) + " dB");
    reportTest("Studer MOL within 3dB of +9dB target", std::abs(studerMOLdB - 9.0) < 3.0,
               std::to_string(studerMOLdB).substr(0,4) + " dB");
}

// ============================================================================
// TEST 3: EVEN/ODD HARMONIC RATIO
// ============================================================================
void testEvenOddRatio()
{
    std::cout << "\n=== TEST 3: Even/Odd Harmonic Ratio ===\n";

    double sampleRate = 96000.0;
    double testLevel = 2.0;  // +6dB - significant saturation

    // Ampex target: E/O = 0.503 (odd-dominant)
    HybridTapeProcessor ampex;
    ampex.setSampleRate(sampleRate);
    ampex.setParameters(0.5, 1.0);

    THDResult ampexResult = measureFullProcessorTHD(ampex, testLevel, sampleRate);

    std::cout << "  Ampex ATR-102 @ +6dB:\n";
    std::cout << "    H2: " << std::scientific << std::setprecision(3) << ampexResult.h2 << "\n";
    std::cout << "    H3: " << ampexResult.h3 << "\n";
    std::cout << "    H4: " << ampexResult.h4 << "\n";
    std::cout << "    H5: " << ampexResult.h5 << "\n";
    std::cout << "    E/O Ratio: " << std::fixed << std::setprecision(3)
              << ampexResult.eoRatio << " (target: 0.503)\n";

    // Studer target: E/O = 1.122 (even-dominant)
    HybridTapeProcessor studer;
    studer.setSampleRate(sampleRate);
    studer.setParameters(0.8, 1.0);

    THDResult studerResult = measureFullProcessorTHD(studer, testLevel, sampleRate);

    std::cout << "\n  Studer A820 @ +6dB:\n";
    std::cout << "    H2: " << std::scientific << std::setprecision(3) << studerResult.h2 << "\n";
    std::cout << "    H3: " << studerResult.h3 << "\n";
    std::cout << "    H4: " << studerResult.h4 << "\n";
    std::cout << "    H5: " << studerResult.h5 << "\n";
    std::cout << "    E/O Ratio: " << std::fixed << std::setprecision(3)
              << studerResult.eoRatio << " (target: 1.122)\n";

    // Ampex should be odd-dominant (E/O < 1.0)
    reportTest("Ampex Odd-Dominant (E/O < 1.0)", ampexResult.eoRatio < 1.0,
               "E/O = " + std::to_string(ampexResult.eoRatio).substr(0,5));

    // Studer should be even-dominant (E/O > 1.0)
    reportTest("Studer Even-Dominant (E/O > 1.0)", studerResult.eoRatio > 1.0,
               "E/O = " + std::to_string(studerResult.eoRatio).substr(0,5));

    // Studer E/O should be higher than Ampex E/O
    reportTest("Studer E/O > Ampex E/O", studerResult.eoRatio > ampexResult.eoRatio,
               "Studer " + std::to_string(studerResult.eoRatio).substr(0,5) +
               " > Ampex " + std::to_string(ampexResult.eoRatio).substr(0,5));
}

// ============================================================================
// TEST 4: THD AT SPECIFIC LEVELS
// ============================================================================
void testTHDatSpecificLevels()
{
    std::cout << "\n=== TEST 4: THD at Specific Levels ===\n";

    double sampleRate = 96000.0;

    HybridTapeProcessor ampex;
    ampex.setSampleRate(sampleRate);
    ampex.setParameters(0.5, 1.0);

    HybridTapeProcessor studer;
    studer.setSampleRate(sampleRate);
    studer.setParameters(0.8, 1.0);

    // At 0dB, THD should be low (< 1% for Ampex, < 2% for Studer)
    THDResult ampex0dB = measureFullProcessorTHD(ampex, 1.0, sampleRate);
    THDResult studer0dB = measureFullProcessorTHD(studer, 1.0, sampleRate);

    reportTest("Ampex THD @ 0dB < 1%", ampex0dB.thd < 1.0,
               std::to_string(ampex0dB.thd).substr(0,5) + "%");
    reportTest("Studer THD @ 0dB < 2%", studer0dB.thd < 2.0,
               std::to_string(studer0dB.thd).substr(0,5) + "%");

    // At +6dB, THD should be moderate
    THDResult ampex6dB = measureFullProcessorTHD(ampex, 2.0, sampleRate);
    THDResult studer6dB = measureFullProcessorTHD(studer, 2.0, sampleRate);

    std::cout << "  Ampex @ +6dB: " << std::fixed << std::setprecision(3)
              << ampex6dB.thd << "% THD\n";
    std::cout << "  Studer @ +6dB: " << studer6dB.thd << "% THD\n";

    reportTest("THD increases with level (Ampex)", ampex6dB.thd > ampex0dB.thd,
               "+6dB: " + std::to_string(ampex6dB.thd).substr(0,5) + "% > 0dB: " +
               std::to_string(ampex0dB.thd).substr(0,5) + "%");
    reportTest("THD increases with level (Studer)", studer6dB.thd > studer0dB.thd,
               "+6dB: " + std::to_string(studer6dB.thd).substr(0,5) + "% > 0dB: " +
               std::to_string(studer0dB.thd).substr(0,5) + "%");
}

// ============================================================================
// TEST 5: FREQUENCY DEPENDENCE OF THD
// ============================================================================
void testFrequencyDependentTHD()
{
    std::cout << "\n=== TEST 5: Frequency-Dependent THD ===\n";

    double sampleRate = 96000.0;
    double testLevel = 2.0;  // +6dB

    double testFreqs[] = {100, 500, 1000, 4000, 8000, 12000};
    int numFreqs = 6;

    HybridTapeProcessor ampex;
    ampex.setSampleRate(sampleRate);
    ampex.setParameters(0.5, 1.0);

    std::cout << "  Ampex ATR-102 @ +6dB:\n";
    std::cout << "  Freq (Hz)    THD%\n";
    std::cout << "  ------------------\n";

    for (int i = 0; i < numFreqs; ++i)
    {
        THDResult result = measureFullProcessorTHD(ampex, testLevel, sampleRate, testFreqs[i]);
        std::cout << "  " << std::setw(8) << static_cast<int>(testFreqs[i]) << "   "
                  << std::fixed << std::setprecision(3) << result.thd << "%\n";
    }

    HybridTapeProcessor studer;
    studer.setSampleRate(sampleRate);
    studer.setParameters(0.8, 1.0);

    std::cout << "\n  Studer A820 @ +6dB:\n";
    std::cout << "  Freq (Hz)    THD%\n";
    std::cout << "  ------------------\n";

    for (int i = 0; i < numFreqs; ++i)
    {
        THDResult result = measureFullProcessorTHD(studer, testLevel, sampleRate, testFreqs[i]);
        std::cout << "  " << std::setw(8) << static_cast<int>(testFreqs[i]) << "   "
                  << std::fixed << std::setprecision(3) << result.thd << "%\n";
    }

    // HF should have less THD due to AC bias shielding (HF cut before saturation)
    THDResult ampex1k = measureFullProcessorTHD(ampex, testLevel, sampleRate, 1000);
    THDResult ampex8k = measureFullProcessorTHD(ampex, testLevel, sampleRate, 8000);

    reportTest("HF THD < LF THD (AC Bias Shielding)", ampex8k.thd < ampex1k.thd,
               "8kHz: " + std::to_string(ampex8k.thd).substr(0,5) + "% < 1kHz: " +
               std::to_string(ampex1k.thd).substr(0,5) + "%");
}

// ============================================================================
// MAIN
// ============================================================================
int main()
{
    std::cout << "================================================================\n";
    std::cout << "   Full Processor THD Validation Suite\n";
    std::cout << "================================================================\n";
    std::cout << "\n  Testing complete HybridTapeProcessor signal chain:\n";
    std::cout << "  AC Bias Shielding -> J-A Hysteresis -> Tanh/Atan Saturation\n";
    std::cout << "  -> Machine EQ -> Dispersive Allpass -> DC Blocking\n";

    testTHDvsLevel();
    testMOL();
    testEvenOddRatio();
    testTHDatSpecificLevels();
    testFrequencyDependentTHD();

    // Summary
    std::cout << "\n================================================================\n";
    std::cout << "   TEST SUMMARY\n";
    std::cout << "================================================================\n";

    int passed = 0, failed = 0;
    for (const auto& result : allResults)
    {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "\n  Total: " << (passed + failed) << " tests\n";
    std::cout << "  Passed: " << passed << "\n";
    std::cout << "  Failed: " << failed << "\n\n";

    if (failed > 0)
    {
        std::cout << "  Failed tests:\n";
        for (const auto& result : allResults)
        {
            if (!result.passed)
            {
                std::cout << "    - " << result.name;
                if (!result.details.empty()) std::cout << ": " << result.details;
                std::cout << "\n";
            }
        }
    }

    std::cout << "\n================================================================\n";
    std::cout << (failed == 0 ? "   ALL TESTS PASSED" : "   SOME TESTS FAILED") << "\n";
    std::cout << "================================================================\n";

    return (failed == 0) ? 0 : 1;
}

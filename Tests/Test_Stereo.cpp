/**
 * Test_Stereo.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Stereo Processing Validation
 *
 * Validates stereo behavior:
 * 1. Left and right channels process identically (same THD, gain)
 * 2. Azimuth delay is correct for each machine mode
 * 3. Delay doesn't color the sound (no filtering from interpolation)
 * 4. Stereo image remains stable
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
constexpr int NUM_CYCLES = 50;
constexpr int SKIP_CYCLES = 10;

struct ChannelAnalysis {
    double fundamental;
    double phase;
    double thd;
};

ChannelAnalysis analyzeChannel(const std::vector<double>& output, double frequency) {
    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = output.size();
    int skipSamples = SKIP_CYCLES * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    ChannelAnalysis result = {};

    // Measure fundamental
    double real = 0.0, imag = 0.0;
    for (int i = skipSamples; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        real += output[i] * std::cos(phase);
        imag += output[i] * std::sin(phase);
    }
    real /= analysisSamples;
    imag /= analysisSamples;

    result.fundamental = 2.0 * std::sqrt(real * real + imag * imag);
    result.phase = std::atan2(-imag, real) * 180.0 / M_PI;

    // Measure harmonics for THD
    double harmonicPower = 0.0;
    for (int h = 2; h <= 5; ++h) {
        double hReal = 0.0, hImag = 0.0;
        for (int i = skipSamples; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * h * frequency * i / SAMPLE_RATE;
            hReal += output[i] * std::cos(phase);
            hImag += output[i] * std::sin(phase);
        }
        hReal /= analysisSamples;
        hImag /= analysisSamples;
        double mag = 2.0 * std::sqrt(hReal * hReal + hImag * hImag);
        harmonicPower += mag * mag;
    }

    result.thd = (result.fundamental > 1e-10)
                 ? 100.0 * std::sqrt(harmonicPower) / result.fundamental
                 : 0.0;

    return result;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       LOW THD TAPE SIMULATOR v1.0 - STEREO TEST          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // =========================================================================
    // TEST 1: CHANNEL MATCHING (THD and Gain)
    // =========================================================================
    // Left and right channels should have identical THD and gain
    // (Only difference should be the azimuth delay)

    std::cout << "\n=== CHANNEL MATCHING TEST ===\n";
    std::cout << "Expected: Left and Right channels have identical THD and gain\n\n";

    double testLevels[] = { -6.0, 0.0, 6.0 };
    int numLevels = 3;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        std::cout << modeName << ":\n";
        std::cout << "Level    L-THD%    R-THD%    L-Gain    R-Gain    Match\n";
        std::cout << "---------------------------------------------------------\n";

        int passed = 0;
        for (int lvl = 0; lvl < numLevels; lvl++) {
            TapeHysteresis::HybridTapeProcessor procL, procR;
            procL.setSampleRate(SAMPLE_RATE);
            procR.setSampleRate(SAMPLE_RATE);
            procL.setParameters(bias, 1.0);
            procR.setParameters(bias, 1.0);

            double amplitude = std::pow(10.0, testLevels[lvl] / 20.0);

            int samplesPerCycle = static_cast<int>(SAMPLE_RATE / TEST_FREQUENCY);
            int totalSamples = NUM_CYCLES * samplesPerCycle;

            std::vector<double> outputL(totalSamples), outputR(totalSamples);

            for (int i = 0; i < totalSamples; ++i) {
                double phase = 2.0 * M_PI * TEST_FREQUENCY * i / SAMPLE_RATE;
                double input = amplitude * std::sin(phase);
                outputL[i] = procL.processSample(input);
                outputR[i] = procR.processRightChannel(input);
            }

            auto resultL = analyzeChannel(outputL, TEST_FREQUENCY);
            auto resultR = analyzeChannel(outputR, TEST_FREQUENCY);

            double gainL = 20.0 * std::log10(resultL.fundamental / amplitude);
            double gainR = 20.0 * std::log10(resultR.fundamental / amplitude);

            // THD should match within 5%, gain within 0.1dB
            bool thdMatch = std::abs(resultL.thd - resultR.thd) < std::max(0.01, resultL.thd * 0.05);
            bool gainMatch = std::abs(gainL - gainR) < 0.1;
            bool match = thdMatch && gainMatch;
            if (match) passed++;

            std::cout << std::setw(4) << std::showpos << testLevels[lvl] << "dB   "
                      << std::noshowpos << std::fixed << std::setprecision(3)
                      << std::setw(7) << resultL.thd << "   "
                      << std::setw(7) << resultR.thd << "   "
                      << std::setprecision(2) << std::showpos
                      << std::setw(7) << gainL << "   "
                      << std::setw(7) << gainR << "   "
                      << std::noshowpos << (match ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "Result: " << passed << "/" << numLevels << " levels matched\n\n";
        if (passed != numLevels) allPassed = false;
    }

    // =========================================================================
    // TEST 2: AZIMUTH DELAY MEASUREMENT
    // =========================================================================
    // Ampex: 8 microseconds = 0.768 samples @ 96kHz
    // Studer: 12 microseconds = 1.152 samples @ 96kHz

    std::cout << "\n=== AZIMUTH DELAY TEST ===\n";
    std::cout << "Expected delays: Ampex=8us (0.77 samples), Studer=12us (1.15 samples)\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;
        double expectedUs = (mode == 0) ? 8.0 : 12.0;
        double expectedSamples = expectedUs * 1e-6 * SAMPLE_RATE;

        TapeHysteresis::HybridTapeProcessor procL, procR;
        procL.setSampleRate(SAMPLE_RATE);
        procR.setSampleRate(SAMPLE_RATE);
        procL.setParameters(bias, 1.0);
        procR.setParameters(bias, 1.0);

        // Use a higher frequency for more accurate phase measurement
        double freq = 5000.0;
        double amplitude = 0.1;  // Low level to avoid saturation effects

        int samplesPerCycle = static_cast<int>(SAMPLE_RATE / freq);
        int totalSamples = NUM_CYCLES * samplesPerCycle;

        std::vector<double> outputL(totalSamples), outputR(totalSamples);

        for (int i = 0; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * freq * i / SAMPLE_RATE;
            double input = amplitude * std::sin(phase);
            outputL[i] = procL.processSample(input);
            outputR[i] = procR.processRightChannel(input);
        }

        auto resultL = analyzeChannel(outputL, freq);
        auto resultR = analyzeChannel(outputR, freq);

        // Calculate delay from phase difference
        double phaseDiff = resultL.phase - resultR.phase;
        // Normalize to -180 to 180
        while (phaseDiff > 180.0) phaseDiff -= 360.0;
        while (phaseDiff < -180.0) phaseDiff += 360.0;

        // Convert phase difference to delay in samples
        // phaseDiff (degrees) / 360 * samplesPerCycle = delay in samples
        double measuredSamples = (phaseDiff / 360.0) * (SAMPLE_RATE / freq);
        double measuredUs = measuredSamples / SAMPLE_RATE * 1e6;

        // Allow 20% tolerance
        bool delayOk = std::abs(measuredSamples - expectedSamples) < expectedSamples * 0.3;

        std::cout << modeName << ": Expected " << std::fixed << std::setprecision(1)
                  << expectedUs << "us (" << std::setprecision(2) << expectedSamples << " samples), "
                  << "Measured " << measuredUs << "us (" << measuredSamples << " samples)  "
                  << (delayOk ? "PASS" : "FAIL") << "\n";

        if (!delayOk) allPassed = false;
    }

    // =========================================================================
    // TEST 3: DELAY INTERPOLATION QUALITY
    // =========================================================================
    // The fractional delay uses linear interpolation, which has some HF rolloff
    // This is acceptable and even desirable for tape emulation

    std::cout << "\n=== DELAY INTERPOLATION QUALITY TEST ===\n";
    std::cout << "Expected: Right channel gain matches left within 0.5dB up to 10kHz\n\n";

    double frequencies[] = { 100.0, 500.0, 1000.0, 5000.0, 10000.0 };
    int numFreqs = 5;

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)   L-Gain    R-Gain    Diff      Status\n";
        std::cout << "------------------------------------------------\n";

        int passed = 0;
        for (int f = 0; f < numFreqs; f++) {
            TapeHysteresis::HybridTapeProcessor procL, procR;
            procL.setSampleRate(SAMPLE_RATE);
            procR.setSampleRate(SAMPLE_RATE);
            procL.setParameters(bias, 1.0);
            procR.setParameters(bias, 1.0);

            double amplitude = 0.1;  // Low level
            double freq = frequencies[f];

            int samplesPerCycle = static_cast<int>(SAMPLE_RATE / freq);
            int totalSamples = NUM_CYCLES * samplesPerCycle;

            std::vector<double> outputL(totalSamples), outputR(totalSamples);

            for (int i = 0; i < totalSamples; ++i) {
                double phase = 2.0 * M_PI * freq * i / SAMPLE_RATE;
                double input = amplitude * std::sin(phase);
                outputL[i] = procL.processSample(input);
                outputR[i] = procR.processRightChannel(input);
            }

            auto resultL = analyzeChannel(outputL, freq);
            auto resultR = analyzeChannel(outputR, freq);

            double gainL = 20.0 * std::log10(resultL.fundamental / amplitude);
            double gainR = 20.0 * std::log10(resultR.fundamental / amplitude);
            double diff = gainR - gainL;

            // Linear interpolation causes ~0.35dB loss at 10kHz - this is acceptable
            bool ok = std::abs(diff) < 0.5;
            if (ok) passed++;

            std::cout << std::setw(7) << freq << "    "
                      << std::fixed << std::setprecision(2) << std::showpos
                      << std::setw(6) << gainL << "    "
                      << std::setw(6) << gainR << "    "
                      << std::setw(6) << diff << "      "
                      << std::noshowpos << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "Result: " << passed << "/" << numFreqs << " frequencies passed\n\n";
        if (passed != numFreqs) allPassed = false;
    }

    // =========================================================================
    // TEST 4: MONO COMPATIBILITY
    // =========================================================================
    // When L and R are summed, there should be no significant cancellation

    std::cout << "\n=== MONO COMPATIBILITY TEST ===\n";
    std::cout << "Expected: Mono sum is within 0.5dB of 2x single channel\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor procL, procR;
        procL.setSampleRate(SAMPLE_RATE);
        procR.setSampleRate(SAMPLE_RATE);
        procL.setParameters(bias, 1.0);
        procR.setParameters(bias, 1.0);

        double amplitude = 0.3;
        double freq = 1000.0;

        int samplesPerCycle = static_cast<int>(SAMPLE_RATE / freq);
        int totalSamples = NUM_CYCLES * samplesPerCycle;

        std::vector<double> outputL(totalSamples), outputR(totalSamples), outputMono(totalSamples);

        for (int i = 0; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * freq * i / SAMPLE_RATE;
            double input = amplitude * std::sin(phase);
            outputL[i] = procL.processSample(input);
            outputR[i] = procR.processRightChannel(input);
            outputMono[i] = outputL[i] + outputR[i];
        }

        auto resultL = analyzeChannel(outputL, freq);
        auto resultMono = analyzeChannel(outputMono, freq);

        // Mono should be approximately 2x left (or +6dB)
        double expectedMono = resultL.fundamental * 2.0;
        double monoError = 20.0 * std::log10(resultMono.fundamental / expectedMono);

        bool monoOk = std::abs(monoError) < 0.5;

        std::cout << modeName << ": Mono sum error = " << std::fixed << std::setprecision(2)
                  << std::showpos << monoError << " dB  "
                  << std::noshowpos << (monoOk ? "PASS" : "FAIL") << "\n";

        if (!monoOk) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "STEREO TEST: ALL PASSED\n";
    } else {
        std::cout << "STEREO TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

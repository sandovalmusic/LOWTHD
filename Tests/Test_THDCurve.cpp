/**
 * Test_THDCurve.cpp
 *
 * Tests THD curve shape after Hermite spline implementation
 * Target: 2x THD per 3dB (cubic behavior)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/HybridTapeProcessor.cpp"
#include "../Source/DSP/BiasShielding.cpp"
#include "../Source/DSP/MachineEQ.cpp"

using namespace TapeHysteresis;

// Simple THD measurement
double measureTHD(HybridTapeProcessor& processor, double inputLevel, double sampleRate)
{
    const double freq = 100.0;  // 100Hz test tone
    const int numCycles = 50;
    const int samplesPerCycle = static_cast<int>(sampleRate / freq);
    const int totalSamples = numCycles * samplesPerCycle;
    const int measureCycles = 20;  // Last 20 cycles for measurement
    const int measureSamples = measureCycles * samplesPerCycle;

    processor.reset();

    // Run warmup cycles
    std::vector<double> output(totalSamples);
    for (int i = 0; i < totalSamples; ++i) {
        double input = inputLevel * std::sin(2.0 * M_PI * freq * i / sampleRate);
        output[i] = processor.processSample(input);
    }

    // FFT-like harmonic analysis using DFT at specific frequencies
    int measureStart = totalSamples - measureSamples;

    // Measure fundamental and harmonics
    std::vector<double> harmonicPower(10, 0.0);

    for (int h = 1; h <= 9; ++h) {
        double realSum = 0.0, imagSum = 0.0;
        double harmFreq = h * freq;

        for (int i = measureStart; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * harmFreq * (i - measureStart) / sampleRate;
            realSum += output[i] * std::cos(phase);
            imagSum += output[i] * std::sin(phase);
        }

        realSum /= measureSamples;
        imagSum /= measureSamples;
        harmonicPower[h] = std::sqrt(realSum * realSum + imagSum * imagSum) * 2.0;
    }

    // THD = sqrt(sum of harmonic powers) / fundamental
    double harmonicSum = 0.0;
    for (int h = 2; h <= 9; ++h) {
        harmonicSum += harmonicPower[h] * harmonicPower[h];
    }

    double thd = std::sqrt(harmonicSum) / harmonicPower[1] * 100.0;
    return thd;
}

int main()
{
    const double sampleRate = 96000.0;

    std::cout << "=== THD Curve Test (Hermite Spline Blend) ===" << std::endl;
    std::cout << std::endl;

    // Test levels
    std::vector<double> levels = {-12.0, -9.0, -6.0, -3.0, 0.0, 3.0, 6.0};

    // Test both machines
    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);

        if (machine == 0) {
            std::cout << "--- AMPEX ATR-102 (Master Mode) ---" << std::endl;
            std::cout << "Target: 2x THD per 3dB" << std::endl;
            processor.setParameters(0.5, 1.0);  // Ampex mode
        } else {
            std::cout << std::endl;
            std::cout << "--- STUDER A820 (Tracks Mode) ---" << std::endl;
            std::cout << "Target: 2x THD per 3dB" << std::endl;
            processor.setParameters(0.8, 1.0);  // Studer mode
        }

        std::cout << std::fixed << std::setprecision(4);
        std::cout << std::endl;
        std::cout << "Level(dB)  THD(%)    Ratio vs prev" << std::endl;
        std::cout << "--------------------------------" << std::endl;

        double prevTHD = 0.0;
        for (size_t i = 0; i < levels.size(); ++i) {
            double level = levels[i];
            double amplitude = std::pow(10.0, level / 20.0);
            double thd = measureTHD(processor, amplitude, sampleRate);

            std::cout << std::setw(6) << level << "     ";
            std::cout << std::setw(7) << thd << "   ";

            if (i > 0 && prevTHD > 0.001) {
                double ratio = thd / prevTHD;
                std::cout << std::setw(6) << ratio;
                // For cubic: expect ~2x per 3dB step, ~1.41x per 1.5dB
                if (ratio >= 1.8 && ratio <= 2.2) {
                    std::cout << " (good)";
                } else if (ratio >= 1.5 && ratio <= 2.5) {
                    std::cout << " (ok)";
                }
            }
            std::cout << std::endl;

            prevTHD = thd;
        }
    }

    std::cout << std::endl;
    std::cout << "Target ratios for cubic behavior:" << std::endl;
    std::cout << "  Per 3dB step: ~2.0x" << std::endl;
    std::cout << "  Per 6dB step: ~4.0x" << std::endl;

    return 0;
}

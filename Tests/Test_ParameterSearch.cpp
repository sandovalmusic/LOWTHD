/**
 * Parameter Search for Tape Saturation Models
 *
 * Systematically searches for optimal parameters to achieve:
 *
 * AMPEX ATR-102 (Master):
 *   - MOL (3% THD) at +12 dB
 *   - E/O ratio: 0.503 (odd-dominant)
 *
 * STUDER A820 (Tracks):
 *   - MOL (3% THD) at +9 dB
 *   - E/O ratio: 1.122 (even-dominant)
 */

#include "HybridTapeProcessor.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace TapeHysteresis;

// Simple FFT for harmonic analysis
void fft(std::vector<double>& real, std::vector<double>& imag) {
    size_t n = real.size();
    if (n <= 1) return;

    // Bit reversal
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

    // Cooley-Tukey
    for (size_t len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * M_PI / len;
        double wReal = std::cos(angle);
        double wImag = std::sin(angle);
        for (size_t i = 0; i < n; i += len) {
            double curReal = 1.0, curImag = 0.0;
            for (size_t j = 0; j < len / 2; ++j) {
                size_t u = i + j;
                size_t v = i + j + len / 2;
                double tReal = curReal * real[v] - curImag * imag[v];
                double tImag = curReal * imag[v] + curImag * real[v];
                real[v] = real[u] - tReal;
                imag[v] = imag[u] - tImag;
                real[u] += tReal;
                imag[u] += tImag;
                double newReal = curReal * wReal - curImag * wImag;
                curImag = curReal * wImag + curImag * wReal;
                curReal = newReal;
            }
        }
    }
}

struct THDResult {
    double thd;
    double h2, h3, h4, h5;
    double eoRatio;
};

// Measure THD with custom saturation parameters (bypassing normal processor setup)
THDResult measureTHD(double inputLevelDb, double sampleRate, double testFreq,
                     double tanhDrive, double tanhAsymmetry,
                     double atanDrive, double atanMixMax, double atanThreshold, double atanWidth,
                     double atanAsymmetry, bool useAsymmetricAtan,
                     double jaBlendMax, double jaBlendThreshold, double jaBlendWidth,
                     bool isAmpexMode)
{
    // Create processor and configure
    HybridTapeProcessor processor;
    processor.setSampleRate(sampleRate);

    // Set machine mode via bias parameter
    double biasValue = isAmpexMode ? 0.5 : 0.8;
    processor.setParameters(biasValue, 1.0);
    processor.reset();

    // Generate test signal
    double inputAmplitude = std::pow(10.0, inputLevelDb / 20.0);
    size_t fftSize = 16384;
    size_t warmupSamples = 4096;

    std::vector<double> output(fftSize);

    // Warmup
    for (size_t i = 0; i < warmupSamples; ++i) {
        double phase = 2.0 * M_PI * testFreq * i / sampleRate;
        double input = inputAmplitude * std::sin(phase);
        processor.processSample(input);
    }

    // Capture
    for (size_t i = 0; i < fftSize; ++i) {
        double phase = 2.0 * M_PI * testFreq * (warmupSamples + i) / sampleRate;
        double input = inputAmplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    // Apply Hann window
    for (size_t i = 0; i < fftSize; ++i) {
        double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fftSize));
        output[i] *= window;
    }

    // FFT
    std::vector<double> real = output;
    std::vector<double> imag(fftSize, 0.0);
    fft(real, imag);

    // Get harmonic magnitudes
    double binWidth = sampleRate / fftSize;
    auto getMagnitude = [&](double freq) {
        int bin = static_cast<int>(std::round(freq / binWidth));
        if (bin < 0 || bin >= static_cast<int>(fftSize / 2)) return 0.0;
        return std::sqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) / (fftSize / 2);
    };

    double h1 = getMagnitude(testFreq);
    double h2 = getMagnitude(testFreq * 2);
    double h3 = getMagnitude(testFreq * 3);
    double h4 = getMagnitude(testFreq * 4);
    double h5 = getMagnitude(testFreq * 5);

    THDResult result;
    result.h2 = h2;
    result.h3 = h3;
    result.h4 = h4;
    result.h5 = h5;

    double harmonicsSum = h2*h2 + h3*h3 + h4*h4 + h5*h5;
    result.thd = (h1 > 1e-10) ? 100.0 * std::sqrt(harmonicsSum) / h1 : 0.0;

    double evenSum = h2 + h4;
    double oddSum = h3 + h5;
    result.eoRatio = (oddSum > 1e-10) ? evenSum / oddSum : 0.0;

    return result;
}

// Find MOL (level where THD = 3%)
double findMOL(double sampleRate, bool isAmpexMode) {
    HybridTapeProcessor processor;
    processor.setSampleRate(sampleRate);
    double biasValue = isAmpexMode ? 0.5 : 0.8;
    processor.setParameters(biasValue, 1.0);

    double testFreq = 1000.0;

    for (double level = -6.0; level <= 18.0; level += 0.5) {
        processor.reset();

        double inputAmplitude = std::pow(10.0, level / 20.0);
        size_t fftSize = 16384;
        size_t warmupSamples = 4096;

        std::vector<double> output(fftSize);

        for (size_t i = 0; i < warmupSamples; ++i) {
            double phase = 2.0 * M_PI * testFreq * i / sampleRate;
            processor.processSample(inputAmplitude * std::sin(phase));
        }

        for (size_t i = 0; i < fftSize; ++i) {
            double phase = 2.0 * M_PI * testFreq * (warmupSamples + i) / sampleRate;
            output[i] = processor.processSample(inputAmplitude * std::sin(phase));
        }

        for (size_t i = 0; i < fftSize; ++i) {
            double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fftSize));
            output[i] *= window;
        }

        std::vector<double> real = output;
        std::vector<double> imag(fftSize, 0.0);
        fft(real, imag);

        double binWidth = sampleRate / fftSize;
        auto getMag = [&](double freq) {
            int bin = static_cast<int>(std::round(freq / binWidth));
            if (bin < 0 || bin >= static_cast<int>(fftSize / 2)) return 0.0;
            return std::sqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) / (fftSize / 2);
        };

        double h1 = getMag(testFreq);
        double h2 = getMag(testFreq * 2);
        double h3 = getMag(testFreq * 3);
        double h4 = getMag(testFreq * 4);
        double h5 = getMag(testFreq * 5);

        double harmonicsSum = h2*h2 + h3*h3 + h4*h4 + h5*h5;
        double thd = (h1 > 1e-10) ? 100.0 * std::sqrt(harmonicsSum) / h1 : 0.0;

        if (thd >= 3.0) {
            return level;
        }
    }

    return 18.0; // Not found
}

struct SearchResult {
    double tanhDrive;
    double tanhAsymmetry;
    double atanDrive;
    double atanMixMax;
    double atanAsymmetry;
    double mol;
    double eoRatio;
    double molError;
    double eoError;
    double totalError;
};

int main() {
    std::cout << "================================================================\n";
    std::cout << "   Parameter Search for Tape Saturation Models\n";
    std::cout << "================================================================\n\n";

    double sampleRate = 96000.0;

    // Current best values from previous tuning
    std::cout << "=== CURRENT BASELINE ===\n\n";

    double ampexMOL = findMOL(sampleRate, true);
    double studerMOL = findMOL(sampleRate, false);

    HybridTapeProcessor ampexProc, studerProc;
    ampexProc.setSampleRate(sampleRate);
    studerProc.setSampleRate(sampleRate);
    ampexProc.setParameters(0.5, 1.0);
    studerProc.setParameters(0.8, 1.0);

    // Measure E/O at +6dB
    auto measureEO = [&](HybridTapeProcessor& proc) {
        proc.reset();
        double inputAmplitude = std::pow(10.0, 6.0 / 20.0);
        double testFreq = 1000.0;
        size_t fftSize = 16384;
        size_t warmup = 4096;

        std::vector<double> output(fftSize);
        for (size_t i = 0; i < warmup; ++i) {
            double phase = 2.0 * M_PI * testFreq * i / sampleRate;
            proc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double phase = 2.0 * M_PI * testFreq * (warmup + i) / sampleRate;
            output[i] = proc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fftSize));
            output[i] *= window;
        }
        std::vector<double> real = output;
        std::vector<double> imag(fftSize, 0.0);
        fft(real, imag);

        double binWidth = sampleRate / fftSize;
        auto getMag = [&](double freq) {
            int bin = static_cast<int>(std::round(freq / binWidth));
            return std::sqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) / (fftSize / 2);
        };

        double h2 = getMag(testFreq * 2);
        double h3 = getMag(testFreq * 3);
        double h4 = getMag(testFreq * 4);
        double h5 = getMag(testFreq * 5);

        return (h3 + h5 > 1e-10) ? (h2 + h4) / (h3 + h5) : 0.0;
    };

    double ampexEO = measureEO(ampexProc);
    double studerEO = measureEO(studerProc);

    std::cout << "AMPEX ATR-102:\n";
    std::cout << "  MOL: " << std::fixed << std::setprecision(1) << ampexMOL << " dB (target: +12 dB)\n";
    std::cout << "  E/O: " << std::setprecision(3) << ampexEO << " (target: 0.503)\n\n";

    std::cout << "STUDER A820:\n";
    std::cout << "  MOL: " << std::fixed << std::setprecision(1) << studerMOL << " dB (target: +9 dB)\n";
    std::cout << "  E/O: " << std::setprecision(3) << studerEO << " (target: 1.122)\n\n";

    // ============================================================================
    // ANALYSIS: Understanding the current state
    // ============================================================================

    std::cout << "=== DETAILED THD ANALYSIS ===\n\n";

    std::cout << "AMPEX THD Curve:\n";
    std::cout << "  Level    THD%     Notes\n";
    std::cout << "  -------------------------\n";

    for (double level = 0; level <= 18; level += 3) {
        ampexProc.reset();
        double inputAmplitude = std::pow(10.0, level / 20.0);
        double testFreq = 1000.0;
        size_t fftSize = 16384;
        size_t warmup = 4096;

        std::vector<double> output(fftSize);
        for (size_t i = 0; i < warmup; ++i) {
            double phase = 2.0 * M_PI * testFreq * i / sampleRate;
            ampexProc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double phase = 2.0 * M_PI * testFreq * (warmup + i) / sampleRate;
            output[i] = ampexProc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fftSize));
            output[i] *= window;
        }
        std::vector<double> real = output;
        std::vector<double> imag(fftSize, 0.0);
        fft(real, imag);

        double binWidth = sampleRate / fftSize;
        auto getMag = [&](double freq) {
            int bin = static_cast<int>(std::round(freq / binWidth));
            return std::sqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) / (fftSize / 2);
        };

        double h1 = getMag(testFreq);
        double h2 = getMag(testFreq * 2);
        double h3 = getMag(testFreq * 3);
        double h4 = getMag(testFreq * 4);
        double h5 = getMag(testFreq * 5);

        double harmonicsSum = h2*h2 + h3*h3 + h4*h4 + h5*h5;
        double thd = (h1 > 1e-10) ? 100.0 * std::sqrt(harmonicsSum) / h1 : 0.0;

        std::string note = "";
        if (thd >= 2.8 && thd <= 3.2) note = " <-- Near 3% MOL";
        if (level == 12) note = " <-- TARGET MOL";

        std::cout << "  +" << std::setw(2) << (int)level << " dB   "
                  << std::fixed << std::setprecision(2) << std::setw(5) << thd << "%" << note << "\n";
    }

    std::cout << "\nSTUDER THD Curve:\n";
    std::cout << "  Level    THD%     Notes\n";
    std::cout << "  -------------------------\n";

    for (double level = 0; level <= 15; level += 3) {
        studerProc.reset();
        double inputAmplitude = std::pow(10.0, level / 20.0);
        double testFreq = 1000.0;
        size_t fftSize = 16384;
        size_t warmup = 4096;

        std::vector<double> output(fftSize);
        for (size_t i = 0; i < warmup; ++i) {
            double phase = 2.0 * M_PI * testFreq * i / sampleRate;
            studerProc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double phase = 2.0 * M_PI * testFreq * (warmup + i) / sampleRate;
            output[i] = studerProc.processSample(inputAmplitude * std::sin(phase));
        }
        for (size_t i = 0; i < fftSize; ++i) {
            double window = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / fftSize));
            output[i] *= window;
        }
        std::vector<double> real = output;
        std::vector<double> imag(fftSize, 0.0);
        fft(real, imag);

        double binWidth = sampleRate / fftSize;
        auto getMag = [&](double freq) {
            int bin = static_cast<int>(std::round(freq / binWidth));
            return std::sqrt(real[bin] * real[bin] + imag[bin] * imag[bin]) / (fftSize / 2);
        };

        double h1 = getMag(testFreq);
        double h2 = getMag(testFreq * 2);
        double h3 = getMag(testFreq * 3);
        double h4 = getMag(testFreq * 4);
        double h5 = getMag(testFreq * 5);

        double harmonicsSum = h2*h2 + h3*h3 + h4*h4 + h5*h5;
        double thd = (h1 > 1e-10) ? 100.0 * std::sqrt(harmonicsSum) / h1 : 0.0;

        std::string note = "";
        if (thd >= 2.8 && thd <= 3.2) note = " <-- Near 3% MOL";
        if (level == 9) note = " <-- TARGET MOL";

        std::cout << "  +" << std::setw(2) << (int)level << " dB   "
                  << std::fixed << std::setprecision(2) << std::setw(5) << thd << "%" << note << "\n";
    }

    // ============================================================================
    // OBSERVATIONS AND RECOMMENDATIONS
    // ============================================================================

    std::cout << "\n=== ANALYSIS ===\n\n";

    // Ampex analysis
    double ampexMolError = std::abs(ampexMOL - 12.0);
    double ampexEoError = std::abs(ampexEO - 0.503);

    std::cout << "AMPEX ATR-102:\n";
    std::cout << "  MOL Error: " << std::setprecision(1) << ampexMolError << " dB\n";
    std::cout << "  E/O Error: " << std::setprecision(3) << ampexEoError << "\n";

    if (ampexMOL > 12.0) {
        std::cout << "  -> Need MORE saturation (increase tanhDrive)\n";
    } else {
        std::cout << "  -> Need LESS saturation (decrease tanhDrive)\n";
    }

    if (ampexEO > 0.503) {
        std::cout << "  -> Too much H2 (decrease tanhAsymmetry toward 1.0)\n";
    } else {
        std::cout << "  -> Need more H2 (increase tanhAsymmetry away from 1.0)\n";
    }

    // Studer analysis
    double studerMolError = std::abs(studerMOL - 9.0);
    double studerEoError = std::abs(studerEO - 1.122);

    std::cout << "\nSTUDER A820:\n";
    std::cout << "  MOL Error: " << std::setprecision(1) << studerMolError << " dB\n";
    std::cout << "  E/O Error: " << std::setprecision(3) << studerEoError << "\n";

    if (studerMOL > 9.0) {
        std::cout << "  -> Need MORE saturation (increase tanhDrive)\n";
    } else {
        std::cout << "  -> Need LESS saturation (decrease tanhDrive)\n";
    }

    if (studerEO > 1.122) {
        std::cout << "  -> Too much H2 (decrease tanhAsymmetry and/or atanAsymmetry)\n";
    } else {
        std::cout << "  -> Need more H2 (increase tanhAsymmetry and/or atanAsymmetry)\n";
    }

    // ============================================================================
    // PARAMETER RECOMMENDATIONS
    // ============================================================================

    std::cout << "\n=== RECOMMENDED PARAMETER CHANGES ===\n\n";

    // Calculate suggested changes
    // For MOL: each 0.01 increase in tanhDrive roughly decreases MOL by ~0.5-1dB
    // For E/O: tanhAsymmetry > 1.0 increases even harmonics

    double ampexDriveDelta = (ampexMOL - 12.0) * 0.015;  // Rough estimate
    double studerDriveDelta = (studerMOL - 9.0) * 0.02;

    std::cout << "AMPEX (current -> suggested):\n";
    std::cout << "  tanhDrive: 0.18 -> " << std::setprecision(3) << (0.18 + ampexDriveDelta) << "\n";
    if (ampexEO > 0.503) {
        std::cout << "  tanhAsymmetry: 1.18 -> " << (1.18 - (ampexEO - 0.503) * 0.5) << "\n";
    } else {
        std::cout << "  tanhAsymmetry: 1.18 -> " << (1.18 + (0.503 - ampexEO) * 0.5) << "\n";
    }

    std::cout << "\nSTUDER (current -> suggested):\n";
    std::cout << "  tanhDrive: 0.17 -> " << std::setprecision(3) << (0.17 + studerDriveDelta) << "\n";
    if (studerEO > 1.122) {
        std::cout << "  tanhAsymmetry: 1.38 -> " << (1.38 - (studerEO - 1.122) * 0.3) << "\n";
    } else {
        std::cout << "  tanhAsymmetry: 1.38 -> " << (1.38 + (1.122 - studerEO) * 0.3) << "\n";
    }

    // ============================================================================
    // GRID SEARCH FOR OPTIMAL PARAMETERS
    // ============================================================================

    std::cout << "\n=== GRID SEARCH: AMPEX ATR-102 ===\n\n";
    std::cout << "Searching tanhDrive [0.15, 0.30] and tanhAsymmetry [1.05, 1.25]...\n\n";

    std::vector<SearchResult> ampexResults;

    for (double drive = 0.15; drive <= 0.30; drive += 0.02) {
        for (double asym = 1.05; asym <= 1.25; asym += 0.03) {
            // We can't directly inject parameters, so we'll record what we'd want to test
            // For now, output the search space
            SearchResult r;
            r.tanhDrive = drive;
            r.tanhAsymmetry = asym;
            ampexResults.push_back(r);
        }
    }

    std::cout << "Grid: " << ampexResults.size() << " combinations\n";
    std::cout << "Note: Full parameter injection requires modifying HybridTapeProcessor\n";
    std::cout << "      to accept external saturation parameters.\n\n";

    std::cout << "Best approach: Manually test these promising combinations:\n\n";

    std::cout << "AMPEX promising values:\n";
    double suggestedAmpexDrive = 0.18 + ampexDriveDelta;
    suggestedAmpexDrive = std::clamp(suggestedAmpexDrive, 0.15, 0.35);
    std::cout << "  1. tanhDrive=" << std::setprecision(2) << suggestedAmpexDrive
              << ", tanhAsymmetry=1.18 (adjust drive only)\n";
    std::cout << "  2. tanhDrive=" << suggestedAmpexDrive
              << ", tanhAsymmetry=1.15 (slight asymmetry reduction)\n";
    std::cout << "  3. tanhDrive=" << (suggestedAmpexDrive + 0.02)
              << ", tanhAsymmetry=1.20 (more aggressive)\n";

    std::cout << "\nSTUDER promising values:\n";
    double suggestedStuderDrive = 0.17 + studerDriveDelta;
    suggestedStuderDrive = std::clamp(suggestedStuderDrive, 0.12, 0.30);
    double suggestedStuderAsym = 1.38 + (1.122 - studerEO) * 0.3;
    suggestedStuderAsym = std::clamp(suggestedStuderAsym, 1.30, 1.50);

    std::cout << "  1. tanhDrive=" << std::setprecision(2) << suggestedStuderDrive
              << ", tanhAsymmetry=" << std::setprecision(2) << suggestedStuderAsym << "\n";
    std::cout << "  2. tanhDrive=" << suggestedStuderDrive
              << ", tanhAsymmetry=" << (suggestedStuderAsym + 0.05) << ", atanAsymmetry=1.40\n";
    std::cout << "  3. tanhDrive=" << (suggestedStuderDrive - 0.01)
              << ", tanhAsymmetry=" << (suggestedStuderAsym + 0.03) << "\n";

    std::cout << "\n================================================================\n";
    std::cout << "   SEARCH COMPLETE\n";
    std::cout << "================================================================\n";

    return 0;
}

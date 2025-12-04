/**
 * Realism Analysis for Tape Saturation Models
 *
 * Compares our implementation against known real-world tape behavior:
 * 1. THD curve shape (should follow specific exponential curve)
 * 2. E/O ratio vs level (how does harmonic balance change with level?)
 * 3. Frequency response flatness at low levels
 * 4. Phase response characteristics
 * 5. Transient behavior (attack/release of saturation)
 */

#include "HybridTapeProcessor.h"
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace TapeHysteresis;

// Simple FFT for harmonic analysis
void fft(std::vector<double>& real, std::vector<double>& imag) {
    size_t n = real.size();
    if (n <= 1) return;

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
    }

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

struct HarmonicAnalysis {
    double thd;
    double h1, h2, h3, h4, h5, h6, h7;
    double eoRatio;
    double h2h3Ratio;
};

HarmonicAnalysis analyzeHarmonics(HybridTapeProcessor& processor, double inputLevelDb,
                                   double sampleRate, double testFreq = 1000.0) {
    processor.reset();

    double inputAmplitude = std::pow(10.0, inputLevelDb / 20.0);
    size_t fftSize = 32768;  // Higher resolution
    size_t warmupSamples = 8192;

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

    HarmonicAnalysis result;
    result.h1 = getMag(testFreq);
    result.h2 = getMag(testFreq * 2);
    result.h3 = getMag(testFreq * 3);
    result.h4 = getMag(testFreq * 4);
    result.h5 = getMag(testFreq * 5);
    result.h6 = getMag(testFreq * 6);
    result.h7 = getMag(testFreq * 7);

    double harmonicsSum = result.h2*result.h2 + result.h3*result.h3 +
                          result.h4*result.h4 + result.h5*result.h5 +
                          result.h6*result.h6 + result.h7*result.h7;
    result.thd = (result.h1 > 1e-10) ? 100.0 * std::sqrt(harmonicsSum) / result.h1 : 0.0;

    double evenSum = result.h2 + result.h4 + result.h6;
    double oddSum = result.h3 + result.h5 + result.h7;
    result.eoRatio = (oddSum > 1e-10) ? evenSum / oddSum : 0.0;
    result.h2h3Ratio = (result.h3 > 1e-10) ? result.h2 / result.h3 : 0.0;

    return result;
}

double measureGain(HybridTapeProcessor& processor, double inputLevelDb,
                   double sampleRate, double testFreq = 1000.0) {
    processor.reset();

    double inputAmplitude = std::pow(10.0, inputLevelDb / 20.0);
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

    // Find peak
    double peak = 0.0;
    for (size_t i = 0; i < fftSize; ++i) {
        peak = std::max(peak, std::abs(output[i]));
    }

    return 20.0 * std::log10(peak / inputAmplitude);
}

int main() {
    double sampleRate = 96000.0;

    std::cout << "================================================================\n";
    std::cout << "   Realism Analysis\n";
    std::cout << "================================================================\n\n";

    // =========================================================================
    // 1. THD CURVE SHAPE ANALYSIS
    // =========================================================================
    std::cout << "=== 1. THD CURVE SHAPE ===\n\n";
    std::cout << "Real tape THD follows approximately: THD = k * 10^(level/20)^n\n";
    std::cout << "where n is typically 2-3 (quadratic to cubic rise)\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);
        processor.setParameters(machine == 0 ? 0.5 : 0.8, 1.0);

        std::cout << (machine == 0 ? "AMPEX ATR-102:\n" : "STUDER A820:\n");
        std::cout << "Level     THD%      THD Ratio (vs -6dB)    Expected 2x/level\n";
        std::cout << "----------------------------------------------------------------\n";

        double baseTHD = 0.0;
        for (double level = -12; level <= 12; level += 3) {
            auto result = analyzeHarmonics(processor, level, sampleRate);

            if (level == -6) baseTHD = result.thd;

            double ratio = (baseTHD > 0.001) ? result.thd / baseTHD : 0.0;
            double expectedRatio = std::pow(10.0, (level - (-6)) / 20.0);  // Linear expectation
            expectedRatio = expectedRatio * expectedRatio;  // Quadratic (real tape)

            std::cout << std::setw(4) << std::showpos << (int)level << " dB   "
                      << std::noshowpos << std::fixed << std::setprecision(3)
                      << std::setw(7) << result.thd << "%   "
                      << std::setw(8) << std::setprecision(2) << ratio << "x            "
                      << std::setw(6) << expectedRatio << "x\n";
        }
        std::cout << "\n";
    }

    // =========================================================================
    // 2. E/O RATIO VS LEVEL
    // =========================================================================
    std::cout << "=== 2. E/O RATIO VS LEVEL ===\n\n";
    std::cout << "Real tape: E/O should be relatively constant across levels\n";
    std::cout << "(The asymmetry is in the tape, not the signal level)\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);
        processor.setParameters(machine == 0 ? 0.5 : 0.8, 1.0);

        double targetEO = machine == 0 ? 0.503 : 1.122;

        std::cout << (machine == 0 ? "AMPEX ATR-102 (target E/O = 0.503):\n" :
                                     "STUDER A820 (target E/O = 1.122):\n");
        std::cout << "Level     E/O Ratio    H2/H3      Deviation from target\n";
        std::cout << "-----------------------------------------------------------\n";

        for (double level = -6; level <= 12; level += 3) {
            auto result = analyzeHarmonics(processor, level, sampleRate);
            double deviation = result.eoRatio - targetEO;

            std::cout << std::setw(4) << std::showpos << (int)level << " dB   "
                      << std::noshowpos << std::fixed << std::setprecision(3)
                      << std::setw(8) << result.eoRatio << "      "
                      << std::setw(6) << result.h2h3Ratio << "      "
                      << std::showpos << std::setprecision(3) << deviation << "\n";
        }
        std::cout << std::noshowpos << "\n";
    }

    // =========================================================================
    // 3. FREQUENCY RESPONSE AT LOW LEVEL
    // =========================================================================
    std::cout << "=== 3. FREQUENCY RESPONSE (Low Level, -12dB) ===\n\n";
    std::cout << "Should be flat except for Machine EQ (head bump)\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);
        processor.setParameters(machine == 0 ? 0.5 : 0.8, 1.0);

        std::cout << (machine == 0 ? "AMPEX ATR-102:\n" : "STUDER A820:\n");
        std::cout << "Freq (Hz)    Gain (dB)    Note\n";
        std::cout << "----------------------------------------\n";

        double refGain = measureGain(processor, -12.0, sampleRate, 1000.0);

        std::vector<double> freqs = {30, 50, 100, 200, 500, 1000, 2000, 4000, 8000, 12000, 16000};
        for (double freq : freqs) {
            if (freq > sampleRate / 2.5) continue;

            double gain = measureGain(processor, -12.0, sampleRate, freq);
            double relGain = gain - refGain;

            std::string note = "";
            if (freq >= 30 && freq <= 120) note = "(head bump region)";
            if (freq >= 8000) note = "(HF region)";

            std::cout << std::setw(6) << (int)freq << "       "
                      << std::showpos << std::fixed << std::setprecision(2)
                      << std::setw(6) << relGain << "      "
                      << std::noshowpos << note << "\n";
        }
        std::cout << "\n";
    }

    // =========================================================================
    // 4. GAIN COMPRESSION AT HIGH LEVELS
    // =========================================================================
    std::cout << "=== 4. GAIN COMPRESSION (Saturation Behavior) ===\n\n";
    std::cout << "Real tape: ~0.5-1dB compression at MOL\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);
        processor.setParameters(machine == 0 ? 0.5 : 0.8, 1.0);

        std::cout << (machine == 0 ? "AMPEX ATR-102:\n" : "STUDER A820:\n");
        std::cout << "Input Level    Output Gain    Compression\n";
        std::cout << "---------------------------------------------\n";

        double refGain = measureGain(processor, -12.0, sampleRate);

        for (double level = -12; level <= 15; level += 3) {
            double gain = measureGain(processor, level, sampleRate);
            double compression = refGain - gain;

            std::cout << std::setw(6) << std::showpos << (int)level << " dB       "
                      << std::noshowpos << std::fixed << std::setprecision(2)
                      << std::setw(6) << gain << " dB       "
                      << std::setw(5) << compression << " dB\n";
        }
        std::cout << "\n";
    }

    // =========================================================================
    // 5. HARMONIC STRUCTURE DETAIL
    // =========================================================================
    std::cout << "=== 5. HARMONIC STRUCTURE @ +6dB ===\n\n";
    std::cout << "Real tape harmonic decay: each harmonic ~6-10dB below previous\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        HybridTapeProcessor processor;
        processor.setSampleRate(sampleRate);
        processor.setParameters(machine == 0 ? 0.5 : 0.8, 1.0);

        auto result = analyzeHarmonics(processor, 6.0, sampleRate);

        std::cout << (machine == 0 ? "AMPEX ATR-102:\n" : "STUDER A820:\n");
        std::cout << "Harmonic    Level (dB rel H1)    Decay from previous\n";
        std::cout << "-------------------------------------------------------\n";

        double h1dB = 20.0 * std::log10(result.h1 + 1e-10);
        double prevdB = h1dB;

        auto printHarmonic = [&](const char* name, double level) {
            double dB = 20.0 * std::log10(level + 1e-10);
            double relH1 = dB - h1dB;
            double decay = prevdB - dB;
            std::cout << "   " << name << "         "
                      << std::fixed << std::setprecision(1) << std::setw(7) << relH1
                      << " dB          " << std::setw(5) << decay << " dB\n";
            prevdB = dB;
        };

        std::cout << "   H1          0.0 dB          (reference)\n";
        prevdB = h1dB;
        printHarmonic("H2", result.h2);
        printHarmonic("H3", result.h3);
        printHarmonic("H4", result.h4);
        printHarmonic("H5", result.h5);
        printHarmonic("H6", result.h6);
        printHarmonic("H7", result.h7);
        std::cout << "\n";
    }

    // =========================================================================
    // 6. OBSERVATIONS
    // =========================================================================
    std::cout << "=== 6. ANALYSIS SUMMARY ===\n\n";
    std::cout << "Key things to check:\n";
    std::cout << "  1. THD curve: Is the rise rate realistic (quadratic-ish)?\n";
    std::cout << "  2. E/O ratio: Does it stay consistent across levels?\n";
    std::cout << "  3. Freq response: Is head bump accurate? HF rolloff correct?\n";
    std::cout << "  4. Compression: Is it subtle (~1dB) or too aggressive?\n";
    std::cout << "  5. Harmonic decay: Natural rolloff (~6-10dB per harmonic)?\n";

    std::cout << "\n================================================================\n";

    return 0;
}

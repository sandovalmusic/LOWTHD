#include <iostream>
#include <cmath>
#include <vector>
#include "../Source/DSP/HybridTapeProcessor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace TapeHysteresis;

double measureTHD(const std::vector<double>& signal, double sampleRate, double testFreq,
                   double* h2 = nullptr, double* h3 = nullptr) {
    int N = signal.size();
    int warmup = N / 3;
    int measureN = N - warmup;

    double harmonics[6] = {0};
    for (int h = 1; h <= 5; ++h) {
        double freq = testFreq * h;
        if (freq > sampleRate / 2.0) break;
        double sumCos = 0, sumSin = 0;
        for (int i = warmup; i < N; ++i) {
            double t = i / sampleRate;
            sumCos += signal[i] * std::cos(2.0 * M_PI * freq * t);
            sumSin += signal[i] * std::sin(2.0 * M_PI * freq * t);
        }
        harmonics[h] = 2.0 * std::sqrt(sumCos*sumCos + sumSin*sumSin) / measureN;
    }

    if (h2) *h2 = harmonics[2];
    if (h3) *h3 = harmonics[3];

    double sum = 0;
    for (int h = 2; h <= 5; ++h) sum += harmonics[h] * harmonics[h];
    return 100.0 * std::sqrt(sum) / harmonics[1];
}

int main() {
    double sampleRate = 96000.0;
    double testFreq = 100.0;

    std::cout << "=== STUDER A820 Tuning (100Hz) ===\n\n";
    std::cout << "Targets:\n";
    std::cout << "  - THD @ -6dB: 0.07%\n";
    std::cout << "  - THD @ 0dB: 0.25%\n";
    std::cout << "  - THD @ +6dB: 1.25%\n";
    std::cout << "  - MOL (3% THD): +9dB\n";
    std::cout << "  - E/O ratio: ~1.12 (even-dominant)\n\n";

    double testLevels[] = {-12.0, -6.0, 0.0, 3.0, 6.0, 9.0, 12.0, 15.0};

    HybridTapeProcessor processor;
    processor.setSampleRate(sampleRate);
    processor.setParameters(0.8, 1.0);  // Studer mode (bias >= 0.74)

    std::cout << "Level      THD%       H2/H3     Target      Status\n";
    std::cout << "--------------------------------------------------------\n";

    for (double levelDb : testLevels) {
        double amplitude = std::pow(10.0, levelDb / 20.0);

        int numCycles = 300;
        int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
        int N = numCycles * samplesPerCycle;

        std::vector<double> output(N);
        processor.reset();

        for (int i = 0; i < N; ++i) {
            double t = i / sampleRate;
            double input = amplitude * std::sin(2.0 * M_PI * testFreq * t);
            output[i] = processor.processSample(input);
        }

        double h2, h3;
        double thd = measureTHD(output, sampleRate, testFreq, &h2, &h3);
        double eoRatio = (h3 > 0.0001) ? h2 / h3 : 0;

        const char* target = "";
        const char* status = "";

        if (levelDb == -6.0) {
            target = "0.07%";
            if (thd >= 0.05 && thd <= 0.10) status = "OK";
        } else if (levelDb == 0.0) {
            target = "0.25%";
            if (thd >= 0.20 && thd <= 0.30) status = "OK";
        } else if (levelDb == 6.0) {
            target = "1.25%";
            if (thd >= 1.0 && thd <= 1.5) status = "OK";
        } else if (levelDb == 9.0) {
            target = "3.0%";
            if (thd >= 2.5 && thd <= 3.5) status = "MOL";
        }

        printf("  %+3.0f dB    %.3f%%      %.2f      %s      %s\n", levelDb, thd, eoRatio, target, status);
    }

    std::cout << "\n";
    {
        double amplitude = std::pow(10.0, 6.0 / 20.0);
        int numCycles = 300;
        int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
        int N = numCycles * samplesPerCycle;
        std::vector<double> output(N);
        processor.reset();
        for (int i = 0; i < N; ++i) {
            double t = i / sampleRate;
            double input = amplitude * std::sin(2.0 * M_PI * testFreq * t);
            output[i] = processor.processSample(input);
        }
        double h2, h3;
        measureTHD(output, sampleRate, testFreq, &h2, &h3);
        double eoRatio = h2 / h3;
        std::cout << "E/O ratio @ +6dB: " << eoRatio << " (target: ~1.12)\n";
        if (eoRatio >= 0.9 && eoRatio <= 1.4) std::cout << "  OK - Even-dominant as expected\n";
        else if (eoRatio < 0.9) std::cout << "  Need more even harmonics\n";
        else std::cout << "  Too many even harmonics\n";
    }

    return 0;
}

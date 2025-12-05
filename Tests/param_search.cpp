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

struct THDResult {
    double thd_m12, thd_m6, thd_0, thd_3, thd_6;
    double eo_6;
};

THDResult measureMachine(bool isAmpex) {
    double sampleRate = 96000.0;
    double testFreq = 100.0;
    
    HybridTapeProcessor processor;
    processor.setSampleRate(sampleRate);
    processor.setParameters(isAmpex ? 0.5 : 0.8, 1.0);
    
    THDResult result;
    double levels[] = {-12.0, -6.0, 0.0, 3.0, 6.0};
    double* results[] = {&result.thd_m12, &result.thd_m6, &result.thd_0, &result.thd_3, &result.thd_6};
    
    for (int i = 0; i < 5; ++i) {
        double amplitude = std::pow(10.0, levels[i] / 20.0);
        int N = 300 * static_cast<int>(sampleRate / testFreq);
        std::vector<double> output(N);
        processor.reset();
        
        for (int j = 0; j < N; ++j) {
            double t = j / sampleRate;
            output[j] = processor.processSample(amplitude * std::sin(2.0 * M_PI * testFreq * t));
        }
        
        double h2, h3;
        *results[i] = measureTHD(output, sampleRate, testFreq, &h2, &h3);
        if (i == 4) result.eo_6 = (h3 > 0.0001) ? h2 / h3 : 0;
    }
    
    return result;
}

int main() {
    std::cout << "=== Current Measurements ===\n\n";
    
    // Ampex targets: -6dB=0.02%, 0dB=0.08%, +6dB=0.40%, E/O=0.5
    // Studer targets: -6dB=0.07%, 0dB=0.25%, +6dB=1.25%, E/O=1.12
    
    auto ampex = measureMachine(true);
    auto studer = measureMachine(false);
    
    std::cout << "AMPEX ATR-102:\n";
    std::cout << "  Level     Actual    Target    Error\n";
    printf("  -12dB    %6.3f%%   ~0.005%%\n", ampex.thd_m12);
    printf("   -6dB    %6.3f%%    0.02%%   %+.1f%%\n", ampex.thd_m6, 100*(ampex.thd_m6 - 0.02)/0.02);
    printf("    0dB    %6.3f%%    0.08%%   %+.1f%%\n", ampex.thd_0, 100*(ampex.thd_0 - 0.08)/0.08);
    printf("   +3dB    %6.3f%%\n", ampex.thd_3);
    printf("   +6dB    %6.3f%%    0.40%%   %+.1f%%\n", ampex.thd_6, 100*(ampex.thd_6 - 0.40)/0.40);
    printf("   E/O     %6.2f      0.50    %+.1f%%\n", ampex.eo_6, 100*(ampex.eo_6 - 0.5)/0.5);
    
    std::cout << "\nSTUDER A820:\n";
    std::cout << "  Level     Actual    Target    Error\n";
    printf("  -12dB    %6.3f%%   ~0.02%%\n", studer.thd_m12);
    printf("   -6dB    %6.3f%%    0.07%%   %+.1f%%\n", studer.thd_m6, 100*(studer.thd_m6 - 0.07)/0.07);
    printf("    0dB    %6.3f%%    0.25%%   %+.1f%%\n", studer.thd_0, 100*(studer.thd_0 - 0.25)/0.25);
    printf("   +3dB    %6.3f%%\n", studer.thd_3);
    printf("   +6dB    %6.3f%%    1.25%%   %+.1f%%\n", studer.thd_6, 100*(studer.thd_6 - 1.25)/1.25);
    printf("   E/O     %6.2f      1.12    %+.1f%%\n", studer.eo_6, 100*(studer.eo_6 - 1.12)/1.12);
    
    // Cubic curve check: THD should roughly double every 3dB (factor of sqrt(2) in amplitude)
    // i.e., THD(+3dB) / THD(0dB) should be ~2, THD(+6dB) / THD(+3dB) should be ~2
    std::cout << "\n=== Curve Shape Analysis (target ratio ~2x per 3dB) ===\n";
    printf("AMPEX:  -6→0dB: %.1fx   0→+3dB: %.1fx   +3→+6dB: %.1fx\n", 
           ampex.thd_0/ampex.thd_m6, ampex.thd_3/ampex.thd_0, ampex.thd_6/ampex.thd_3);
    printf("STUDER: -6→0dB: %.1fx   0→+3dB: %.1fx   +3→+6dB: %.1fx\n", 
           studer.thd_0/studer.thd_m6, studer.thd_3/studer.thd_0, studer.thd_6/studer.thd_3);
    
    return 0;
}

#include <iostream>
#include <cmath>
#include <vector>
#include "../Source/DSP/BiasShielding.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace TapeHysteresis;

// Test if the parallel path structure (HFCut + cleanHF) introduces phase issues
// If input = HFCut(input) + (input - HFCut(input)), does it null perfectly?

double measureAmplitude(const std::vector<double>& signal, double sampleRate, double testFreq, int warmup) {
    int N = signal.size();
    int measureN = N - warmup;

    double sumCos = 0, sumSin = 0;
    for (int i = warmup; i < N; ++i) {
        double t = i / sampleRate;
        sumCos += signal[i] * std::cos(2.0 * M_PI * testFreq * t);
        sumSin += signal[i] * std::sin(2.0 * M_PI * testFreq * t);
    }
    double amplitude = 2.0 * std::sqrt(sumCos*sumCos + sumSin*sumSin) / measureN;
    if (amplitude < 1e-15) amplitude = 1e-15;
    return amplitude;
}

int main() {
    double sampleRate = 96000.0;

    std::cout << "=== Parallel Path Phase Analysis ===\n\n";
    std::cout << "Testing: input = HFCut(input) + (input - HFCut(input))\n";
    std::cout << "If phase is matched, this should null perfectly (0dB deviation).\n\n";

    double testFreqs[] = {100, 500, 1000, 2000, 4000, 6000, 8000, 10000, 12000, 15000, 18000, 20000};

    for (int machine = 0; machine < 2; ++machine) {
        bool isAmpex = (machine == 0);
        std::cout << (isAmpex ? "AMPEX ATR-102:\n" : "\nSTUDER A820:\n");
        std::cout << "Freq (Hz)    Input    Reconstructed    Deviation (dB)    HFCut dB    CleanHF dB\n";
        std::cout << "---------------------------------------------------------------------------------\n";

        for (double freq : testFreqs) {
            HFCut hfCut;
            hfCut.setSampleRate(sampleRate);
            hfCut.setMachineMode(isAmpex);

            int numCycles = (freq < 200) ? 500 : 200;
            int samplesPerCycle = static_cast<int>(sampleRate / freq);
            int N = numCycles * samplesPerCycle;
            int warmup = N / 2;

            std::vector<double> input(N);
            std::vector<double> hfCutOut(N);
            std::vector<double> cleanHF(N);
            std::vector<double> reconstructed(N);

            for (int i = 0; i < N; ++i) {
                double t = i / sampleRate;
                input[i] = std::sin(2.0 * M_PI * freq * t);
                hfCutOut[i] = hfCut.processSample(input[i]);
                cleanHF[i] = input[i] - hfCutOut[i];
                // Reconstruction: what if saturation = passthrough?
                reconstructed[i] = hfCutOut[i] + cleanHF[i];
            }

            double inputAmp = measureAmplitude(input, sampleRate, freq, warmup);
            double reconAmp = measureAmplitude(reconstructed, sampleRate, freq, warmup);
            double hfCutAmp = measureAmplitude(hfCutOut, sampleRate, freq, warmup);
            double cleanAmp = measureAmplitude(cleanHF, sampleRate, freq, warmup);

            double deviationDb = 20.0 * std::log10(reconAmp / inputAmp);
            double hfCutDb = 20.0 * std::log10(hfCutAmp / inputAmp);
            double cleanDb = 20.0 * std::log10(cleanAmp / inputAmp);

            printf("%8.0f    %.4f      %.4f          %+.3f          %+.2f        %+.2f\n",
                   freq, inputAmp, reconAmp, deviationDb, hfCutDb, cleanDb);
        }
    }

    std::cout << "\n=== Phase Delay Analysis ===\n\n";
    std::cout << "Measuring phase shift between input and HFCut output:\n\n";

    for (int machine = 0; machine < 2; ++machine) {
        bool isAmpex = (machine == 0);
        std::cout << (isAmpex ? "AMPEX ATR-102:\n" : "\nSTUDER A820:\n");
        std::cout << "Freq (Hz)    Phase Shift (deg)    Group Delay (samples)\n";
        std::cout << "--------------------------------------------------------\n";

        for (double freq : testFreqs) {
            HFCut hfCut;
            hfCut.setSampleRate(sampleRate);
            hfCut.setMachineMode(isAmpex);

            int numCycles = (freq < 200) ? 500 : 200;
            int samplesPerCycle = static_cast<int>(sampleRate / freq);
            int N = numCycles * samplesPerCycle;
            int warmup = N / 2;
            int measureN = N - warmup;

            std::vector<double> input(N);
            std::vector<double> hfCutOut(N);

            for (int i = 0; i < N; ++i) {
                double t = i / sampleRate;
                input[i] = std::sin(2.0 * M_PI * freq * t);
                hfCutOut[i] = hfCut.processSample(input[i]);
            }

            // Measure phase via correlation
            double sumCosIn = 0, sumSinIn = 0;
            double sumCosOut = 0, sumSinOut = 0;
            for (int i = warmup; i < N; ++i) {
                double t = i / sampleRate;
                double cosVal = std::cos(2.0 * M_PI * freq * t);
                double sinVal = std::sin(2.0 * M_PI * freq * t);
                sumCosIn += input[i] * cosVal;
                sumSinIn += input[i] * sinVal;
                sumCosOut += hfCutOut[i] * cosVal;
                sumSinOut += hfCutOut[i] * sinVal;
            }

            double phaseIn = std::atan2(sumSinIn, sumCosIn);
            double phaseOut = std::atan2(sumSinOut, sumCosOut);
            double phaseDiff = (phaseOut - phaseIn) * 180.0 / M_PI;

            // Unwrap
            while (phaseDiff > 180.0) phaseDiff -= 360.0;
            while (phaseDiff < -180.0) phaseDiff += 360.0;

            // Convert to group delay in samples
            double groupDelaySamples = -phaseDiff / 360.0 * (sampleRate / freq);

            printf("%8.0f        %+7.2f              %+.3f\n", freq, phaseDiff, groupDelaySamples);
        }
    }

    std::cout << "\n";
    std::cout << "CONCLUSION:\n";
    std::cout << "- If deviation is ~0dB, the parallel structure nulls correctly\n";
    std::cout << "- Phase shift exists in HFCut, but cleanHF = input - HFCut(input)\n";
    std::cout << "- Since both paths use the same HFCut output, they're phase-aligned\n";
    std::cout << "- No delay compensation needed for the dry path itself\n";
    std::cout << "- However: if saturated path adds delay (it doesn't), we'd need to compensate\n\n";

    return 0;
}

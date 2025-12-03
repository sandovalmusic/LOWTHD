#include <iostream>
#include <cmath>
#include <vector>
#include <iomanip>
#include "../Source/DSP/MachineEQ.h"

using namespace TapeHysteresis;

// Generate a sine wave at a given frequency
std::vector<double> generateSine(double freq, double sampleRate, int numSamples)
{
    std::vector<double> signal(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        signal[i] = std::sin(2.0 * M_PI * freq * i / sampleRate);
    }
    return signal;
}

// Measure RMS of a signal (skip first N samples for filter settling)
double measureRMS(const std::vector<double>& signal, int skipSamples = 0)
{
    double sum = 0.0;
    int count = 0;
    for (size_t i = skipSamples; i < signal.size(); ++i)
    {
        sum += signal[i] * signal[i];
        count++;
    }
    return std::sqrt(sum / count);
}

// Convert linear ratio to dB
double toDB(double ratio)
{
    return 20.0 * std::log10(ratio);
}

// Test frequency response at a single frequency
double testFrequencyResponse(MachineEQ& eq, double freq, double sampleRate)
{
    const int numSamples = static_cast<int>(sampleRate * 0.1); // 100ms
    const int settleSamples = static_cast<int>(sampleRate * 0.02); // 20ms settle time

    auto input = generateSine(freq, sampleRate, numSamples);
    std::vector<double> output(numSamples);

    eq.reset();
    for (int i = 0; i < numSamples; ++i)
    {
        output[i] = eq.processSample(input[i]);
    }

    double inputRMS = measureRMS(input, settleSamples);
    double outputRMS = measureRMS(output, settleSamples);

    return toDB(outputRMS / inputRMS);
}

// Test that EQ has expected response at key frequencies
bool testAmpexEQ(double sampleRate)
{
    std::cout << "\n=== Ampex ATR-102 EQ Test (fs=" << sampleRate << "Hz) ===" << std::endl;

    MachineEQ eq;
    eq.setSampleRate(sampleRate);
    eq.setMachine(MachineEQ::Machine::Ampex);

    // Test frequencies and expected composite response (all filters combined)
    // Values based on measured response from overlapping bell filters
    struct TestPoint {
        double freq;
        double expectedGainDB;
        double toleranceDB;
        const char* description;
    };

    std::vector<TestPoint> testPoints = {
        {20.0, -2.5, 1.5, "HP rolloff region"},
        {40.0, 1.0, 0.5, "Head bump peak"},
        {100.0, 0.5, 0.5, "Post head-bump"},
        {250.0, -0.6, 0.5, "Low-mid dip"},
        {1000.0, -0.2, 0.3, "Mid reference"},
        {6000.0, -0.6, 0.3, "6kHz dip"},
        {10000.0, -0.4, 0.3, "Upper HF"},
        {20000.0, 0.1, 0.5, "HF lift from 30k bell"},
    };

    bool allPassed = true;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(10) << "Freq" << std::setw(12) << "Expected"
              << std::setw(12) << "Measured" << std::setw(10) << "Status" << std::endl;
    std::cout << std::string(44, '-') << std::endl;

    for (const auto& tp : testPoints)
    {
        double measuredDB = testFrequencyResponse(eq, tp.freq, sampleRate);
        bool passed = std::abs(measuredDB - tp.expectedGainDB) <= tp.toleranceDB;

        std::cout << std::setw(8) << tp.freq << "Hz"
                  << std::setw(10) << tp.expectedGainDB << "dB"
                  << std::setw(10) << measuredDB << "dB"
                  << std::setw(10) << (passed ? "PASS" : "FAIL") << std::endl;

        if (!passed) allPassed = false;
    }

    return allPassed;
}

bool testStuderEQ(double sampleRate)
{
    std::cout << "\n=== Studer A820 EQ Test (fs=" << sampleRate << "Hz) ===" << std::endl;

    MachineEQ eq;
    eq.setSampleRate(sampleRate);
    eq.setMachine(MachineEQ::Machine::Studer);

    struct TestPoint {
        double freq;
        double expectedGainDB;
        double toleranceDB;
        const char* description;
    };

    // Test frequencies and expected composite response (all filters combined)
    std::vector<TestPoint> testPoints = {
        {30.0, -5.5, 1.5, "HP rolloff (18dB/oct)"},
        {50.0, -1.2, 0.8, "Low bass transition"},
        {100.0, 1.3, 0.5, "Head bump peak"},
        {200.0, -0.2, 0.5, "Post head-bump dip"},
        {600.0, 0.2, 0.3, "Low-mid"},
        {1000.0, 0.15, 0.3, "Mid reference"},
        {5000.0, 0.1, 0.3, "Upper mid"},
        {10000.0, -0.1, 0.3, "HF"},
    };

    bool allPassed = true;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(10) << "Freq" << std::setw(12) << "Expected"
              << std::setw(12) << "Measured" << std::setw(10) << "Status" << std::endl;
    std::cout << std::string(44, '-') << std::endl;

    for (const auto& tp : testPoints)
    {
        double measuredDB = testFrequencyResponse(eq, tp.freq, sampleRate);
        bool passed = std::abs(measuredDB - tp.expectedGainDB) <= tp.toleranceDB;

        std::cout << std::setw(8) << tp.freq << "Hz"
                  << std::setw(10) << tp.expectedGainDB << "dB"
                  << std::setw(10) << measuredDB << "dB"
                  << std::setw(10) << (passed ? "PASS" : "FAIL") << std::endl;

        if (!passed) allPassed = false;
    }

    return allPassed;
}

// Test that machine switching works
bool testMachineSwitching()
{
    std::cout << "\n=== Machine Switching Test ===" << std::endl;

    MachineEQ eq;
    eq.setSampleRate(96000.0); // Oversampled rate

    // Test at 72Hz where Studer has -2.7dB dip, Ampex should be near flat
    const double testFreq = 72.0;

    eq.setMachine(MachineEQ::Machine::Ampex);
    double ampexResponse = testFrequencyResponse(eq, testFreq, 96000.0);

    eq.setMachine(MachineEQ::Machine::Studer);
    double studerResponse = testFrequencyResponse(eq, testFreq, 96000.0);

    std::cout << "At 72Hz: Ampex=" << std::fixed << std::setprecision(2)
              << ampexResponse << "dB, Studer=" << studerResponse << "dB" << std::endl;

    // Studer should be lower than Ampex at 72Hz (in the dip region)
    bool passed = (studerResponse < ampexResponse);
    std::cout << "Machine switching: " << (passed ? "PASS" : "FAIL") << std::endl;

    return passed;
}

// Test filter reset
bool testFilterReset()
{
    std::cout << "\n=== Filter Reset Test ===" << std::endl;

    MachineEQ eq;
    eq.setSampleRate(96000.0);
    eq.setMachine(MachineEQ::Machine::Ampex);

    // Process some samples to build up filter state
    for (int i = 0; i < 1000; ++i)
    {
        eq.processSample(std::sin(2.0 * M_PI * 1000.0 * i / 96000.0));
    }

    // Reset
    eq.reset();

    // First sample after reset should be very close to input * DC gain
    double testInput = 0.5;
    double output = eq.processSample(testInput);

    // After reset, output should be reasonable (not NaN or huge)
    bool passed = std::isfinite(output) && std::abs(output) < 10.0;

    std::cout << "After reset, output=" << output << " for input=" << testInput << std::endl;
    std::cout << "Filter reset: " << (passed ? "PASS" : "FAIL") << std::endl;

    return passed;
}

// Print full frequency response curve
void printFrequencyResponse(MachineEQ::Machine machine, double sampleRate)
{
    const char* machineName = (machine == MachineEQ::Machine::Ampex) ? "Ampex" : "Studer";
    std::cout << "\n=== " << machineName << " Frequency Response (fs=" << sampleRate << "Hz) ===" << std::endl;

    MachineEQ eq;
    eq.setSampleRate(sampleRate);
    eq.setMachine(machine);

    std::vector<double> freqs = {20, 30, 40, 50, 63, 72, 80, 100, 125, 160, 200, 250, 315,
                                  400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150,
                                  4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};

    std::cout << std::fixed << std::setprecision(2);
    for (double freq : freqs)
    {
        if (freq < sampleRate / 2.0) // Only test below Nyquist
        {
            double response = testFrequencyResponse(eq, freq, sampleRate);
            std::cout << std::setw(8) << freq << "Hz: " << std::setw(7) << response << "dB" << std::endl;
        }
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "   MachineEQ Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    bool allPassed = true;

    // Test at oversampled rate (what the plugin actually uses)
    const double oversampledRate = 96000.0;

    allPassed &= testAmpexEQ(oversampledRate);
    allPassed &= testStuderEQ(oversampledRate);
    allPassed &= testMachineSwitching();
    allPassed &= testFilterReset();

    // Print full frequency response curves for reference
    printFrequencyResponse(MachineEQ::Machine::Ampex, oversampledRate);
    printFrequencyResponse(MachineEQ::Machine::Studer, oversampledRate);

    std::cout << "\n========================================" << std::endl;
    std::cout << "   OVERALL: " << (allPassed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    std::cout << "========================================" << std::endl;

    return allPassed ? 0 : 1;
}

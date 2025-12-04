/**
 * Test_CCIR_Curve.cpp
 *
 * Verifies that our pre/de-emphasis implementation matches the
 * exact CCIR 30 IPS (35μs) equalization standard.
 *
 * CCIR 30 IPS Standard:
 * - Time constant: τ = 35 μs
 * - Turnover frequency: f_t = 1/(2πτ) = 4547.28 Hz
 * - Emphasis gain at frequency f: G(f) = sqrt(1 + (f/f_t)²)
 * - In dB: G_dB(f) = 10 * log10(1 + (f/f_t)²)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// CCIR constants
static constexpr double CCIR_TAU = 35.0e-6;  // 35 microseconds
static constexpr double CCIR_TURNOVER = 1.0 / (2.0 * M_PI * CCIR_TAU);  // 4547.28 Hz

// Calculate exact CCIR emphasis gain in dB at a given frequency
double ccirTargetDB(double freq)
{
    double ratio = freq / CCIR_TURNOVER;
    return 10.0 * std::log10(1.0 + ratio * ratio);
}

// Biquad filter (matches PreEmphasis.h)
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() { z1 = z2 = 0.0; }

    double process(double input)
    {
        double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }
};

void designHighShelf(Biquad& filter, double fc, double gainDB, double Q, double fs)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = (A + 1.0) - (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha;
    filter.b0 = (A * ((A + 1.0) + (A - 1.0) * cosOmega + 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosOmega)) / a0;
    filter.b2 = (A * ((A + 1.0) + (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha)) / a0;
    filter.a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cosOmega)) / a0;
    filter.a2 = ((A + 1.0) - (A - 1.0) * cosOmega - 2.0 * std::sqrt(A) * alpha) / a0;
}

void designBell(Biquad& filter, double fc, double gainDB, double Q, double fs)
{
    double A = std::pow(10.0, gainDB / 40.0);
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha / A;
    filter.b0 = (1.0 + alpha * A) / a0;
    filter.b1 = (-2.0 * cosOmega) / a0;
    filter.b2 = (1.0 - alpha * A) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha / A) / a0;
}

// Replicate the ReEmphasis implementation for testing (matches PreEmphasis.cpp)
struct ReEmphasisTest
{
    double fs = 48000.0;
    Biquad shelf1, shelf2, bell1, bell2, bell3;

    void setSampleRate(double sampleRate)
    {
        fs = sampleRate;
        updateCoefficients();
    }

    void reset()
    {
        shelf1.reset();
        shelf2.reset();
        bell1.reset();
        bell2.reset();
        bell3.reset();
    }

    void updateCoefficients()
    {
        // CCIR 35μs curve matching
        // Target: 10*log10(1+(f/4547)²) dB at each frequency
        //
        // Strategy:
        // 1. Broad shelf in high mids (~3kHz) for gradual rise
        // 2. Steeper shelf around 10kHz for continued rise
        // 3. Broad bell around 20kHz for final push to 13dB
        // 4. Correction bells as needed

        // Shelf 1: Broad shelf in high mids
        double shelf1Freq = 3000.0;
        double shelf1Gain = 4.0;
        double shelf1Q = 0.5;

        // Shelf 2: Shelf around 10k
        double shelf2Freq = 10000.0;
        double shelf2Gain = 5.0;
        double shelf2Q = 0.71;

        // Bell 1: Broad bell around 20k for final rise
        double bell1Freq = 20000.0;
        double bell1Gain = 5.0;       // Increased for 20k target
        double bell1Q = 0.6;          // Slightly narrower

        // Bell 2: Cut at 15k to fix 15k overshoot
        double bell2Freq = 15000.0;
        double bell2Gain = -1.1;      // Slightly more cut
        double bell2Q = 1.2;          // Narrower Q

        // Bell 3: Cut at 3k for early overshoot
        double bell3Freq = 3000.0;
        double bell3Gain = -1.0;
        double bell3Q = 1.5;

        // Clamp to safe frequencies
        double nyquist = fs / 2.0;
        if (bell1Freq > nyquist * 0.9) bell1Freq = nyquist * 0.9;
        if (shelf2Freq > nyquist * 0.9) shelf2Freq = nyquist * 0.9;

        designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
        designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
        designBell(bell1, bell1Freq, bell1Gain, bell1Q, fs);
        designBell(bell2, bell2Freq, bell2Gain, bell2Q, fs);
        designBell(bell3, bell3Freq, bell3Gain, bell3Q, fs);
    }

    double processSample(double input)
    {
        double output = shelf1.process(input);
        output = shelf2.process(output);
        output = bell1.process(output);
        output = bell2.process(output);
        output = bell3.process(output);
        return output;
    }
};

// Measure filter response at a given frequency
double measureResponseDB(ReEmphasisTest& filter, double testFreq, double sampleRate)
{
    filter.reset();

    const int numCycles = 100;
    const int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    const int totalSamples = numCycles * samplesPerCycle;
    const int skipSamples = 10 * samplesPerCycle;  // Skip initial transient

    double sumIn = 0.0, sumOut = 0.0;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);
        double output = filter.processSample(input);

        if (i >= skipSamples)
        {
            sumIn += input * input;
            sumOut += output * output;
        }
    }

    double rmsIn = std::sqrt(sumIn / (totalSamples - skipSamples));
    double rmsOut = std::sqrt(sumOut / (totalSamples - skipSamples));

    return 20.0 * std::log10(rmsOut / rmsIn);
}

int main()
{
    std::cout << "================================================================\n";
    std::cout << "   CCIR 30 IPS (35μs) Equalization Curve Verification\n";
    std::cout << "================================================================\n\n";

    std::cout << "CCIR Standard Parameters:\n";
    std::cout << "  Time constant τ = 35 μs\n";
    std::cout << "  Turnover frequency = " << std::fixed << std::setprecision(2)
              << CCIR_TURNOVER << " Hz\n\n";

    // Test frequencies
    std::vector<double> testFreqs = {100, 500, 1000, 2000, 3000, 4000, 4547, 5000,
                                     6000, 7000, 8000, 10000, 12000, 15000, 18000, 20000};

    double sampleRate = 96000.0;  // Use higher sample rate for better accuracy
    ReEmphasisTest filter;
    filter.setSampleRate(sampleRate);

    std::cout << "Sample rate: " << sampleRate << " Hz\n\n";

    std::cout << "CCIR Emphasis Curve Comparison (Re-Emphasis):\n";
    std::cout << "-------------------------------------------------------\n";
    std::cout << "  Freq (Hz)    Target (dB)    Measured (dB)    Error\n";
    std::cout << "-------------------------------------------------------\n";

    bool allPassed = true;
    double maxError = 0.0;

    for (double freq : testFreqs)
    {
        double target = ccirTargetDB(freq);
        double measured = measureResponseDB(filter, freq, sampleRate);
        double error = std::abs(measured - target);

        if (error > maxError)
            maxError = error;

        // Allow up to 0.5 dB error
        bool pass = (error < 0.5);
        if (!pass) allPassed = false;

        std::cout << std::setw(8) << std::fixed << std::setprecision(0) << freq
                  << "       " << std::setw(7) << std::setprecision(2) << target
                  << "        " << std::setw(7) << measured
                  << "        " << (pass ? "OK" : "FAIL")
                  << " (" << std::setprecision(2) << error << " dB)\n";
    }

    std::cout << "-------------------------------------------------------\n";
    std::cout << "Maximum error: " << std::fixed << std::setprecision(2) << maxError << " dB\n\n";

    // Print target reference table
    std::cout << "================================================================\n";
    std::cout << "   CCIR 35μs Reference Table (Exact Target Values)\n";
    std::cout << "================================================================\n";
    std::cout << "  Freq (Hz)    Gain (dB)    Formula: 10*log10(1+(f/4547.28)²)\n";
    std::cout << "-------------------------------------------------------\n";

    for (double freq : testFreqs)
    {
        double gain = ccirTargetDB(freq);
        std::cout << std::setw(8) << std::fixed << std::setprecision(0) << freq
                  << "       " << std::showpos << std::setw(7) << std::setprecision(2) << gain
                  << std::noshowpos << "\n";
    }

    std::cout << "\n================================================================\n";
    if (allPassed && maxError < 0.3)
    {
        std::cout << "   RESULT: EXCELLENT - All frequencies within 0.3 dB\n";
    }
    else if (allPassed)
    {
        std::cout << "   RESULT: PASS - All frequencies within 0.5 dB tolerance\n";
    }
    else
    {
        std::cout << "   RESULT: FAIL - Some frequencies exceed tolerance\n";
    }
    std::cout << "================================================================\n";

    return allPassed ? 0 : 1;
}

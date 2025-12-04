/**
 * Test_Emphasis_Null.cpp
 *
 * Verifies that ReEmphasis and DeEmphasis perfectly cancel each other
 * (null) when cascaded, without any saturation model between them.
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// ReEmphasis - matches PreEmphasis.cpp
struct ReEmphasis
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
        double shelf1Freq = 3000.0;
        double shelf1Gain = 4.0;
        double shelf1Q = 0.5;

        double shelf2Freq = 10000.0;
        double shelf2Gain = 5.0;
        double shelf2Q = 0.71;

        double bell1Freq = 20000.0;
        double bell1Gain = 5.0;
        double bell1Q = 0.6;

        double bell2Freq = 15000.0;
        double bell2Gain = -1.1;
        double bell2Q = 1.2;

        double bell3Freq = 3000.0;
        double bell3Gain = -1.0;
        double bell3Q = 1.5;

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

// DeEmphasis - matches PreEmphasis.cpp (inverse gains)
struct DeEmphasis
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
        double shelf1Freq = 3000.0;
        double shelf1Gain = -4.0;     // Inverse
        double shelf1Q = 0.5;

        double shelf2Freq = 10000.0;
        double shelf2Gain = -5.0;     // Inverse
        double shelf2Q = 0.71;

        double bell1Freq = 20000.0;
        double bell1Gain = -5.0;      // Inverse
        double bell1Q = 0.6;

        double bell2Freq = 15000.0;
        double bell2Gain = +1.1;      // Inverse
        double bell2Q = 1.2;

        double bell3Freq = 3000.0;
        double bell3Gain = +1.0;      // Inverse
        double bell3Q = 1.5;

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

// Measure cascaded response at a given frequency
double measureCascadedResponseDB(ReEmphasis& reEmph, DeEmphasis& deEmph,
                                  double testFreq, double sampleRate)
{
    reEmph.reset();
    deEmph.reset();

    const int numCycles = 100;
    const int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    const int totalSamples = numCycles * samplesPerCycle;
    const int skipSamples = 10 * samplesPerCycle;

    double sumIn = 0.0, sumOut = 0.0;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);

        // Cascade: ReEmphasis -> DeEmphasis
        double afterReEmph = reEmph.processSample(input);
        double output = deEmph.processSample(afterReEmph);

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
    std::cout << "   ReEmphasis + DeEmphasis Null Test\n";
    std::cout << "================================================================\n\n";

    std::vector<double> testFreqs = {100, 500, 1000, 2000, 3000, 4000, 5000,
                                     6000, 8000, 10000, 12000, 15000, 18000, 20000};

    double sampleRate = 96000.0;
    ReEmphasis reEmph;
    DeEmphasis deEmph;
    reEmph.setSampleRate(sampleRate);
    deEmph.setSampleRate(sampleRate);

    std::cout << "Sample rate: " << sampleRate << " Hz\n\n";

    std::cout << "Cascaded Response (ReEmphasis -> DeEmphasis):\n";
    std::cout << "Target: 0.0 dB at all frequencies (perfect null)\n";
    std::cout << "-----------------------------------------------\n";
    std::cout << "  Freq (Hz)    Response (dB)    Status\n";
    std::cout << "-----------------------------------------------\n";

    bool allPassed = true;
    double maxError = 0.0;

    for (double freq : testFreqs)
    {
        double response = measureCascadedResponseDB(reEmph, deEmph, freq, sampleRate);
        double error = std::abs(response);

        if (error > maxError)
            maxError = error;

        // Allow up to 0.1 dB deviation from perfect null
        bool pass = (error < 0.1);
        if (!pass) allPassed = false;

        std::cout << std::setw(8) << std::fixed << std::setprecision(0) << freq
                  << "        " << std::setw(7) << std::setprecision(3) << response
                  << "        " << (pass ? "OK" : "FAIL") << "\n";
    }

    std::cout << "-----------------------------------------------\n";
    std::cout << "Maximum deviation from null: " << std::fixed << std::setprecision(3)
              << maxError << " dB\n\n";

    std::cout << "================================================================\n";
    if (allPassed)
    {
        std::cout << "   RESULT: PASS - Perfect null within 0.1 dB tolerance\n";
    }
    else
    {
        std::cout << "   RESULT: FAIL - Some frequencies don't null properly\n";
    }
    std::cout << "================================================================\n";

    return allPassed ? 0 : 1;
}

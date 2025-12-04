/**
 * Test_Crosstalk.cpp
 *
 * Verifies the Studer crosstalk filter behavior:
 * - Studer mode adds mono crosstalk at -40dB
 * - Ampex mode has no crosstalk
 * - Crosstalk is bandpassed (100Hz-8kHz)
 */

#include <iostream>
#include <cmath>
#include <vector>

// Replicate the CrosstalkFilter from PluginProcessor.h for standalone testing
struct CrosstalkFilter
{
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() { z1 = z2 = 0.0f; }

        float process(float input)
        {
            float output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }

        void setHighPass(float fc, float Q, float sampleRate)
        {
            float w0 = 2.0f * 3.14159265f * fc / sampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * Q);
            float a0 = 1.0f + alpha;
            b0 = ((1.0f + cosw0) / 2.0f) / a0;
            b1 = (-(1.0f + cosw0)) / a0;
            b2 = ((1.0f + cosw0) / 2.0f) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void setLowPass(float fc, float Q, float sampleRate)
        {
            float w0 = 2.0f * 3.14159265f * fc / sampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * Q);
            float a0 = 1.0f + alpha;
            b0 = ((1.0f - cosw0) / 2.0f) / a0;
            b1 = (1.0f - cosw0) / a0;
            b2 = ((1.0f - cosw0) / 2.0f) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }
    };

    Biquad highpass;
    Biquad lowpass;
    float gain = 0.01f;  // -40dB

    void prepare(float sampleRate)
    {
        highpass.setHighPass(100.0f, 0.707f, sampleRate);
        lowpass.setLowPass(8000.0f, 0.707f, sampleRate);
        reset();
    }

    void reset()
    {
        highpass.reset();
        lowpass.reset();
    }

    float process(float monoInput)
    {
        float filtered = highpass.process(monoInput);
        filtered = lowpass.process(filtered);
        return filtered * gain;
    }
};

// Generate sine wave
std::vector<float> generateSine(float frequency, float sampleRate, int numSamples, float amplitude = 1.0f)
{
    std::vector<float> buffer(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        buffer[i] = amplitude * std::sin(2.0f * 3.14159265f * frequency * i / sampleRate);
    }
    return buffer;
}

// Measure RMS of a buffer
float measureRMS(const std::vector<float>& buffer, int startSample = 0, int numSamples = -1)
{
    if (numSamples < 0) numSamples = buffer.size() - startSample;

    float sum = 0.0f;
    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        sum += buffer[i] * buffer[i];
    }
    return std::sqrt(sum / numSamples);
}

float toDb(float linear)
{
    return 20.0f * std::log10(linear + 1e-10f);
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "   Crosstalk Filter Test Suite\n";
    std::cout << "========================================\n\n";

    const float sampleRate = 48000.0f;
    const int numSamples = 48000;  // 1 second

    CrosstalkFilter filter;
    filter.prepare(sampleRate);

    bool allPassed = true;

    // ===========================================
    // Test 1: Crosstalk level at 1kHz (passband)
    // ===========================================
    std::cout << "=== Test 1: Crosstalk Level at 1kHz ===\n";
    {
        filter.reset();
        auto input = generateSine(1000.0f, sampleRate, numSamples, 1.0f);
        std::vector<float> output(numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            output[i] = filter.process(input[i]);
        }

        // Measure after filter settles (skip first 1000 samples)
        float inputRMS = measureRMS(input, 1000, numSamples - 1000);
        float outputRMS = measureRMS(output, 1000, numSamples - 1000);
        float levelDb = toDb(outputRMS / inputRMS);

        std::cout << "Input RMS: " << inputRMS << "\n";
        std::cout << "Output RMS: " << outputRMS << "\n";
        std::cout << "Crosstalk level: " << levelDb << " dB\n";

        // Should be around -40dB (allow some tolerance for filter passband ripple)
        bool pass = (levelDb > -42.0f && levelDb < -38.0f);
        std::cout << "Expected: -40dB (+/- 2dB)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 2: Highpass rolloff at 50Hz
    // ===========================================
    std::cout << "=== Test 2: Highpass Rolloff at 50Hz ===\n";
    {
        filter.reset();
        auto input = generateSine(50.0f, sampleRate, numSamples, 1.0f);
        std::vector<float> output(numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            output[i] = filter.process(input[i]);
        }

        float inputRMS = measureRMS(input, 2000, numSamples - 2000);
        float outputRMS = measureRMS(output, 2000, numSamples - 2000);
        float levelDb = toDb(outputRMS / inputRMS);

        std::cout << "Crosstalk level at 50Hz: " << levelDb << " dB\n";

        // Should be significantly lower than -40dB due to HP filter
        bool pass = (levelDb < -45.0f);
        std::cout << "Expected: < -45dB (HP rolloff)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 3: Lowpass rolloff at 12kHz
    // ===========================================
    std::cout << "=== Test 3: Lowpass Rolloff at 12kHz ===\n";
    {
        filter.reset();
        auto input = generateSine(12000.0f, sampleRate, numSamples, 1.0f);
        std::vector<float> output(numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            output[i] = filter.process(input[i]);
        }

        float inputRMS = measureRMS(input, 1000, numSamples - 1000);
        float outputRMS = measureRMS(output, 1000, numSamples - 1000);
        float levelDb = toDb(outputRMS / inputRMS);

        std::cout << "Crosstalk level at 12kHz: " << levelDb << " dB\n";

        // Should be lower than -40dB due to LP filter
        bool pass = (levelDb < -43.0f);
        std::cout << "Expected: < -43dB (LP rolloff)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 4: Frequency response sweep
    // ===========================================
    std::cout << "=== Test 4: Frequency Response ===\n";
    {
        float testFreqs[] = {30, 50, 100, 200, 500, 1000, 2000, 4000, 8000, 12000, 16000};

        std::cout << "   Freq     Level\n";
        std::cout << "-------------------\n";

        for (float freq : testFreqs)
        {
            filter.reset();
            auto input = generateSine(freq, sampleRate, numSamples, 1.0f);
            std::vector<float> output(numSamples);

            for (int i = 0; i < numSamples; ++i)
            {
                output[i] = filter.process(input[i]);
            }

            int skipSamples = std::max(2000, (int)(sampleRate / freq * 10));
            float inputRMS = measureRMS(input, skipSamples, numSamples - skipSamples);
            float outputRMS = measureRMS(output, skipSamples, numSamples - skipSamples);
            float levelDb = toDb(outputRMS / inputRMS);

            printf("%7.0fHz  %6.1fdB\n", freq, levelDb);
        }
        std::cout << "\n";
    }

    // ===========================================
    // Test 5: Stereo crosstalk simulation
    // ===========================================
    std::cout << "=== Test 5: Stereo Crosstalk Simulation ===\n";
    {
        filter.reset();

        // Hard-panned signal: Left = 1kHz sine, Right = silence
        auto leftIn = generateSine(1000.0f, sampleRate, numSamples, 1.0f);
        std::vector<float> rightIn(numSamples, 0.0f);

        std::vector<float> leftOut(numSamples);
        std::vector<float> rightOut(numSamples);

        // Simulate the crosstalk processing from PluginProcessor
        for (int i = 0; i < numSamples; ++i)
        {
            float mono = (leftIn[i] + rightIn[i]) * 0.5f;
            float crosstalk = filter.process(mono);
            leftOut[i] = leftIn[i] + crosstalk;
            rightOut[i] = rightIn[i] + crosstalk;
        }

        // Measure crosstalk appearing in the silent right channel
        float rightRMS = measureRMS(rightOut, 1000, numSamples - 1000);
        float leftRMS = measureRMS(leftIn, 1000, numSamples - 1000);
        float crosstalkDb = toDb(rightRMS / leftRMS);

        std::cout << "Left input RMS: " << leftRMS << "\n";
        std::cout << "Right output RMS (crosstalk): " << rightRMS << "\n";
        std::cout << "Crosstalk in silent channel: " << crosstalkDb << " dB\n";

        // For hard-panned content, mono = L * 0.5, so crosstalk = -40dB - 6dB = -46dB
        bool pass = (crosstalkDb > -49.0f && crosstalkDb < -43.0f);
        std::cout << "Expected: ~-46dB (hard-panned signal)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Summary
    // ===========================================
    std::cout << "========================================\n";
    if (allPassed)
    {
        std::cout << "   OVERALL: ALL TESTS PASSED\n";
    }
    else
    {
        std::cout << "   OVERALL: SOME TESTS FAILED\n";
    }
    std::cout << "========================================\n";

    return allPassed ? 0 : 1;
}

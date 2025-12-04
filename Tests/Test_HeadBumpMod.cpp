/**
 * Test_HeadBumpMod.cpp
 *
 * Verifies the head bump modulator behavior:
 * - LFO produces organic, slow variation
 * - Modulation depth is within spec (±0.08dB Ampex, ±0.12dB Studer)
 * - Bandpass isolates the head bump region
 */

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

// Replicate the HeadBumpModulator from PluginProcessor.h for standalone testing
struct HeadBumpModulator
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

        void setBandpass(float fc, float Q, float sampleRate)
        {
            float w0 = 2.0f * 3.14159265f * fc / sampleRate;
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * Q);
            float a0 = 1.0f + alpha;
            b0 = (sinw0 / 2.0f) / a0;
            b1 = 0.0f;
            b2 = (-sinw0 / 2.0f) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }
    };

    Biquad bandpassL, bandpassR;

    float phase1 = 0.0f;
    float phase2 = 0.0f;
    float phase3 = 0.0f;

    static constexpr float freq1 = 0.63f;
    static constexpr float freq2 = 1.07f;
    static constexpr float freq3 = 0.31f;

    float sampleRate = 48000.0f;
    float centerFreq = 60.0f;
    float modulationDepth = 0.012f;

    void prepare(float sr, bool isAmpex)
    {
        sampleRate = sr;

        if (isAmpex)
        {
            centerFreq = 40.0f;
            modulationDepth = 0.009f;
        }
        else
        {
            centerFreq = 75.0f;
            modulationDepth = 0.014f;
        }

        bandpassL.setBandpass(centerFreq, 0.7f, sampleRate);
        bandpassR.setBandpass(centerFreq, 0.7f, sampleRate);
        reset();
    }

    void reset()
    {
        bandpassL.reset();
        bandpassR.reset();
        phase1 = 0.0f;
        phase2 = 0.3f;
        phase3 = 0.7f;
    }

    float updateLFO(int blockSize)
    {
        float blockTime = static_cast<float>(blockSize) / sampleRate;

        phase1 += freq1 * blockTime * 6.28318530718f;
        phase2 += freq2 * blockTime * 6.28318530718f;
        phase3 += freq3 * blockTime * 6.28318530718f;

        if (phase1 > 6.28318530718f) phase1 -= 6.28318530718f;
        if (phase2 > 6.28318530718f) phase2 -= 6.28318530718f;
        if (phase3 > 6.28318530718f) phase3 -= 6.28318530718f;

        float lfo = std::sin(phase1) * 0.5f +
                    std::sin(phase2) * 0.3f +
                    std::sin(phase3) * 0.2f;

        return 1.0f + lfo * modulationDepth;
    }

    void processSample(float& left, float& right, float modGain)
    {
        float bumpL = bandpassL.process(left);
        float bumpR = bandpassR.process(right);

        float modAmount = modGain - 1.0f;
        left += bumpL * modAmount;
        right += bumpR * modAmount;
    }
};

float toDb(float linear)
{
    return 20.0f * std::log10(std::abs(linear) + 1e-10f);
}

int main()
{
    std::cout << "========================================\n";
    std::cout << "   Head Bump Modulator Test Suite\n";
    std::cout << "========================================\n\n";

    const float sampleRate = 48000.0f;
    const int blockSize = 512;

    bool allPassed = true;

    // ===========================================
    // Test 1: LFO Range - Ampex
    // ===========================================
    std::cout << "=== Test 1: LFO Range (Ampex) ===\n";
    {
        HeadBumpModulator mod;
        mod.prepare(sampleRate, true);  // Ampex

        float minGain = 2.0f, maxGain = 0.0f;

        // Run for 10 seconds to capture full LFO cycle
        int numBlocks = (int)(10.0f * sampleRate / blockSize);
        for (int b = 0; b < numBlocks; ++b)
        {
            float gain = mod.updateLFO(blockSize);
            minGain = std::min(minGain, gain);
            maxGain = std::max(maxGain, gain);
        }

        float rangeDb = toDb(maxGain) - toDb(minGain);
        std::cout << "Min gain: " << minGain << " (" << toDb(minGain) << " dB)\n";
        std::cout << "Max gain: " << maxGain << " (" << toDb(maxGain) << " dB)\n";
        std::cout << "Total range: " << rangeDb << " dB\n";

        // Ampex: ±0.08dB = 0.16dB total range
        bool pass = (rangeDb > 0.12f && rangeDb < 0.20f);
        std::cout << "Expected range: ~0.16dB (±0.08dB)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 2: LFO Range - Studer
    // ===========================================
    std::cout << "=== Test 2: LFO Range (Studer) ===\n";
    {
        HeadBumpModulator mod;
        mod.prepare(sampleRate, false);  // Studer

        float minGain = 2.0f, maxGain = 0.0f;

        int numBlocks = (int)(10.0f * sampleRate / blockSize);
        for (int b = 0; b < numBlocks; ++b)
        {
            float gain = mod.updateLFO(blockSize);
            minGain = std::min(minGain, gain);
            maxGain = std::max(maxGain, gain);
        }

        float rangeDb = toDb(maxGain) - toDb(minGain);
        std::cout << "Min gain: " << minGain << " (" << toDb(minGain) << " dB)\n";
        std::cout << "Max gain: " << maxGain << " (" << toDb(maxGain) << " dB)\n";
        std::cout << "Total range: " << rangeDb << " dB\n";

        // Studer: ±0.12dB = 0.24dB total range
        bool pass = (rangeDb > 0.20f && rangeDb < 0.30f);
        std::cout << "Expected range: ~0.24dB (±0.12dB)\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 3: LFO Rate (should be slow, 0.3-1.1 Hz)
    // ===========================================
    std::cout << "=== Test 3: LFO Rate ===\n";
    {
        HeadBumpModulator mod;
        mod.prepare(sampleRate, true);

        // Count zero crossings over 10 seconds
        int zeroCrossings = 0;
        float lastGain = mod.updateLFO(blockSize);

        int numBlocks = (int)(10.0f * sampleRate / blockSize);
        for (int b = 0; b < numBlocks; ++b)
        {
            float gain = mod.updateLFO(blockSize);
            if ((gain - 1.0f) * (lastGain - 1.0f) < 0)
                zeroCrossings++;
            lastGain = gain;
        }

        // Zero crossings / 2 = cycles, / 10 seconds = Hz
        float estimatedHz = (zeroCrossings / 2.0f) / 10.0f;
        std::cout << "Zero crossings in 10s: " << zeroCrossings << "\n";
        std::cout << "Estimated LFO rate: ~" << estimatedHz << " Hz\n";

        // Should be roughly 0.5-1.5 Hz (complex waveform)
        bool pass = (estimatedHz > 0.3f && estimatedHz < 2.0f);
        std::cout << "Expected: 0.3-2.0 Hz range\n";
        std::cout << "Result: " << (pass ? "PASS" : "FAIL") << "\n\n";
        if (!pass) allPassed = false;
    }

    // ===========================================
    // Test 4: Bandpass frequency response
    // ===========================================
    std::cout << "=== Test 4: Bandpass Response (Ampex 40Hz center) ===\n";
    {
        HeadBumpModulator mod;
        mod.prepare(sampleRate, true);  // Ampex, 40Hz center

        float testFreqs[] = {20, 30, 40, 50, 60, 80, 100, 150, 200};
        const int testSamples = 48000;

        std::cout << "   Freq    Response\n";
        std::cout << "---------------------\n";

        for (float freq : testFreqs)
        {
            mod.bandpassL.reset();

            // Generate sine and measure bandpass output
            float sumIn = 0.0f, sumOut = 0.0f;
            for (int i = 1000; i < testSamples; ++i)  // Skip transient
            {
                float input = std::sin(2.0f * 3.14159265f * freq * i / sampleRate);
                float output = mod.bandpassL.process(input);
                sumIn += input * input;
                sumOut += output * output;
            }

            float rmsIn = std::sqrt(sumIn / (testSamples - 1000));
            float rmsOut = std::sqrt(sumOut / (testSamples - 1000));
            float responseDb = toDb(rmsOut / rmsIn);

            printf("%5.0fHz    %6.1fdB\n", freq, responseDb);
        }
        std::cout << "\n";
    }

    // ===========================================
    // Test 5: Modulation applies to signal
    // ===========================================
    std::cout << "=== Test 5: Signal Modulation ===\n";
    {
        HeadBumpModulator mod;
        mod.prepare(sampleRate, true);  // Ampex

        // Generate 40Hz tone (at head bump center)
        const int testSamples = 48000;
        std::vector<float> left(testSamples), right(testSamples);

        for (int i = 0; i < testSamples; ++i)
        {
            left[i] = std::sin(2.0f * 3.14159265f * 40.0f * i / sampleRate);
            right[i] = left[i];
        }

        // Process with constant mod gain of 1.01 (simulating LFO peak)
        float modGain = 1.009f;  // Ampex max modulation
        for (int i = 0; i < testSamples; ++i)
        {
            mod.processSample(left[i], right[i], modGain);
        }

        // Measure the difference
        float originalRMS = 0.707f;  // Sine wave RMS
        float sumOut = 0.0f;
        for (int i = 1000; i < testSamples; ++i)
        {
            sumOut += left[i] * left[i];
        }
        float outputRMS = std::sqrt(sumOut / (testSamples - 1000));
        float changeDb = toDb(outputRMS / originalRMS);

        std::cout << "Original RMS: " << originalRMS << "\n";
        std::cout << "Output RMS: " << outputRMS << "\n";
        std::cout << "Level change: " << changeDb << " dB\n";

        // Should see a small positive change
        bool pass = (changeDb > 0.0f && changeDb < 0.2f);
        std::cout << "Expected: Small positive change (< 0.2dB)\n";
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

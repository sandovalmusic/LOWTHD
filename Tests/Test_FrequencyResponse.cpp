/**
 * Test_FrequencyResponse.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Frequency Response Validation
 *
 * Validates:
 * 1. Flat frequency response at low levels (no unwanted coloration)
 * 2. Frequency-dependent saturation at high levels (bass saturates more)
 * 3. De-emphasis/Re-emphasis cancellation
 * 4. No frequency-dependent artifacts from DC blocking
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;
constexpr int NUM_CYCLES = 100;
constexpr int SKIP_CYCLES = 20;

double measureGain(TapeHysteresis::HybridTapeProcessor& processor,
                   double amplitude,
                   double frequency) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = NUM_CYCLES * samplesPerCycle;
    int skipSamples = SKIP_CYCLES * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    // Measure fundamental magnitude
    double real = 0.0, imag = 0.0;
    for (int i = skipSamples; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        real += output[i] * std::cos(phase);
        imag += output[i] * std::sin(phase);
    }

    real /= analysisSamples;
    imag /= analysisSamples;
    double outputLevel = 2.0 * std::sqrt(real * real + imag * imag);

    return 20.0 * std::log10(outputLevel / amplitude);
}

double measureTHD(TapeHysteresis::HybridTapeProcessor& processor,
                  double amplitude,
                  double frequency) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = NUM_CYCLES * samplesPerCycle;
    int skipSamples = SKIP_CYCLES * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        double phase = 2.0 * M_PI * frequency * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    double fundamental = 0.0;
    double harmonicPower = 0.0;

    for (int h = 1; h <= 5; ++h) {
        double real = 0.0, imag = 0.0;
        for (int i = skipSamples; i < totalSamples; ++i) {
            double phase = 2.0 * M_PI * h * frequency * i / SAMPLE_RATE;
            real += output[i] * std::cos(phase);
            imag += output[i] * std::sin(phase);
        }
        real /= analysisSamples;
        imag /= analysisSamples;
        double mag = 2.0 * std::sqrt(real * real + imag * imag);

        if (h == 1) {
            fundamental = mag;
        } else {
            harmonicPower += mag * mag;
        }
    }

    return (fundamental > 1e-10) ? 100.0 * std::sqrt(harmonicPower) / fundamental : 0.0;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  LOW THD TAPE SIMULATOR v1.0 - FREQUENCY RESPONSE TEST   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    // Test frequencies spanning the audio range
    double frequencies[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };
    int numFreqs = 10;

    // =========================================================================
    // TEST 1: FLAT FREQUENCY RESPONSE AT LOW LEVELS
    // =========================================================================
    // At -20dB input, response should be flat within ±0.5dB from 20Hz to 20kHz
    // This ensures de-emphasis and re-emphasis properly cancel

    std::cout << "\n=== FLAT RESPONSE TEST (Low Level: -20dB) ===\n";
    std::cout << "Expected: Flat within +/-0.5dB from 20Hz to 20kHz\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = std::pow(10.0, -20.0 / 20.0);  // -20dB

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)     Gain(dB)   Status\n";
        std::cout << "--------------------------------\n";

        // Use 1kHz as reference
        double refGain = measureGain(processor, amplitude, 1000.0);
        int passed = 0;

        for (int i = 0; i < numFreqs; i++) {
            double gain = measureGain(processor, amplitude, frequencies[i]);
            double deviation = gain - refGain;

            bool ok = (std::abs(deviation) < 0.5);
            if (ok) passed++;

            std::cout << std::setw(7) << frequencies[i] << "     "
                      << std::fixed << std::setprecision(2) << std::showpos
                      << std::setw(6) << deviation << "     "
                      << std::noshowpos << (ok ? "PASS" : "FAIL") << "\n";
        }

        std::cout << "Result: " << passed << "/" << numFreqs << " frequencies passed\n\n";
        if (passed != numFreqs) allPassed = false;
    }

    // =========================================================================
    // TEST 2: FREQUENCY-DEPENDENT SATURATION
    // =========================================================================
    // At high levels (+6dB), bass should show more THD than treble
    // This verifies the de-emphasis/re-emphasis is working correctly

    std::cout << "\n=== FREQUENCY-DEPENDENT SATURATION TEST (+6dB) ===\n";
    std::cout << "Expected: Higher THD at low frequencies (bass saturates more)\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = std::pow(10.0, 6.0 / 20.0);  // +6dB

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)     THD%       Note\n";
        std::cout << "--------------------------------\n";

        double thd100 = measureTHD(processor, amplitude, 100.0);
        double thd1k = measureTHD(processor, amplitude, 1000.0);
        double thd10k = measureTHD(processor, amplitude, 10000.0);

        std::cout << "     100     " << std::fixed << std::setprecision(3)
                  << std::setw(6) << thd100 << "     Bass\n";
        std::cout << "    1000     " << std::setw(6) << thd1k << "     Mid\n";
        std::cout << "   10000     " << std::setw(6) << thd10k << "     Treble\n";

        // Verify bass has more THD than treble
        bool bassMore = (thd100 > thd10k);
        std::cout << "Bass > Treble THD: " << (bassMore ? "PASS" : "FAIL") << "\n\n";
        if (!bassMore) allPassed = false;
    }

    // =========================================================================
    // TEST 3: DC BLOCKING EFFECTIVENESS
    // =========================================================================
    // Very low frequencies (5Hz) should be attenuated by DC blocker
    // 20Hz should pass with minimal attenuation

    std::cout << "\n=== DC BLOCKING TEST ===\n";
    std::cout << "Expected: Strong attenuation at 5Hz, minimal at 20Hz\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = 0.1;  // Low level to avoid saturation effects

        double gain5Hz = measureGain(processor, amplitude, 5.0);
        double gain20Hz = measureGain(processor, amplitude, 20.0);
        double gain1kHz = measureGain(processor, amplitude, 1000.0);

        std::cout << modeName << ":\n";
        std::cout << "  5Hz attenuation:  " << std::fixed << std::setprecision(1)
                  << (gain1kHz - gain5Hz) << " dB";
        bool block5 = (gain5Hz < gain1kHz - 6.0);  // Should be >6dB down
        std::cout << "  " << (block5 ? "PASS" : "FAIL") << "\n";

        std::cout << "  20Hz attenuation: " << (gain1kHz - gain20Hz) << " dB";
        bool pass20 = (std::abs(gain20Hz - gain1kHz) < 1.0);  // Should be < 1dB
        std::cout << "  " << (pass20 ? "PASS" : "FAIL") << "\n\n";

        if (!block5 || !pass20) allPassed = false;
    }

    // =========================================================================
    // TEST 4: UNITY GAIN AT LOW LEVELS
    // =========================================================================
    // At very low levels, gain should be approximately 0dB (unity)

    std::cout << "\n=== UNITY GAIN TEST ===\n";
    std::cout << "Expected: Gain within +/-0.5dB at -30dB input level\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double amplitude = std::pow(10.0, -30.0 / 20.0);  // -30dB
        double gain = measureGain(processor, amplitude, 1000.0);

        bool unity = (std::abs(gain) < 0.5);
        std::cout << modeName << ": Gain = " << std::fixed << std::setprecision(2)
                  << std::showpos << gain << " dB  "
                  << std::noshowpos << (unity ? "PASS" : "FAIL") << "\n";

        if (!unity) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "FREQUENCY RESPONSE TEST: ALL PASSED\n";
    } else {
        std::cout << "FREQUENCY RESPONSE TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

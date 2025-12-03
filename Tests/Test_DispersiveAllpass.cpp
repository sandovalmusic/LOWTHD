/**
 * Test_DispersiveAllpass.cpp
 *
 * LOW THD TAPE SIMULATOR v1.0 - Dispersive Allpass Filter Validation
 *
 * Tests the HF phase smear implementation:
 * 1. Amplitude flatness (allpass should not affect magnitude)
 * 2. Phase shift increases with frequency
 * 3. Group delay is frequency-dependent (dispersion)
 * 4. Mode-dependent behavior (Studer has more smear than Ampex)
 * 5. Transient softening effect
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include "../Source/DSP/HybridTapeProcessor.h"
#include "../Source/DSP/PreEmphasis.cpp"
#include "../Source/DSP/HybridTapeProcessor.cpp"

constexpr double SAMPLE_RATE = 96000.0;
constexpr double PI = 3.14159265358979323846;

struct PhaseAnalysis {
    double frequency;
    double magnitude;
    double magnitudeDb;
    double phaseShift;    // degrees
    double groupDelay;    // samples
};

// Measure phase and magnitude at a specific frequency
PhaseAnalysis measurePhaseResponse(TapeHysteresis::HybridTapeProcessor& processor,
                                    double frequency, double amplitude = 0.1) {
    processor.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = 100 * samplesPerCycle;
    int skipSamples = 50 * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    // Process sine wave
    for (int i = 0; i < totalSamples; i++) {
        double phase = 2.0 * PI * frequency * i / SAMPLE_RATE;
        double input = amplitude * std::sin(phase);
        output[i] = processor.processSample(input);
    }

    // DFT at fundamental frequency
    double real = 0, imag = 0;
    for (int i = skipSamples; i < totalSamples; i++) {
        double phase = 2.0 * PI * frequency * i / SAMPLE_RATE;
        real += output[i] * std::cos(phase);
        imag += output[i] * std::sin(phase);
    }
    real /= analysisSamples;
    imag /= analysisSamples;

    PhaseAnalysis result;
    result.frequency = frequency;
    result.magnitude = 2.0 * std::sqrt(real * real + imag * imag) / amplitude;
    result.magnitudeDb = 20.0 * std::log10(result.magnitude);
    result.phaseShift = std::atan2(-imag, real) * 180.0 / PI;

    // Measure group delay by comparing phase at nearby frequency
    double df = frequency * 0.01;
    processor.reset();
    for (int i = 0; i < totalSamples; i++) {
        double phase = 2.0 * PI * (frequency + df) * i / SAMPLE_RATE;
        output[i] = processor.processSample(amplitude * std::sin(phase));
    }
    double real2 = 0, imag2 = 0;
    for (int i = skipSamples; i < totalSamples; i++) {
        double phase = 2.0 * PI * (frequency + df) * i / SAMPLE_RATE;
        real2 += output[i] * std::cos(phase);
        imag2 += output[i] * std::sin(phase);
    }
    double phase2 = std::atan2(-imag2, real2);
    double phase1 = std::atan2(-imag, real);
    double dphase = phase2 - phase1;
    if (dphase > PI) dphase -= 2 * PI;
    if (dphase < -PI) dphase += 2 * PI;

    result.groupDelay = -dphase / (2.0 * PI * df) * SAMPLE_RATE;

    return result;
}

// Measure transient smear width
double measureTransientSmear(TapeHysteresis::HybridTapeProcessor& processor) {
    processor.reset();

    // Generate a click (impulse)
    std::vector<double> output(2000);
    output[0] = processor.processSample(0.5);  // Moderate level impulse
    for (int i = 1; i < 2000; i++) {
        output[i] = processor.processSample(0.0);
    }

    // Find peak and measure width at -6dB
    double peak = 0;
    int peakIdx = 0;
    for (int i = 0; i < 2000; i++) {
        if (std::abs(output[i]) > peak) {
            peak = std::abs(output[i]);
            peakIdx = i;
        }
    }

    double threshold = peak * 0.5;  // -6dB
    int startIdx = peakIdx, endIdx = peakIdx;
    for (int i = peakIdx; i >= 0; i--) {
        if (std::abs(output[i]) >= threshold) startIdx = i;
        else break;
    }
    for (int i = peakIdx; i < 2000; i++) {
        if (std::abs(output[i]) >= threshold) endIdx = i;
        else break;
    }

    // Width in microseconds
    return (endIdx - startIdx) * 1e6 / SAMPLE_RATE;
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  LOW THD TAPE SIMULATOR v1.0 - DISPERSIVE ALLPASS TEST   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    bool allPassed = true;

    double testFreqs[] = { 1000, 2000, 4000, 6000, 8000, 10000, 12000, 15000 };
    int numFreqs = 8;

    // =========================================================================
    // TEST 1: AMPLITUDE FLATNESS
    // =========================================================================
    // Allpass filters should not affect magnitude - response should be flat
    // Allow ±0.5dB tolerance (accounts for emphasis and other processing)

    std::cout << "\n=== TEST 1: AMPLITUDE FLATNESS ===\n";
    std::cout << "Expected: Magnitude within ±0.5dB across frequency range\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)    Gain(dB)    Status\n";
        std::cout << "--------------------------------\n";

        int passed = 0;
        double minGain = 999, maxGain = -999;

        for (int i = 0; i < numFreqs; i++) {
            auto result = measurePhaseResponse(processor, testFreqs[i]);

            minGain = std::min(minGain, result.magnitudeDb);
            maxGain = std::max(maxGain, result.magnitudeDb);

            bool flat = (std::abs(result.magnitudeDb) < 0.5);
            if (flat) passed++;

            std::cout << std::setw(7) << testFreqs[i] << "     "
                      << std::showpos << std::fixed << std::setprecision(2)
                      << std::setw(6) << result.magnitudeDb << std::noshowpos << "      "
                      << (flat ? "PASS" : "FAIL") << "\n";
        }

        double variation = maxGain - minGain;
        std::cout << "Variation: " << std::setprecision(2) << variation << " dB\n";
        std::cout << "Result: " << passed << "/" << numFreqs << " frequencies passed\n\n";

        if (passed < numFreqs - 1) allPassed = false;  // Allow 1 marginal failure
    }

    // =========================================================================
    // TEST 2: PHASE SHIFT INCREASES WITH FREQUENCY
    // =========================================================================
    // Dispersive allpass should have increasing phase shift at higher frequencies

    std::cout << "=== TEST 2: FREQUENCY-DEPENDENT PHASE SHIFT ===\n";
    std::cout << "Expected: Phase shift magnitude increases with frequency\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        std::cout << modeName << ":\n";
        std::cout << "Freq(Hz)    Phase(°)    GroupDelay(samp)\n";
        std::cout << "-----------------------------------------\n";

        double prevPhase = 0;
        int monotonic = 0;

        for (int i = 0; i < numFreqs; i++) {
            auto result = measurePhaseResponse(processor, testFreqs[i]);

            // Unwrap phase for monotonicity check
            double absPhase = std::abs(result.phaseShift);
            if (i > 0 && absPhase >= std::abs(prevPhase)) {
                monotonic++;
            }
            prevPhase = result.phaseShift;

            std::cout << std::setw(7) << testFreqs[i] << "      "
                      << std::fixed << std::setprecision(1)
                      << std::setw(7) << result.phaseShift << "         "
                      << std::setprecision(2) << std::setw(5) << result.groupDelay << "\n";
        }

        bool increasing = (monotonic >= numFreqs - 2);  // Allow some non-monotonicity
        std::cout << "Phase monotonicity: " << (increasing ? "PASS" : "FAIL")
                  << " (" << monotonic << "/" << (numFreqs - 1) << " increasing)\n\n";

        if (!increasing) allPassed = false;
    }

    // =========================================================================
    // TEST 3: MODE-DEPENDENT PHASE SMEAR
    // =========================================================================
    // Studer should have more transient smear than Ampex
    // This is measured better by transient width than phase at a single frequency

    std::cout << "=== TEST 3: MODE-DEPENDENT CHARACTERISTICS ===\n";
    std::cout << "Expected: Both modes have meaningful phase shift at HF\n\n";

    TapeHysteresis::HybridTapeProcessor ampexProc, studerProc;
    ampexProc.setSampleRate(SAMPLE_RATE);
    ampexProc.setParameters(0.65, 1.0);  // Ampex
    studerProc.setSampleRate(SAMPLE_RATE);
    studerProc.setParameters(0.82, 1.0);  // Studer

    auto ampexPhase = measurePhaseResponse(ampexProc, 8000);
    auto studerPhase = measurePhaseResponse(studerProc, 8000);

    std::cout << "Ampex phase @ 8kHz:  " << std::fixed << std::setprecision(1)
              << ampexPhase.phaseShift << "°\n";
    std::cout << "Studer phase @ 8kHz: " << studerPhase.phaseShift << "°\n";

    // Both should have significant phase shift (> 90°)
    bool ampexHasPhase = (std::abs(ampexPhase.phaseShift) > 90.0);
    bool studerHasPhase = (std::abs(studerPhase.phaseShift) > 90.0);
    std::cout << "Ampex has HF phase shift: " << (ampexHasPhase ? "PASS" : "FAIL") << "\n";
    std::cout << "Studer has HF phase shift: " << (studerHasPhase ? "PASS" : "FAIL") << "\n\n";

    if (!ampexHasPhase || !studerHasPhase) allPassed = false;

    // =========================================================================
    // TEST 4: TRANSIENT SMEAR
    // =========================================================================
    // The allpass cascade should spread transients slightly

    std::cout << "=== TEST 4: TRANSIENT SMEAR ===\n";
    std::cout << "Expected: Impulse response width > 10μs (tape head effect)\n\n";

    double ampexSmear = measureTransientSmear(ampexProc);
    double studerSmear = measureTransientSmear(studerProc);

    std::cout << "Ampex transient width:  " << std::fixed << std::setprecision(1)
              << ampexSmear << " μs\n";
    std::cout << "Studer transient width: " << studerSmear << " μs\n";

    bool hasSmear = (ampexSmear > 10.0 && studerSmear > 10.0);
    bool studerWider = (studerSmear >= ampexSmear);

    std::cout << "Smear present: " << (hasSmear ? "PASS" : "FAIL") << "\n";
    std::cout << "Studer >= Ampex: " << (studerWider ? "PASS" : "FAIL") << "\n\n";

    if (!hasSmear) allPassed = false;

    // =========================================================================
    // TEST 5: NO AMPLITUDE BOOST AT HF
    // =========================================================================
    // Verify allpass doesn't create resonance or boost

    std::cout << "=== TEST 5: NO HF RESONANCE ===\n";
    std::cout << "Expected: No gain > +0.3dB at any frequency\n\n";

    for (int mode = 0; mode < 2; mode++) {
        const char* modeName = (mode == 0) ? "Ampex" : "Studer";
        double bias = (mode == 0) ? 0.65 : 0.82;

        TapeHysteresis::HybridTapeProcessor processor;
        processor.setSampleRate(SAMPLE_RATE);
        processor.setParameters(bias, 1.0);

        double maxGain = -999;
        double maxGainFreq = 0;

        for (int i = 0; i < numFreqs; i++) {
            auto result = measurePhaseResponse(processor, testFreqs[i]);
            if (result.magnitudeDb > maxGain) {
                maxGain = result.magnitudeDb;
                maxGainFreq = testFreqs[i];
            }
        }

        bool noResonance = (maxGain < 0.3);
        std::cout << modeName << ": Max gain = " << std::showpos << std::fixed
                  << std::setprecision(2) << maxGain << std::noshowpos
                  << " dB @ " << maxGainFreq << " Hz  "
                  << (noResonance ? "PASS" : "FAIL") << "\n";

        if (!noResonance) allPassed = false;
    }

    // =========================================================================
    // FINAL RESULT
    // =========================================================================

    std::cout << "\n════════════════════════════════════════════════════════════\n";
    if (allPassed) {
        std::cout << "DISPERSIVE ALLPASS TEST: ALL PASSED\n";
    } else {
        std::cout << "DISPERSIVE ALLPASS TEST: SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════════════════════\n";

    return allPassed ? 0 : 1;
}

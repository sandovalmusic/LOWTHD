/**
 * Test_HFPhaseShift.cpp
 *
 * Preliminary tests for adding HF phase smear to tape emulation.
 *
 * Tests three approaches:
 * 1. Allpass filter chain (phase shift without amplitude change)
 * 2. First-order lowpass with phase (amplitude + phase coupled)
 * 3. Frequency-dependent delay (dispersive delay line)
 *
 * Measures:
 * - Phase shift at various frequencies
 * - Group delay characteristics
 * - Impact on transients
 * - Amplitude flatness (should remain flat for allpass)
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <complex>

constexpr double SAMPLE_RATE = 96000.0;
constexpr double PI = 3.14159265358979323846;

// ============================================================================
// APPROACH 1: Allpass Filter Chain
// ============================================================================
// Adds phase shift without changing amplitude
// Can be tuned to specific frequency ranges

class AllpassFilter {
public:
    void setFrequency(double freq, double sampleRate) {
        // First-order allpass: H(z) = (a + z^-1) / (1 + a*z^-1)
        // Phase shift is 180° at DC, 0° at Nyquist, 90° at the tuning frequency
        double w0 = 2.0 * PI * freq / sampleRate;
        coefficient = (1.0 - std::tan(w0 / 2.0)) / (1.0 + std::tan(w0 / 2.0));
        z1 = 0.0;
    }

    void reset() { z1 = 0.0; }

    double process(double input) {
        double output = coefficient * input + z1;
        z1 = input - coefficient * output;
        return output;
    }

private:
    double coefficient = 0.0;
    double z1 = 0.0;
};

// ============================================================================
// APPROACH 2: Gentle Lowpass (Phase + Amplitude coupled)
// ============================================================================
// Simple first-order lowpass - adds phase shift but also amplitude rolloff

class GentleLowpass {
public:
    void setFrequency(double freq, double sampleRate) {
        double w0 = 2.0 * PI * freq / sampleRate;
        double alpha = std::sin(w0) / 2.0;

        b0 = (1.0 - std::cos(w0)) / 2.0;
        b1 = 1.0 - std::cos(w0);
        b2 = (1.0 - std::cos(w0)) / 2.0;
        double a0 = 1.0 + alpha;
        a1 = -2.0 * std::cos(w0);
        a2 = 1.0 - alpha;

        // Normalize
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;

        z1 = z2 = 0.0;
    }

    void reset() { z1 = z2 = 0.0; }

    double process(double input) {
        double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        return output;
    }

private:
    double b0 = 1, b1 = 0, b2 = 0;
    double a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;
};

// ============================================================================
// APPROACH 3: Frequency-Dependent Delay (Dispersion)
// ============================================================================
// Higher frequencies get slightly more delay - simulates tape head effects

class DispersiveDelay {
public:
    void configure(double maxDelayUs, double cornerFreq, double sampleRate) {
        // Allpass-based dispersion: delay increases with frequency
        // This creates the "soft focus" effect on transients
        maxDelaySamples = maxDelayUs * 1e-6 * sampleRate;

        // Use cascaded allpass filters to create frequency-dependent delay
        for (int i = 0; i < 4; i++) {
            // Spread tuning frequencies across HF range
            double freq = cornerFreq * std::pow(2.0, i * 0.5);  // 4k, 5.6k, 8k, 11k
            allpass[i].setFrequency(freq, sampleRate);
        }
    }

    void reset() {
        for (int i = 0; i < 4; i++) allpass[i].reset();
    }

    double process(double input) {
        double output = input;
        for (int i = 0; i < 4; i++) {
            output = allpass[i].process(output);
        }
        return output;
    }

private:
    AllpassFilter allpass[4];
    double maxDelaySamples = 0;
};

// ============================================================================
// MEASUREMENT FUNCTIONS
// ============================================================================

struct PhaseResult {
    double frequency;
    double phaseShift;   // degrees
    double groupDelay;   // samples
    double magnitude;    // linear
    double magnitudeDb;  // dB
};

template<typename T>
PhaseResult measurePhase(T& filter, double frequency) {
    filter.reset();

    int samplesPerCycle = static_cast<int>(SAMPLE_RATE / frequency);
    int totalSamples = 100 * samplesPerCycle;
    int skipSamples = 50 * samplesPerCycle;
    int analysisSamples = totalSamples - skipSamples;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; i++) {
        double phase = 2.0 * PI * frequency * i / SAMPLE_RATE;
        double input = std::sin(phase);
        output[i] = filter.process(input);
    }

    // DFT at fundamental
    double real = 0, imag = 0;
    for (int i = skipSamples; i < totalSamples; i++) {
        double phase = 2.0 * PI * frequency * i / SAMPLE_RATE;
        real += output[i] * std::cos(phase);
        imag += output[i] * std::sin(phase);
    }
    real /= analysisSamples;
    imag /= analysisSamples;

    PhaseResult result;
    result.frequency = frequency;
    result.magnitude = 2.0 * std::sqrt(real * real + imag * imag);
    result.magnitudeDb = 20.0 * std::log10(result.magnitude);
    result.phaseShift = std::atan2(-imag, real) * 180.0 / PI;

    // Group delay = -d(phase)/d(frequency)
    // Approximate by measuring at nearby frequency
    double df = frequency * 0.01;
    filter.reset();
    for (int i = 0; i < totalSamples; i++) {
        double phase = 2.0 * PI * (frequency + df) * i / SAMPLE_RATE;
        output[i] = filter.process(std::sin(phase));
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

template<typename T>
double measureTransientSmear(T& filter) {
    filter.reset();

    // Generate a click (impulse)
    std::vector<double> output(1000);
    output[0] = filter.process(1.0);
    for (int i = 1; i < 1000; i++) {
        output[i] = filter.process(0.0);
    }

    // Measure how spread out the energy is
    // Find peak and measure width at -6dB
    double peak = 0;
    int peakIdx = 0;
    for (int i = 0; i < 1000; i++) {
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
    for (int i = peakIdx; i < 1000; i++) {
        if (std::abs(output[i]) >= threshold) endIdx = i;
        else break;
    }

    return (endIdx - startIdx) / (SAMPLE_RATE / 1000.0);  // Width in ms
}

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     HF PHASE SHIFT APPROACH COMPARISON                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    double testFreqs[] = { 1000, 2000, 4000, 6000, 8000, 10000, 12000, 15000 };
    int numFreqs = 8;

    // =========================================================================
    // APPROACH 1: Single Allpass at 6kHz
    // =========================================================================
    std::cout << "=== APPROACH 1: Single Allpass @ 6kHz ===\n";
    std::cout << "Freq(Hz)   Phase(°)   GroupDelay(samp)   Gain(dB)\n";
    std::cout << "----------------------------------------------------\n";

    AllpassFilter ap1;
    ap1.setFrequency(6000, SAMPLE_RATE);

    for (int i = 0; i < numFreqs; i++) {
        auto r = measurePhase(ap1, testFreqs[i]);
        std::cout << std::setw(7) << r.frequency << "    "
                  << std::fixed << std::setprecision(1) << std::setw(6) << r.phaseShift << "      "
                  << std::setprecision(2) << std::setw(6) << r.groupDelay << "            "
                  << std::showpos << std::setprecision(2) << r.magnitudeDb << std::noshowpos << "\n";
    }

    double smear1 = measureTransientSmear(ap1);
    std::cout << "Transient smear: " << std::setprecision(3) << smear1 << " ms\n\n";

    // =========================================================================
    // APPROACH 2: Cascaded Allpass (2 stages at 4kHz and 8kHz)
    // =========================================================================
    std::cout << "=== APPROACH 2: Cascaded Allpass @ 4kHz + 8kHz ===\n";
    std::cout << "Freq(Hz)   Phase(°)   GroupDelay(samp)   Gain(dB)\n";
    std::cout << "----------------------------------------------------\n";

    struct CascadedAllpass {
        AllpassFilter ap1, ap2;
        void reset() { ap1.reset(); ap2.reset(); }
        double process(double x) { return ap2.process(ap1.process(x)); }
    } ap2;
    ap2.ap1.setFrequency(4000, SAMPLE_RATE);
    ap2.ap2.setFrequency(8000, SAMPLE_RATE);

    for (int i = 0; i < numFreqs; i++) {
        auto r = measurePhase(ap2, testFreqs[i]);
        std::cout << std::setw(7) << r.frequency << "    "
                  << std::fixed << std::setprecision(1) << std::setw(6) << r.phaseShift << "      "
                  << std::setprecision(2) << std::setw(6) << r.groupDelay << "            "
                  << std::showpos << std::setprecision(2) << r.magnitudeDb << std::noshowpos << "\n";
    }

    double smear2 = measureTransientSmear(ap2);
    std::cout << "Transient smear: " << std::setprecision(3) << smear2 << " ms\n\n";

    // =========================================================================
    // APPROACH 3: Dispersive Delay (4-stage allpass cascade)
    // =========================================================================
    std::cout << "=== APPROACH 3: Dispersive Delay (4-stage) ===\n";
    std::cout << "Freq(Hz)   Phase(°)   GroupDelay(samp)   Gain(dB)\n";
    std::cout << "----------------------------------------------------\n";

    DispersiveDelay dd;
    dd.configure(20.0, 4000, SAMPLE_RATE);  // 20μs max, corner at 4kHz

    for (int i = 0; i < numFreqs; i++) {
        auto r = measurePhase(dd, testFreqs[i]);
        std::cout << std::setw(7) << r.frequency << "    "
                  << std::fixed << std::setprecision(1) << std::setw(6) << r.phaseShift << "      "
                  << std::setprecision(2) << std::setw(6) << r.groupDelay << "            "
                  << std::showpos << std::setprecision(2) << r.magnitudeDb << std::noshowpos << "\n";
    }

    double smear3 = measureTransientSmear(dd);
    std::cout << "Transient smear: " << std::setprecision(3) << smear3 << " ms\n\n";

    // =========================================================================
    // APPROACH 4: Gentle Lowpass at 20kHz (for comparison - has amplitude effect)
    // =========================================================================
    std::cout << "=== APPROACH 4: Gentle Lowpass @ 20kHz (reference) ===\n";
    std::cout << "Freq(Hz)   Phase(°)   GroupDelay(samp)   Gain(dB)\n";
    std::cout << "----------------------------------------------------\n";

    GentleLowpass lp;
    lp.setFrequency(20000, SAMPLE_RATE);

    for (int i = 0; i < numFreqs; i++) {
        auto r = measurePhase(lp, testFreqs[i]);
        std::cout << std::setw(7) << r.frequency << "    "
                  << std::fixed << std::setprecision(1) << std::setw(6) << r.phaseShift << "      "
                  << std::setprecision(2) << std::setw(6) << r.groupDelay << "            "
                  << std::showpos << std::setprecision(2) << r.magnitudeDb << std::noshowpos << "\n";
    }

    double smear4 = measureTransientSmear(lp);
    std::cout << "Transient smear: " << std::setprecision(3) << smear4 << " ms\n\n";

    // =========================================================================
    // SUMMARY
    // =========================================================================
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                      SUMMARY                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";

    std::cout << "Approach                      Phase@8kHz   Smear    Flat?\n";
    std::cout << "------------------------------------------------------------\n";

    ap1.setFrequency(6000, SAMPLE_RATE);
    auto r1 = measurePhase(ap1, 8000);
    std::cout << "1. Single Allpass @ 6kHz      " << std::setw(6) << r1.phaseShift << "°     "
              << std::setprecision(3) << smear1 << "ms   YES\n";

    ap2.ap1.setFrequency(4000, SAMPLE_RATE);
    ap2.ap2.setFrequency(8000, SAMPLE_RATE);
    auto r2 = measurePhase(ap2, 8000);
    std::cout << "2. Cascaded Allpass 4k+8k     " << std::setw(6) << r2.phaseShift << "°     "
              << std::setprecision(3) << smear2 << "ms   YES\n";

    dd.configure(20.0, 4000, SAMPLE_RATE);
    auto r3 = measurePhase(dd, 8000);
    std::cout << "3. Dispersive (4-stage)       " << std::setw(6) << r3.phaseShift << "°     "
              << std::setprecision(3) << smear3 << "ms   YES\n";

    lp.setFrequency(20000, SAMPLE_RATE);
    auto r4 = measurePhase(lp, 8000);
    std::cout << "4. Lowpass @ 20kHz            " << std::setw(6) << r4.phaseShift << "°     "
              << std::setprecision(3) << smear4 << "ms   NO (rolls off)\n";

    std::cout << "\nRECOMMENDATION:\n";
    std::cout << "Approach 2 (Cascaded Allpass) or 3 (Dispersive) provide the most\n";
    std::cout << "realistic HF phase smear without affecting amplitude response.\n";
    std::cout << "The 4-stage dispersive delay gives smoother frequency-dependent\n";
    std::cout << "group delay, closer to real tape head behavior.\n";

    return 0;
}

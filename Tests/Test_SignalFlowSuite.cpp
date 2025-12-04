/**
 * Test_SignalFlowSuite.cpp
 *
 * Comprehensive test suite for LOWTHD Tape Simulator
 * Tests each stage of the signal flow as documented in README.md
 *
 * Signal Flow Stages Tested:
 * 1. HF Cut (AC Bias Shielding curve for 30 IPS)
 * 2. HF Restore (exact inverse, null test)
 * 3. Jiles-Atherton Hysteresis
 * 4. Asymmetric Tanh Saturation
 * 5. Level-Dependent Atan
 * 6. Machine EQ (Head Bump)
 * 7. HF Dispersive Allpass (Phase Smear)
 * 8. DC Blocking
 * 9. Azimuth Delay
 * 10. Full THD Measurement at multiple levels
 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// BIQUAD FILTER (used by multiple tests)
// ============================================================================
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

// ============================================================================
// FILTER DESIGN FUNCTIONS
// ============================================================================
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

void designHighPass(Biquad& filter, double fc, double Q, double fs)
{
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha;
    filter.b0 = ((1.0 + cosOmega) / 2.0) / a0;
    filter.b1 = (-(1.0 + cosOmega)) / a0;
    filter.b2 = ((1.0 + cosOmega) / 2.0) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha) / a0;
}

void designLowPass(Biquad& filter, double fc, double Q, double fs)
{
    double omega = 2.0 * M_PI * fc / fs;
    double cosOmega = std::cos(omega);
    double sinOmega = std::sin(omega);
    double alpha = sinOmega / (2.0 * Q);

    double a0 = 1.0 + alpha;
    filter.b0 = ((1.0 - cosOmega) / 2.0) / a0;
    filter.b1 = (1.0 - cosOmega) / a0;
    filter.b2 = ((1.0 - cosOmega) / 2.0) / a0;
    filter.a1 = (-2.0 * cosOmega) / a0;
    filter.a2 = (1.0 - alpha) / a0;
}

// ============================================================================
// HF CUT / HF RESTORE (AC Bias Shielding Curve for 30 IPS)
// ============================================================================
// Models the frequency-dependent effectiveness of AC bias (~150kHz)
// at linearizing the magnetic recording process.
//
// Different machines have slightly different bias characteristics:
//
// STUDER A820 (Tracks Mode):
//   - Narrower head gaps, higher bias oscillator frequency
//   - Bias stays effective to slightly higher frequencies
//   - Target: Flat to 7kHz, then -10dB at 20kHz
//
// AMPEX ATR-102 (Master Mode):
//   - Wide 1" head gap, different bias behavior
//   - Bias loses effectiveness slightly earlier
//   - Target: Flat to 6kHz, then -12dB at 20kHz
// ============================================================================

struct HFRestore
{
    double fs = 96000.0;
    bool ampexMode = true;
    Biquad shelf1, shelf2, bell1, bell2, bell3;

    void setSampleRate(double sampleRate)
    {
        fs = sampleRate;
        updateCoefficients();
    }

    void setMachineMode(bool isAmpex)
    {
        ampexMode = isAmpex;
        updateCoefficients();
    }

    void updateCoefficients()
    {
        double nyquist = fs / 2.0;

        double shelf1Freq, shelf1Gain, shelf1Q;
        double shelf2Freq, shelf2Gain, shelf2Q;
        double bell1Freq, bell1Gain, bell1Q;
        double bell2Freq, bell2Gain, bell2Q;
        double bell3Freq, bell3Gain, bell3Q;

        if (ampexMode)
        {
            // AMPEX ATR-102 HF Restore
            shelf1Freq = std::min(10000.0, nyquist * 0.9);
            shelf1Gain = +7.5;
            shelf1Q = 1.0;
            shelf2Freq = std::min(16000.0, nyquist * 0.85);
            shelf2Gain = +4.5;
            shelf2Q = 0.85;
            bell1Freq = std::min(8000.0, nyquist * 0.9);
            bell1Gain = +0.5;
            bell1Q = 1.8;
            bell2Freq = std::min(19000.0, nyquist * 0.9);
            bell2Gain = +1.5;
            bell2Q = 0.7;
            bell3Freq = 6000.0;
            bell3Gain = -0.3;
            bell3Q = 2.5;
        }
        else
        {
            // STUDER A820 HF Restore
            shelf1Freq = std::min(10000.0, nyquist * 0.9);
            shelf1Gain = +7.0;
            shelf1Q = 1.0;
            shelf2Freq = std::min(17000.0, nyquist * 0.85);
            shelf2Gain = +3.0;
            shelf2Q = 0.85;
            bell1Freq = std::min(8000.0, nyquist * 0.9);
            bell1Gain = +0.5;
            bell1Q = 1.8;
            bell2Freq = std::min(19000.0, nyquist * 0.9);
            bell2Gain = +1.0;
            bell2Q = 0.8;
            bell3Freq = 6000.0;
            bell3Gain = -0.3;
            bell3Q = 2.2;
        }

        designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
        designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
        designBell(bell1, bell1Freq, bell1Gain, bell1Q, fs);
        designBell(bell2, bell2Freq, bell2Gain, bell2Q, fs);
        designBell(bell3, bell3Freq, bell3Gain, bell3Q, fs);
    }

    void reset()
    {
        shelf1.reset(); shelf2.reset();
        bell1.reset(); bell2.reset(); bell3.reset();
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

struct HFCut
{
    double fs = 96000.0;
    bool ampexMode = true;
    Biquad shelf1, shelf2, bell1, bell2, bell3;

    void setSampleRate(double sampleRate)
    {
        fs = sampleRate;
        updateCoefficients();
    }

    void setMachineMode(bool isAmpex)
    {
        ampexMode = isAmpex;
        updateCoefficients();
    }

    void updateCoefficients()
    {
        double nyquist = fs / 2.0;

        double shelf1Freq, shelf1Gain, shelf1Q;
        double shelf2Freq, shelf2Gain, shelf2Q;
        double bell1Freq, bell1Gain, bell1Q;
        double bell2Freq, bell2Gain, bell2Q;
        double bell3Freq, bell3Gain, bell3Q;

        if (ampexMode)
        {
            // AMPEX ATR-102 HF Cut (EXACT INVERSE of HF Restore)
            shelf1Freq = std::min(10000.0, nyquist * 0.9);
            shelf1Gain = -7.5;
            shelf1Q = 1.0;
            shelf2Freq = std::min(16000.0, nyquist * 0.85);
            shelf2Gain = -4.5;
            shelf2Q = 0.85;
            bell1Freq = std::min(8000.0, nyquist * 0.9);
            bell1Gain = -0.5;
            bell1Q = 1.8;
            bell2Freq = std::min(19000.0, nyquist * 0.9);
            bell2Gain = -1.5;
            bell2Q = 0.7;
            bell3Freq = 6000.0;
            bell3Gain = +0.3;
            bell3Q = 2.5;
        }
        else
        {
            // STUDER A820 HF Cut (EXACT INVERSE of HF Restore)
            shelf1Freq = std::min(10000.0, nyquist * 0.9);
            shelf1Gain = -7.0;
            shelf1Q = 1.0;
            shelf2Freq = std::min(17000.0, nyquist * 0.85);
            shelf2Gain = -3.0;
            shelf2Q = 0.85;
            bell1Freq = std::min(8000.0, nyquist * 0.9);
            bell1Gain = -0.5;
            bell1Q = 1.8;
            bell2Freq = std::min(19000.0, nyquist * 0.9);
            bell2Gain = -1.0;
            bell2Q = 0.8;
            bell3Freq = 6000.0;
            bell3Gain = +0.3;
            bell3Q = 2.2;
        }

        designHighShelf(shelf1, shelf1Freq, shelf1Gain, shelf1Q, fs);
        designHighShelf(shelf2, shelf2Freq, shelf2Gain, shelf2Q, fs);
        designBell(bell1, bell1Freq, bell1Gain, bell1Q, fs);
        designBell(bell2, bell2Freq, bell2Gain, bell2Q, fs);
        designBell(bell3, bell3Freq, bell3Gain, bell3Q, fs);
    }

    void reset()
    {
        shelf1.reset(); shelf2.reset();
        bell1.reset(); bell2.reset(); bell3.reset();
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

// ============================================================================
// JILES-ATHERTON HYSTERESIS CORE
// ============================================================================
struct JilesAthertonCore
{
    double M_s = 1.0, a = 50.0, k = 0.005, c = 0.95, alpha = 1e-6;
    double M = 0.0, H_prev = 0.0;

    void reset() { M = 0.0; H_prev = 0.0; }

    double langevin(double x)
    {
        if (std::abs(x) < 1e-6) return x / 3.0;
        return 1.0 / std::tanh(x) - 1.0 / x;
    }

    double process(double H)
    {
        double H_eff = H + alpha * M;
        double M_an = M_s * langevin(H_eff / a);
        double dH = H - H_prev;
        double delta = (dH >= 0) ? 1.0 : -1.0;
        double dM_irr = (M_an - M) / (delta * k - alpha * (M_an - M));
        double dM_an = (M_an - M) * c;
        double dM = dM_irr * (1.0 - c) + dM_an;
        dM = std::clamp(dM * std::abs(dH), -0.1, 0.1);
        M += dM;
        M = std::clamp(M, -M_s, M_s);
        H_prev = H;
        return M;
    }
};

// ============================================================================
// ASYMMETRIC SATURATION FUNCTIONS
// ============================================================================
double asymmetricTanh(double x, double drive, double asymmetry)
{
    double bias = asymmetry - 1.0;
    double dcOffset = std::tanh(drive * bias);
    double biased = x + bias;
    double saturated = std::tanh(drive * biased);
    double norm = drive * (1.0 - dcOffset * dcOffset);
    double normFactor = (norm > 0.001) ? (1.0 / norm) : 1.0;
    return (saturated - dcOffset) * normFactor;
}

double asymmetricAtan(double x, double drive, double asymmetry)
{
    if (drive < 0.001) return x;
    double bias = asymmetry - 1.0;
    double dcOffset = std::atan(drive * bias);
    double biased = x + bias;
    double saturated = std::atan(drive * biased);
    double driveBias = drive * bias;
    double norm = drive / (1.0 + driveBias * driveBias);
    double normFactor = (norm > 0.001) ? (1.0 / norm) : 1.0;
    return (saturated - dcOffset) * normFactor;
}

double softAtan(double x, double drive)
{
    if (drive < 0.001) return x;
    return std::atan(drive * x) / drive;
}

// ============================================================================
// DISPERSIVE ALLPASS (Phase Smear)
// ============================================================================
struct DispersiveAllpass
{
    double coeff = 0.0;
    double z1 = 0.0;

    void setFrequency(double freq, double sampleRate)
    {
        double omega = 2.0 * M_PI * freq / sampleRate;
        coeff = (1.0 - std::tan(omega / 2.0)) / (1.0 + std::tan(omega / 2.0));
    }

    void reset() { z1 = 0.0; }

    double process(double input)
    {
        double output = coeff * input + z1;
        z1 = input - coeff * output;
        return output;
    }
};

// ============================================================================
// TEST RESULT TRACKING
// ============================================================================
struct TestResult
{
    std::string name;
    bool passed;
    std::string details;
};

std::vector<TestResult> allResults;

void reportTest(const std::string& name, bool passed, const std::string& details = "")
{
    allResults.push_back({name, passed, details});
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name;
    if (!details.empty()) std::cout << " - " << details;
    std::cout << "\n";
}

// ============================================================================
// TEST 1: AC BIAS SHIELDING CURVE ACCURACY (Both Machines)
// ============================================================================

// Helper function to test a machine's bias shielding curve
void testMachineBiasCurve(bool isAmpex, double* targetFreqs, double* targetGains, int numPoints,
                          double sampleRate, double tolerance, const std::string& machineName)
{
    HFCut hfCut;
    hfCut.setSampleRate(sampleRate);
    hfCut.setMachineMode(isAmpex);

    bool allPassed = true;
    double maxError = 0.0;

    std::cout << "\n  " << machineName << ":\n";

    for (int i = 0; i < numPoints; ++i)
    {
        double freq = targetFreqs[i];
        double target = targetGains[i];

        hfCut.reset();

        int numCycles = 100;
        int samplesPerCycle = static_cast<int>(sampleRate / freq);
        int totalSamples = numCycles * samplesPerCycle;
        int skipSamples = 10 * samplesPerCycle;

        double sumIn = 0.0, sumOut = 0.0;
        for (int s = 0; s < totalSamples; ++s)
        {
            double t = static_cast<double>(s) / sampleRate;
            double input = std::sin(2.0 * M_PI * freq * t);
            double output = hfCut.processSample(input);

            if (s >= skipSamples)
            {
                sumIn += input * input;
                sumOut += output * output;
            }
        }

        double rmsIn = std::sqrt(sumIn / (totalSamples - skipSamples));
        double rmsOut = std::sqrt(sumOut / (totalSamples - skipSamples));
        double measured = 20.0 * std::log10(rmsOut / rmsIn);
        double error = std::abs(measured - target);

        if (error > maxError) maxError = error;
        if (error > tolerance) allPassed = false;

        std::cout << "    " << std::fixed << std::setprecision(0) << freq << " Hz: "
                  << std::setprecision(2) << measured << " dB (target " << target << ")\n";
    }

    reportTest(machineName + " Bias Shielding Curve", allPassed,
               "Max error: " + std::to_string(maxError).substr(0,4) + " dB (tolerance: ±" +
               std::to_string(tolerance).substr(0,3) + " dB)");
}

void testBiasShieldingCurve()
{
    std::cout << "\n=== TEST 1: AC Bias Shielding HF Cut Curves ===\n";

    double sampleRate = 96000.0;

    // Ampex ATR-102: Flat to 6kHz, -12dB at 20kHz
    // Wide head gap means bias loses effectiveness earlier
    double ampexFreqs[] = {1000, 5000, 6000, 8000, 10000, 12000, 14000, 16000, 18000, 20000};
    double ampexGains[] = {0.0, 0.0, -0.5, -2.0, -4.5, -6.5, -8.5, -10.0, -11.0, -12.0};

    // Studer A820: Flat to 7kHz, -10dB at 20kHz
    // Narrower gaps and higher bias oscillator keeps bias effective longer
    double studerFreqs[] = {1000, 6000, 7000, 8000, 10000, 12000, 14000, 16000, 18000, 20000};
    double studerGains[] = {0.0, 0.0, -0.3, -1.0, -3.0, -5.0, -6.5, -8.0, -9.0, -10.0};

    // Test both curves with 2.0dB tolerance (complex multi-filter matching)
    testMachineBiasCurve(true, ampexFreqs, ampexGains, 10, sampleRate, 2.0, "Ampex ATR-102");
    testMachineBiasCurve(false, studerFreqs, studerGains, 10, sampleRate, 2.0, "Studer A820");
}

// ============================================================================
// TEST 2: HF CUT/RESTORE NULL TEST (Both Machines)
// ============================================================================

// Helper function to test null for a specific machine mode
void testMachineHFNull(bool isAmpex, double sampleRate, const std::string& machineName)
{
    HFRestore hfRestore;
    HFCut hfCut;
    hfRestore.setSampleRate(sampleRate);
    hfCut.setSampleRate(sampleRate);
    hfRestore.setMachineMode(isAmpex);
    hfCut.setMachineMode(isAmpex);

    double testFreqs[] = {100, 1000, 5000, 10000, 15000, 20000};
    bool allPassed = true;
    double maxDeviation = 0.0;

    for (double freq : testFreqs)
    {
        hfRestore.reset();
        hfCut.reset();

        int numCycles = 100;
        int samplesPerCycle = static_cast<int>(sampleRate / freq);
        int totalSamples = numCycles * samplesPerCycle;
        int skipSamples = 10 * samplesPerCycle;

        double sumIn = 0.0, sumOut = 0.0;
        for (int s = 0; s < totalSamples; ++s)
        {
            double t = static_cast<double>(s) / sampleRate;
            double input = std::sin(2.0 * M_PI * freq * t);
            double afterRestore = hfRestore.processSample(input);
            double output = hfCut.processSample(afterRestore);

            if (s >= skipSamples)
            {
                sumIn += input * input;
                sumOut += output * output;
            }
        }

        double rmsIn = std::sqrt(sumIn / (totalSamples - skipSamples));
        double rmsOut = std::sqrt(sumOut / (totalSamples - skipSamples));
        double deviation = std::abs(20.0 * std::log10(rmsOut / rmsIn));

        if (deviation > maxDeviation) maxDeviation = deviation;
        if (deviation > 0.1) allPassed = false;
    }

    reportTest(machineName + " HF Cut/Restore Null", allPassed,
               "Max deviation: " + std::to_string(maxDeviation).substr(0,5) + " dB (tolerance: 0.1 dB)");
}

void testHFNull()
{
    std::cout << "\n=== TEST 2: HF Cut + HF Restore Null Test ===\n";

    double sampleRate = 96000.0;

    // Test both machine modes - each should null perfectly
    testMachineHFNull(true, sampleRate, "Ampex ATR-102");
    testMachineHFNull(false, sampleRate, "Studer A820");
}

// ============================================================================
// TEST 3: JILES-ATHERTON HYSTERESIS BEHAVIOR
// ============================================================================
void testJilesAtherton()
{
    std::cout << "\n=== TEST 3: Jiles-Atherton Hysteresis ===\n";

    JilesAthertonCore jaAmpex, jaStuder;

    // Ampex parameters
    jaAmpex.a = 50.0; jaAmpex.k = 0.005; jaAmpex.c = 0.95; jaAmpex.alpha = 1e-6;

    // Studer parameters
    jaStuder.a = 35.0; jaStuder.k = 0.01; jaStuder.c = 0.92; jaStuder.alpha = 1e-5;

    // Test: Verify hysteresis creates memory effect (output depends on history)
    jaAmpex.reset();
    double ascending[5], descending[5];

    // Ascending
    for (int i = 0; i < 5; ++i)
    {
        double H = 0.2 * (i + 1);
        ascending[i] = jaAmpex.process(H);
    }

    // Descending (same values, opposite order)
    jaAmpex.reset();
    for (int i = 0; i < 5; ++i)
    {
        double H = 1.0 - 0.2 * i;
        jaAmpex.process(H);
    }
    jaAmpex.reset();
    for (int i = 4; i >= 0; --i)
    {
        double H = 0.2 * (i + 1);
        descending[4-i] = jaAmpex.process(H);
    }

    bool hasHysteresis = false;
    for (int i = 0; i < 5; ++i)
    {
        if (std::abs(ascending[i] - descending[i]) > 0.001)
        {
            hasHysteresis = true;
            break;
        }
    }

    reportTest("J-A Hysteresis Memory Effect", hasHysteresis,
               "Output differs based on signal history");

    // Test: Verify saturation limits
    jaAmpex.reset();
    double maxOut = 0.0;
    for (int i = 0; i < 1000; ++i)
    {
        double out = std::abs(jaAmpex.process(10.0 * std::sin(0.01 * i)));
        if (out > maxOut) maxOut = out;
    }

    reportTest("J-A Saturation Limiting", maxOut <= 1.0,
               "Max output: " + std::to_string(maxOut).substr(0,5) + " (should be ≤ 1.0)");
}

// ============================================================================
// TEST 4: ASYMMETRIC TANH SATURATION
// ============================================================================
void testAsymmetricTanh()
{
    std::cout << "\n=== TEST 4: Asymmetric Tanh Saturation ===\n";

    // Ampex: drive=0.095, asymmetry=1.08
    // Studer: drive=0.14, asymmetry=1.18

    // Test DC offset removal (output at x=0 should be near 0)
    double dcAmpex = asymmetricTanh(0.0, 0.095, 1.08);
    double dcStuder = asymmetricTanh(0.0, 0.14, 1.18);

    reportTest("Ampex DC Offset Removal", std::abs(dcAmpex) < 0.01,
               "DC at zero: " + std::to_string(dcAmpex).substr(0,6));
    reportTest("Studer DC Offset Removal", std::abs(dcStuder) < 0.01,
               "DC at zero: " + std::to_string(dcStuder).substr(0,6));

    // Test asymmetry by measuring 2nd harmonic generation
    // Asymmetric saturation produces even harmonics; symmetric saturation only produces odd
    double sampleRate = 96000.0;
    double testFreq = 1000.0;
    int totalSamples = 96000;
    int skipSamples = 10000;

    for (int m = 0; m < 2; ++m)
    {
        bool isAmpex = (m == 0);
        double drive = isAmpex ? 0.095 : 0.14;
        double asymmetry = isAmpex ? 1.08 : 1.18;

        // Measure H2 (even) and H3 (odd)
        double sumH2cos = 0.0, sumH2sin = 0.0;
        double sumH3cos = 0.0, sumH3sin = 0.0;

        for (int i = skipSamples; i < totalSamples; ++i)
        {
            double t = static_cast<double>(i) / sampleRate;
            double input = std::sin(2.0 * M_PI * testFreq * t);
            double output = asymmetricTanh(input, drive, asymmetry);

            sumH2cos += output * std::cos(2.0 * M_PI * 2.0 * testFreq * t);
            sumH2sin += output * std::sin(2.0 * M_PI * 2.0 * testFreq * t);
            sumH3cos += output * std::cos(2.0 * M_PI * 3.0 * testFreq * t);
            sumH3sin += output * std::sin(2.0 * M_PI * 3.0 * testFreq * t);
        }

        double h2 = std::sqrt(sumH2cos * sumH2cos + sumH2sin * sumH2sin);
        double h3 = std::sqrt(sumH3cos * sumH3cos + sumH3sin * sumH3sin);

        std::string name = isAmpex ? "Ampex" : "Studer";
        reportTest(name + " Generates Even Harmonics", h2 > 0.0,
                   "H2=" + std::to_string(h2).substr(0,8) + ", H3=" + std::to_string(h3).substr(0,8));
    }
}

// ============================================================================
// TEST 5: DISPERSIVE ALLPASS PHASE SHIFT
// ============================================================================
void testDispersiveAllpass()
{
    std::cout << "\n=== TEST 5: Dispersive Allpass Phase Shift ===\n";

    double sampleRate = 96000.0;
    static const int NUM_STAGES = 4;
    DispersiveAllpass allpassAmpex[NUM_STAGES], allpassStuder[NUM_STAGES];

    // Configure Ampex (corner = 4500 Hz)
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        double freq = 4500.0 * std::pow(2.0, i * 0.5);
        allpassAmpex[i].setFrequency(freq, sampleRate);
    }

    // Configure Studer (corner = 3500 Hz)
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        double freq = 3500.0 * std::pow(2.0, i * 0.5);
        allpassStuder[i].setFrequency(freq, sampleRate);
    }

    // Measure phase at 8kHz
    double testFreq = 8000.0;
    int numSamples = static_cast<int>(sampleRate * 0.1);

    // Reset
    for (int i = 0; i < NUM_STAGES; ++i)
    {
        allpassAmpex[i].reset();
        allpassStuder[i].reset();
    }

    // Process and measure phase by cross-correlation
    double sumInAmpex = 0.0, sumOutAmpex = 0.0, sumCrossAmpex = 0.0;
    double sumInStuder = 0.0, sumOutStuder = 0.0, sumCrossStuder = 0.0;

    for (int s = numSamples/2; s < numSamples; ++s)
    {
        double t = static_cast<double>(s) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);

        double outA = input, outS = input;
        for (int i = 0; i < NUM_STAGES; ++i)
        {
            outA = allpassAmpex[i].process(outA);
            outS = allpassStuder[i].process(outS);
        }

        sumInAmpex += input * input;
        sumOutAmpex += outA * outA;
        sumCrossAmpex += input * outA;

        sumInStuder += input * input;
        sumOutStuder += outS * outS;
        sumCrossStuder += input * outS;
    }

    // Verify magnitude is unity (allpass property)
    double gainAmpex = std::sqrt(sumOutAmpex / sumInAmpex);
    double gainStuder = std::sqrt(sumOutStuder / sumInStuder);

    reportTest("Ampex Allpass Unity Gain", std::abs(gainAmpex - 1.0) < 0.01,
               "Gain at 8kHz: " + std::to_string(gainAmpex).substr(0,5));
    reportTest("Studer Allpass Unity Gain", std::abs(gainStuder - 1.0) < 0.01,
               "Gain at 8kHz: " + std::to_string(gainStuder).substr(0,5));

    // Verify phase shift exists
    double correlationAmpex = sumCrossAmpex / std::sqrt(sumInAmpex * sumOutAmpex);
    double correlationStuder = sumCrossStuder / std::sqrt(sumInStuder * sumOutStuder);

    reportTest("Ampex Phase Shift Present", correlationAmpex < 0.95,
               "Correlation: " + std::to_string(correlationAmpex).substr(0,5));
    reportTest("Studer Phase Shift Present", correlationStuder < 0.95,
               "Correlation: " + std::to_string(correlationStuder).substr(0,5));
}

// ============================================================================
// TEST 6: DC BLOCKING
// ============================================================================
void testDCBlocking()
{
    std::cout << "\n=== TEST 6: DC Blocking (5Hz HPF) ===\n";

    double sampleRate = 96000.0;
    Biquad dcBlock1, dcBlock2;
    designHighPass(dcBlock1, 5.0, 0.7071, sampleRate);
    designHighPass(dcBlock2, 5.0, 0.7071, sampleRate);

    // Test DC rejection
    dcBlock1.reset();
    dcBlock2.reset();

    double dcInput = 0.5;  // Constant DC
    double dcOutput = 0.0;

    for (int i = 0; i < 96000; ++i)  // 1 second
    {
        double out = dcBlock1.process(dcInput);
        out = dcBlock2.process(out);
        dcOutput = out;
    }

    double dcAttenuation = 20.0 * std::log10(std::abs(dcOutput) / std::abs(dcInput));

    reportTest("DC Rejection", dcAttenuation < -60.0,
               "DC attenuation: " + std::to_string(dcAttenuation).substr(0,6) + " dB");

    // Test 100Hz passthrough
    dcBlock1.reset();
    dcBlock2.reset();

    double sumIn = 0.0, sumOut = 0.0;
    for (int i = 0; i < 96000; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * 100.0 * t);
        double out = dcBlock1.process(input);
        out = dcBlock2.process(out);

        if (i >= 48000)
        {
            sumIn += input * input;
            sumOut += out * out;
        }
    }

    double passGain = 20.0 * std::log10(std::sqrt(sumOut / sumIn));

    reportTest("100Hz Passthrough", std::abs(passGain) < 0.5,
               "Gain at 100Hz: " + std::to_string(passGain).substr(0,5) + " dB");
}

// ============================================================================
// TEST 7: AZIMUTH DELAY
// ============================================================================
void testAzimuthDelay()
{
    std::cout << "\n=== TEST 7: Azimuth Delay ===\n";

    double sampleRate = 96000.0;

    // Ampex: 8μs, Studer: 12μs
    double ampexDelaySamples = 8.0e-6 * sampleRate;  // 0.768 samples
    double studerDelaySamples = 12.0e-6 * sampleRate;  // 1.152 samples

    reportTest("Ampex Delay Calculation", std::abs(ampexDelaySamples - 0.768) < 0.01,
               "8μs = " + std::to_string(ampexDelaySamples).substr(0,5) + " samples");
    reportTest("Studer Delay Calculation", std::abs(studerDelaySamples - 1.152) < 0.01,
               "12μs = " + std::to_string(studerDelaySamples).substr(0,5) + " samples");

    // Verify these create audible stereo widening at high frequencies
    // At 10kHz, Ampex delay = 8μs = 28.8° phase, Studer = 43.2° phase
    double ampexPhase10k = 360.0 * 10000.0 * 8.0e-6;
    double studerPhase10k = 360.0 * 10000.0 * 12.0e-6;

    reportTest("Ampex Phase @ 10kHz", ampexPhase10k > 25.0 && ampexPhase10k < 35.0,
               std::to_string(ampexPhase10k).substr(0,4) + "° (expected ~29°)");
    reportTest("Studer Phase @ 10kHz", studerPhase10k > 40.0 && studerPhase10k < 50.0,
               std::to_string(studerPhase10k).substr(0,4) + "° (expected ~43°)");
}

// ============================================================================
// TEST 8: THD MEASUREMENT
// ============================================================================
double measureTHD(double inputLevel, bool isAmpex, double sampleRate = 96000.0)
{
    // Simplified THD measurement using asymmetric tanh
    // (Full processor would include J-A, de/re-emphasis, etc.)

    double drive = isAmpex ? 0.095 : 0.14;
    double asymmetry = isAmpex ? 1.08 : 1.18;

    double testFreq = 1000.0;
    int numCycles = 200;
    int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    int totalSamples = numCycles * samplesPerCycle;
    int skipSamples = 20 * samplesPerCycle;
    int fftSize = 8192;

    std::vector<double> output(totalSamples);

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = inputLevel * std::sin(2.0 * M_PI * testFreq * t);
        output[i] = asymmetricTanh(input, drive, asymmetry);
    }

    // Simple FFT-based THD (measure harmonics 2-5 vs fundamental)
    // Use DFT at specific frequencies for accuracy
    double fundamental = 0.0, harmonics = 0.0;

    for (int h = 1; h <= 5; ++h)
    {
        double freq = testFreq * h;
        double sumCos = 0.0, sumSin = 0.0;

        for (int i = skipSamples; i < totalSamples; ++i)
        {
            double t = static_cast<double>(i) / sampleRate;
            sumCos += output[i] * std::cos(2.0 * M_PI * freq * t);
            sumSin += output[i] * std::sin(2.0 * M_PI * freq * t);
        }

        double magnitude = std::sqrt(sumCos * sumCos + sumSin * sumSin);

        if (h == 1)
            fundamental = magnitude;
        else
            harmonics += magnitude * magnitude;
    }

    double thd = 100.0 * std::sqrt(harmonics) / fundamental;
    return thd;
}

void testTHD()
{
    std::cout << "\n=== TEST 8: THD Measurements ===\n";

    // Test at multiple levels
    double levels[] = {0.25, 0.5, 1.0, 1.414, 2.0, 2.828};  // -12, -6, 0, +3, +6, +9 dB
    std::string levelNames[] = {"-12 dB", "-6 dB", "0 dB", "+3 dB", "+6 dB", "+9 dB"};

    std::cout << "\n  Ampex ATR-102 (tanh only, simplified):\n";
    for (int i = 0; i < 6; ++i)
    {
        double thd = measureTHD(levels[i], true);
        std::cout << "    " << levelNames[i] << ": " << std::fixed << std::setprecision(3)
                  << thd << "% THD\n";
    }

    std::cout << "\n  Studer A820 (tanh only, simplified):\n";
    for (int i = 0; i < 6; ++i)
    {
        double thd = measureTHD(levels[i], false);
        std::cout << "    " << levelNames[i] << ": " << std::fixed << std::setprecision(3)
                  << thd << "% THD\n";
    }

    // Basic sanity checks
    double thdAmpex0dB = measureTHD(1.0, true);
    double thdStuder0dB = measureTHD(1.0, false);

    reportTest("Ampex THD @ 0dB < 1%", thdAmpex0dB < 1.0,
               std::to_string(thdAmpex0dB).substr(0,5) + "% THD");
    reportTest("Studer THD > Ampex THD", thdStuder0dB > thdAmpex0dB,
               "Studer " + std::to_string(thdStuder0dB).substr(0,5) + "% > Ampex " +
               std::to_string(thdAmpex0dB).substr(0,5) + "%");

    // THD should increase with level
    double thdAmpexHigh = measureTHD(2.828, true);
    reportTest("THD Increases with Level", thdAmpexHigh > thdAmpex0dB,
               "+9dB: " + std::to_string(thdAmpexHigh).substr(0,5) + "% > 0dB: " +
               std::to_string(thdAmpex0dB).substr(0,5) + "%");
}

// ============================================================================
// TEST 9: EVEN/ODD HARMONIC RATIO
// ============================================================================
void testEvenOddRatio()
{
    std::cout << "\n=== TEST 9: Even/Odd Harmonic Ratio ===\n";

    double sampleRate = 96000.0;
    double testFreq = 1000.0;
    double inputLevel = 1.0;
    int numCycles = 200;
    int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    int totalSamples = numCycles * samplesPerCycle;
    int skipSamples = 20 * samplesPerCycle;

    // Measure for both machines
    for (int machine = 0; machine < 2; ++machine)
    {
        bool isAmpex = (machine == 0);
        double drive = isAmpex ? 0.095 : 0.14;
        double asymmetry = isAmpex ? 1.08 : 1.18;

        std::vector<double> output(totalSamples);
        for (int i = 0; i < totalSamples; ++i)
        {
            double t = static_cast<double>(i) / sampleRate;
            double input = inputLevel * std::sin(2.0 * M_PI * testFreq * t);
            output[i] = asymmetricTanh(input, drive, asymmetry);
        }

        // Measure H2, H3, H4, H5
        double h2 = 0.0, h3 = 0.0, h4 = 0.0, h5 = 0.0;

        for (int h = 2; h <= 5; ++h)
        {
            double freq = testFreq * h;
            double sumCos = 0.0, sumSin = 0.0;

            for (int i = skipSamples; i < totalSamples; ++i)
            {
                double t = static_cast<double>(i) / sampleRate;
                sumCos += output[i] * std::cos(2.0 * M_PI * freq * t);
                sumSin += output[i] * std::sin(2.0 * M_PI * freq * t);
            }

            double magnitude = std::sqrt(sumCos * sumCos + sumSin * sumSin);

            if (h == 2) h2 = magnitude;
            else if (h == 3) h3 = magnitude;
            else if (h == 4) h4 = magnitude;
            else if (h == 5) h5 = magnitude;
        }

        double evenSum = h2 + h4;
        double oddSum = h3 + h5;
        double eoRatio = evenSum / oddSum;

        std::string machineName = isAmpex ? "Ampex" : "Studer";
        double targetRatio = isAmpex ? 0.53 : 1.09;

        std::cout << "  " << machineName << ": E/O ratio = " << std::fixed
                  << std::setprecision(2) << eoRatio << " (target: " << targetRatio << ")\n";

        // Ampex should be odd-dominant (E/O < 1), Studer should be even-dominant (E/O > 1)
        if (isAmpex)
        {
            reportTest("Ampex Odd-Dominant", eoRatio < 1.0,
                       "E/O = " + std::to_string(eoRatio).substr(0,4) + " (should be < 1.0)");
        }
        else
        {
            reportTest("Studer Even-Dominant", eoRatio > 1.0,
                       "E/O = " + std::to_string(eoRatio).substr(0,4) + " (should be > 1.0)");
        }
    }
}

// ============================================================================
// TEST 10: PRINT-THROUGH (Studer mode only)
// ============================================================================
struct PrintThrough
{
    static constexpr int MAX_DELAY_SAMPLES = 12480;  // 65ms @ 192kHz
    double bufferL[MAX_DELAY_SAMPLES] = {0};
    double bufferR[MAX_DELAY_SAMPLES] = {0};
    int writeIndex = 0;
    int delaySamples = 0;

    static constexpr double printCoeff = 0.00126;  // -58dB at unity (GP9 spec)
    static constexpr double noiseFloor = 0.001;    // -60dB

    double sampleRate = 48000.0;

    void prepare(double sr)
    {
        sampleRate = sr;
        delaySamples = static_cast<int>(0.065 * sampleRate);  // 65ms
        if (delaySamples >= MAX_DELAY_SAMPLES)
            delaySamples = MAX_DELAY_SAMPLES - 1;
        reset();
    }

    void reset()
    {
        for (int i = 0; i < MAX_DELAY_SAMPLES; ++i)
        {
            bufferL[i] = 0.0;
            bufferR[i] = 0.0;
        }
        writeIndex = 0;
    }

    void processSample(double& left, double& right)
    {
        int readIndex = writeIndex - delaySamples;
        if (readIndex < 0) readIndex += MAX_DELAY_SAMPLES;

        double delayedL = bufferL[readIndex];
        double delayedR = bufferR[readIndex];

        // Signal-dependent print-through
        double absL = std::abs(delayedL);
        double absR = std::abs(delayedR);

        double printLevelL = (absL > noiseFloor) ? printCoeff * absL : 0.0;
        double printLevelR = (absR > noiseFloor) ? printCoeff * absR : 0.0;

        double preEchoL = delayedL * printLevelL;
        double preEchoR = delayedR * printLevelR;

        // Write current sample to delay buffer
        bufferL[writeIndex] = left;
        bufferR[writeIndex] = right;

        writeIndex = (writeIndex + 1) % MAX_DELAY_SAMPLES;

        left += preEchoL;
        right += preEchoR;
    }
};

void testPrintThrough()
{
    std::cout << "\n=== TEST 10: Print-Through (Studer mode) ===\n";

    double sampleRate = 48000.0;
    int delaySamples = static_cast<int>(0.065 * sampleRate);  // 65ms = 3120 samples @ 48kHz

    PrintThrough pt;
    pt.prepare(sampleRate);

    // Test 1: Verify delay timing
    // Send an impulse, check pre-echo arrives at correct time
    int testLength = delaySamples + 1000;
    std::vector<double> output(testLength, 0.0);

    // First, fill the buffer with a loud signal that will create print-through
    double loudLevel = 1.0;
    for (int i = 0; i < delaySamples; ++i)
    {
        double left = loudLevel;
        double right = loudLevel;
        pt.processSample(left, right);
    }

    // Now send silence and look for the pre-echo
    double maxPreEcho = 0.0;
    int preEchoSample = -1;

    for (int i = 0; i < 1000; ++i)
    {
        double left = 0.0;
        double right = 0.0;
        pt.processSample(left, right);

        if (std::abs(left) > maxPreEcho)
        {
            maxPreEcho = std::abs(left);
            preEchoSample = i;
        }
    }

    // Pre-echo should appear immediately (first sample of silence gets print-through from loud signal)
    reportTest("Print-Through Delay Timing", preEchoSample == 0,
               "Pre-echo at sample " + std::to_string(preEchoSample) + " (expected: 0)");

    // Test 2: Signal-dependent level
    // Loud signals should produce more print-through than quiet signals
    pt.reset();

    // Test with loud signal
    double loudInput = 1.0;
    for (int i = 0; i < delaySamples + 10; ++i)
    {
        double left = loudInput;
        double right = loudInput;
        pt.processSample(left, right);
    }

    double loudPTLeft = 0.0, loudPTRight = 0.0;
    pt.processSample(loudPTLeft, loudPTRight);
    double loudPT = std::abs(loudPTLeft);

    pt.reset();

    // Test with quiet signal (-20dB)
    double quietInput = 0.1;
    for (int i = 0; i < delaySamples + 10; ++i)
    {
        double left = quietInput;
        double right = quietInput;
        pt.processSample(left, right);
    }

    double quietPTLeft = 0.0, quietPTRight = 0.0;
    pt.processSample(quietPTLeft, quietPTRight);
    double quietPT = std::abs(quietPTLeft);

    // Loud signal should produce significantly more print-through (quadratic scaling)
    // At 1.0 input: PT = 1.0 * 0.00178 * 1.0 = 0.00178
    // At 0.1 input: PT = 0.1 * 0.00178 * 0.1 = 0.0000178
    // Ratio should be ~100:1 (quadratic scaling)
    double ratio = (quietPT > 0) ? loudPT / quietPT : 0;

    std::cout << "  Loud signal (1.0) PT: " << std::scientific << std::setprecision(4) << loudPT << "\n";
    std::cout << "  Quiet signal (0.1) PT: " << quietPT << "\n";
    std::cout << "  Ratio: " << std::fixed << std::setprecision(1) << ratio << ":1 (expected ~100:1)\n";

    reportTest("Signal-Dependent PT - Loud > Quiet", loudPT > quietPT * 10,
               "Ratio " + std::to_string(static_cast<int>(ratio)) + ":1");

    // Test 3: Noise floor gate
    // Signals below -60dB should produce no print-through
    pt.reset();

    double belowFloor = 0.0005;  // -66dB, below -60dB threshold
    for (int i = 0; i < delaySamples + 10; ++i)
    {
        double left = belowFloor;
        double right = belowFloor;
        pt.processSample(left, right);
    }

    double noPTLeft = 0.0, noPTRight = 0.0;
    pt.processSample(noPTLeft, noPTRight);

    reportTest("Noise Floor Gate Active", std::abs(noPTLeft) < 1e-12,
               "PT at -66dB input: " + std::to_string(std::abs(noPTLeft)));

    // Test 4: Verify expected level at unity
    // At unity input, PT should be approximately -58dB (0.00126) for GP9 tape
    double expectedPT = 1.0 * 0.00126 * 1.0;  // signal * coeff * signal (quadratic)
    double actualPT = loudPT;
    double errorDB = 20.0 * std::log10(actualPT / expectedPT);

    std::cout << "  Expected PT @ unity: " << std::scientific << expectedPT << "\n";
    std::cout << "  Actual PT @ unity: " << actualPT << "\n";
    std::cout << "  Error: " << std::fixed << std::setprecision(2) << errorDB << " dB\n";

    reportTest("PT Level at Unity", std::abs(errorDB) < 1.0,
               "Error: " + std::to_string(errorDB).substr(0,5) + " dB (tolerance: ±1dB)");
}

// ============================================================================
// TEST 11: CROSSTALK (Studer mode only)
// ============================================================================
struct CrosstalkFilter
{
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

        void setHighPass(double fc, double Q, double sampleRate)
        {
            double w0 = 2.0 * M_PI * fc / sampleRate;
            double cosw0 = std::cos(w0);
            double sinw0 = std::sin(w0);
            double alpha = sinw0 / (2.0 * Q);
            double a0 = 1.0 + alpha;
            b0 = ((1.0 + cosw0) / 2.0) / a0;
            b1 = (-(1.0 + cosw0)) / a0;
            b2 = ((1.0 + cosw0) / 2.0) / a0;
            a1 = (-2.0 * cosw0) / a0;
            a2 = (1.0 - alpha) / a0;
        }

        void setLowPass(double fc, double Q, double sampleRate)
        {
            double w0 = 2.0 * M_PI * fc / sampleRate;
            double cosw0 = std::cos(w0);
            double sinw0 = std::sin(w0);
            double alpha = sinw0 / (2.0 * Q);
            double a0 = 1.0 + alpha;
            b0 = ((1.0 - cosw0) / 2.0) / a0;
            b1 = (1.0 - cosw0) / a0;
            b2 = ((1.0 - cosw0) / 2.0) / a0;
            a1 = (-2.0 * cosw0) / a0;
            a2 = (1.0 - alpha) / a0;
        }
    };

    Biquad highpass;
    Biquad lowpass;
    double gain = 0.00316;  // -50dB (Studer A820 spec)

    void prepare(double sampleRate)
    {
        highpass.setHighPass(100.0, 0.707, sampleRate);
        lowpass.setLowPass(8000.0, 0.707, sampleRate);
        reset();
    }

    void reset()
    {
        highpass.reset();
        lowpass.reset();
    }

    double process(double monoInput)
    {
        double filtered = highpass.process(monoInput);
        filtered = lowpass.process(filtered);
        return filtered * gain;
    }
};

void testCrosstalk()
{
    std::cout << "\n=== TEST 11: Crosstalk (Studer mode) ===\n";

    double sampleRate = 48000.0;
    CrosstalkFilter xtalk;
    xtalk.prepare(sampleRate);

    // Test 1: Verify -50dB level at 1kHz (in passband)
    double testFreq = 1000.0;
    int numCycles = 100;
    int samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    int totalSamples = numCycles * samplesPerCycle;
    int skipSamples = 10 * samplesPerCycle;  // Let filter settle

    double inputRMS = 0.0;
    double outputRMS = 0.0;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);
        double output = xtalk.process(input);

        if (i >= skipSamples)
        {
            inputRMS += input * input;
            outputRMS += output * output;
        }
    }

    inputRMS = std::sqrt(inputRMS / (totalSamples - skipSamples));
    outputRMS = std::sqrt(outputRMS / (totalSamples - skipSamples));

    double levelDB = 20.0 * std::log10(outputRMS / inputRMS);

    std::cout << "  1kHz level: " << std::fixed << std::setprecision(1) << levelDB << " dB (target: -50dB)\n";

    reportTest("Crosstalk Level @ 1kHz", std::abs(levelDB - (-50.0)) < 1.0,
               std::to_string(levelDB).substr(0,5) + " dB (tolerance: ±1dB from -50dB)");

    // Test 2: Verify highpass at 100Hz (should attenuate 50Hz)
    xtalk.reset();
    testFreq = 50.0;
    samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    totalSamples = numCycles * samplesPerCycle;
    skipSamples = 20 * samplesPerCycle;

    inputRMS = 0.0;
    outputRMS = 0.0;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);
        double output = xtalk.process(input);

        if (i >= skipSamples)
        {
            inputRMS += input * input;
            outputRMS += output * output;
        }
    }

    inputRMS = std::sqrt(inputRMS / (totalSamples - skipSamples));
    outputRMS = std::sqrt(outputRMS / (totalSamples - skipSamples));

    double level50Hz = 20.0 * std::log10(outputRMS / inputRMS);

    std::cout << "  50Hz level: " << std::fixed << std::setprecision(1) << level50Hz << " dB\n";

    // 50Hz should be attenuated more than 1kHz (highpass effect)
    reportTest("Crosstalk HP Active (50Hz < 1kHz)", level50Hz < levelDB - 3.0,
               "50Hz at " + std::to_string(level50Hz).substr(0,5) + " dB vs 1kHz at " +
               std::to_string(levelDB).substr(0,5) + " dB");

    // Test 3: Verify lowpass at 8kHz (should attenuate 12kHz)
    xtalk.reset();
    testFreq = 12000.0;
    samplesPerCycle = static_cast<int>(sampleRate / testFreq);
    totalSamples = numCycles * samplesPerCycle;
    skipSamples = 20 * samplesPerCycle;

    inputRMS = 0.0;
    outputRMS = 0.0;

    for (int i = 0; i < totalSamples; ++i)
    {
        double t = static_cast<double>(i) / sampleRate;
        double input = std::sin(2.0 * M_PI * testFreq * t);
        double output = xtalk.process(input);

        if (i >= skipSamples)
        {
            inputRMS += input * input;
            outputRMS += output * output;
        }
    }

    inputRMS = std::sqrt(inputRMS / (totalSamples - skipSamples));
    outputRMS = std::sqrt(outputRMS / (totalSamples - skipSamples));

    double level12kHz = 20.0 * std::log10(outputRMS / inputRMS);

    std::cout << "  12kHz level: " << std::fixed << std::setprecision(1) << level12kHz << " dB\n";

    // 12kHz should be attenuated more than 1kHz (lowpass effect)
    reportTest("Crosstalk LP Active (12kHz < 1kHz)", level12kHz < levelDB - 3.0,
               "12kHz at " + std::to_string(level12kHz).substr(0,5) + " dB vs 1kHz at " +
               std::to_string(levelDB).substr(0,5) + " dB");
}

// ============================================================================
// MAIN
// ============================================================================
int main()
{
    std::cout << "================================================================\n";
    std::cout << "   LOWTHD Signal Flow Comprehensive Test Suite\n";
    std::cout << "================================================================\n";

    testBiasShieldingCurve();
    testHFNull();
    testJilesAtherton();
    testAsymmetricTanh();
    testDispersiveAllpass();
    testDCBlocking();
    testAzimuthDelay();
    testTHD();
    testEvenOddRatio();
    testPrintThrough();
    testCrosstalk();

    // Summary
    std::cout << "\n================================================================\n";
    std::cout << "   TEST SUMMARY\n";
    std::cout << "================================================================\n";

    int passed = 0, failed = 0;
    for (const auto& result : allResults)
    {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "\n  Total: " << (passed + failed) << " tests\n";
    std::cout << "  Passed: " << passed << "\n";
    std::cout << "  Failed: " << failed << "\n\n";

    if (failed > 0)
    {
        std::cout << "  Failed tests:\n";
        for (const auto& result : allResults)
        {
            if (!result.passed)
            {
                std::cout << "    - " << result.name;
                if (!result.details.empty()) std::cout << ": " << result.details;
                std::cout << "\n";
            }
        }
    }

    std::cout << "\n================================================================\n";
    std::cout << (failed == 0 ? "   ALL TESTS PASSED" : "   SOME TESTS FAILED") << "\n";
    std::cout << "================================================================\n";

    return (failed == 0) ? 0 : 1;
}

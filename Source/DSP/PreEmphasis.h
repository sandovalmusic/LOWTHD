#pragma once

#include <cmath>

namespace TapeHysteresis
{

/**
 * 30 IPS CCIR Re-Emphasis Filter
 *
 * Applied AFTER saturation to restore high frequencies that were
 * cut by de-emphasis before saturation.
 *
 * Target curve (measured from user's EQ):
 * 1.5k: +0.2 dB
 * 3k:   +0.5 dB
 * 5k:   +1.3 dB
 * 7k:   +2.0 dB
 * 10k:  +3.5 dB
 * 15k:  +5.5 dB
 * 20k:  +7.0 dB
 * 25k:  +9.0 dB
 *
 * Uses cascaded high-shelf filters to approximate the curve
 */
class ReEmphasis
{
public:
    ReEmphasis();
    ~ReEmphasis() = default;

    void setSampleRate(double sampleRate);
    void reset();

    /**
     * Process a single sample through re-emphasis
     */
    double processSample(double input);

    /**
     * Get magnitude response at a given frequency (for testing)
     * @param frequency - frequency in Hz
     * @return magnitude in dB
     */
    double getMagnitudeDB(double frequency) const;

private:
    double fs = 48000.0;

    // Biquad filter structure
    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        void reset()
        {
            z1 = z2 = 0.0;
        }

        double process(double input)
        {
            double output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }
    };

    // Combination of shelves and bell filters for precise curve matching
    Biquad shelf1;   // Primary high shelf
    Biquad shelf2;   // Secondary high shelf
    Biquad shelf3;   // Tertiary high shelf
    Biquad bell1;    // Bell cut to fine-tune mid response
    Biquad bell2;    // Bell cut/boost for high frequency sculpting

    void updateCoefficients();
    void designHighShelf(Biquad& filter, double fc, double gainDB, double Q);
    void designBell(Biquad& filter, double fc, double gainDB, double Q);
};

/**
 * De-Emphasis (inverse of pre-emphasis)
 */
class DeEmphasis
{
public:
    DeEmphasis();
    ~DeEmphasis() = default;

    void setSampleRate(double sampleRate);
    void reset();

    double processSample(double input);

private:
    double fs = 48000.0;

    struct Biquad
    {
        double b0 = 1.0, b1 = 0.0, b2 = 0.0;
        double a1 = 0.0, a2 = 0.0;
        double z1 = 0.0, z2 = 0.0;

        void reset()
        {
            z1 = z2 = 0.0;
        }

        double process(double input)
        {
            double output = b0 * input + z1;
            z1 = b1 * input - a1 * output + z2;
            z2 = b2 * input - a2 * output;
            return output;
        }
    };

    Biquad shelf1, shelf2, shelf3, bell1, bell2;

    void updateCoefficients();
    void designHighShelf(Biquad& filter, double fc, double gainDB, double Q);
    void designBell(Biquad& filter, double fc, double gainDB, double Q);
};

} // namespace TapeHysteresis

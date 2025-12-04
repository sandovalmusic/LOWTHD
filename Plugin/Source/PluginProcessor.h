#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/HybridTapeProcessor.h"

//==============================================================================
/**
 * Low THD Tape Simulator Plugin
 *
 * Wraps the HybridTapeProcessor for use as a VST3/AU plugin.
 *
 * Features:
 * - Machine mode selection (Ampex ATR-102 vs Studer A820)
 * - Input trim control
 * - Auto gain compensation on/off
 * - Zero latency
 * - Stereo processing (independent L/R channels)
 */
class LowTHDTapeSimulatorAudioProcessor : public juce::AudioProcessor,
                                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    LowTHDTapeSimulatorAudioProcessor();
    ~LowTHDTapeSimulatorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter IDs
    static constexpr const char* PARAM_MACHINE_MODE = "machineMode";
    static constexpr const char* PARAM_INPUT_TRIM = "inputTrim";
    static constexpr const char* PARAM_OUTPUT_TRIM = "outputTrim";

    // Access to parameter tree state
    juce::AudioProcessorValueTreeState& getValueTreeState() { return parameters; }

    // Get current output level in dB for metering
    float getCurrentLevelDB() const { return currentLevelDB.load(); }

private:
    //==============================================================================
    // Parameter creation helper
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter tree state
    juce::AudioProcessorValueTreeState parameters;

    // DSP processors (stereo - one per channel)
    TapeHysteresis::HybridTapeProcessor tapeProcessorLeft;
    TapeHysteresis::HybridTapeProcessor tapeProcessorRight;

    // Atomic parameter pointers for efficient access in process block
    std::atomic<float>* machineModeParam = nullptr;
    std::atomic<float>* inputTrimParam = nullptr;
    std::atomic<float>* outputTrimParam = nullptr;

    // Level metering
    std::atomic<float> currentLevelDB { -96.0f };

    // 2x Minimum Phase Oversampling (hardcoded, always on)
    // Uses JUCE's IIR half-band polyphase filters for minimum phase response
    using Oversampler = juce::dsp::Oversampling<float>;
    std::unique_ptr<Oversampler> oversampler;

    // Crosstalk filter for Studer mode
    // Simulates adjacent track bleed on 24-track tape machines
    // Bandpassed mono signal mixed at -40dB into both channels
    struct CrosstalkFilter
    {
        // Simple biquad for HP and LP
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

        Biquad highpass;  // ~100Hz HP
        Biquad lowpass;   // ~8kHz LP
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

    CrosstalkFilter crosstalkFilter;

    // Head bump modulator - simulates wow-induced LF gain variation
    // Real tape transport wow causes subtle amplitude modulation in the head bump region
    // as the effective tape speed varies slightly
    struct HeadBumpModulator
    {
        // Biquad bandpass to isolate head bump region
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

            // Peaking/bell filter to boost head bump region
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

        // LFO phases (3 incommensurate frequencies for organic feel)
        float phase1 = 0.0f;
        float phase2 = 0.0f;
        float phase3 = 0.0f;

        // LFO frequencies (Hz) - slow wow rates
        static constexpr float freq1 = 0.63f;   // Primary wow
        static constexpr float freq2 = 1.07f;   // Secondary variation
        static constexpr float freq3 = 0.31f;   // Slow drift

        // Phase increments (calculated in prepare)
        float phaseInc1 = 0.0f;
        float phaseInc2 = 0.0f;
        float phaseInc3 = 0.0f;

        float sampleRate = 48000.0f;
        float centerFreq = 60.0f;      // Head bump center (set per machine)
        float modulationDepth = 0.012f; // ±0.1dB = ±0.012 linear (set per machine)

        void prepare(float sr, bool isAmpex)
        {
            sampleRate = sr;

            if (isAmpex)
            {
                // Ampex ATR-102: tighter transport, less wow
                // Head bump at 40Hz, ±0.08dB modulation
                centerFreq = 40.0f;
                modulationDepth = 0.009f;  // ±0.08dB
            }
            else
            {
                // Studer A820: multitrack, slightly more wow
                // Head bump centered between 50Hz and 110Hz
                centerFreq = 75.0f;
                modulationDepth = 0.014f;  // ±0.12dB
            }

            // Wide Q to cover the bump region
            bandpassL.setBandpass(centerFreq, 0.7f, sampleRate);
            bandpassR.setBandpass(centerFreq, 0.7f, sampleRate);

            // Phase increments for block-rate update
            // We'll update once per block, so these are per-block increments
            // Actual values set in updateLFO based on block size
            reset();
        }

        void reset()
        {
            bandpassL.reset();
            bandpassR.reset();
            phase1 = 0.0f;
            phase2 = 0.3f;  // Offset for variety
            phase3 = 0.7f;
        }

        // Call once per block to update LFO (block-rate processing)
        float updateLFO(int blockSize)
        {
            // Calculate how much time this block represents
            float blockTime = static_cast<float>(blockSize) / sampleRate;

            // Update phases
            phase1 += freq1 * blockTime * 6.28318530718f;
            phase2 += freq2 * blockTime * 6.28318530718f;
            phase3 += freq3 * blockTime * 6.28318530718f;

            // Wrap phases
            if (phase1 > 6.28318530718f) phase1 -= 6.28318530718f;
            if (phase2 > 6.28318530718f) phase2 -= 6.28318530718f;
            if (phase3 > 6.28318530718f) phase3 -= 6.28318530718f;

            // Combine sines with different weights for organic feel
            float lfo = std::sin(phase1) * 0.5f +
                        std::sin(phase2) * 0.3f +
                        std::sin(phase3) * 0.2f;

            // Return modulation multiplier (1.0 ± depth)
            return 1.0f + lfo * modulationDepth;
        }

        // Process a sample - modulate the head bump region
        void processSample(float& left, float& right, float modGain)
        {
            // Extract head bump region
            float bumpL = bandpassL.process(left);
            float bumpR = bandpassR.process(right);

            // Apply modulation only to the bump region
            // modGain varies from (1-depth) to (1+depth)
            // We subtract the original bump and add the modulated version
            float modAmount = modGain - 1.0f;
            left += bumpL * modAmount;
            right += bumpR * modAmount;
        }
    };

    HeadBumpModulator headBumpModulator;

    // Auto-gain: Track the last input trim to detect changes
    float lastInputTrimValue = 0.5f;
    bool isUpdatingOutputTrim = false;  // Prevent listener recursion

    // Parameter listener callback
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowTHDTapeSimulatorAudioProcessor)
};

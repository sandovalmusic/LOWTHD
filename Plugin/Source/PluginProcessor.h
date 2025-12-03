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

    // Parameter smoothers for audio-rate interpolation
    juce::SmoothedValue<float> inputTrimSmooth;
    juce::SmoothedValue<float> outputTrimSmooth;

    // Level metering
    std::atomic<float> currentLevelDB { -96.0f };

    // 2x Minimum Phase Oversampling (hardcoded, always on)
    // Uses JUCE's IIR half-band polyphase filters for minimum phase response
    using Oversampler = juce::dsp::Oversampling<float>;
    std::unique_ptr<Oversampler> oversampler;

    // Auto-gain: Track the last input trim to detect changes
    float lastInputTrimValue = 0.5f;
    bool isUpdatingOutputTrim = false;  // Prevent listener recursion

    // Parameter listener callback
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowTHDTapeSimulatorAudioProcessor)
};

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * Low THD Tape Simulator GUI Editor
 *
 * Simple but functional interface with:
 * - Machine mode selector (Ampex/Studer)
 * - Input trim slider
 * - PPM-style level meter with color gradient
 */
class LowTHDTapeSimulatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                 public juce::Timer
{
public:
    LowTHDTapeSimulatorAudioProcessorEditor (LowTHDTapeSimulatorAudioProcessor&);
    ~LowTHDTapeSimulatorAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    // Reference to processor
    LowTHDTapeSimulatorAudioProcessor& audioProcessor;

    // UI Components
    juce::Label titleLabel;

    // Machine Mode
    juce::Label machineModeLabel;
    juce::ComboBox machineModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> machineModeAttachment;

    // Input Trim
    juce::Label inputTrimLabel;
    juce::Slider inputTrimSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputTrimAttachment;

    // Output Trim
    juce::Label outputTrimLabel;
    juce::Slider outputTrimSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;

    // Tape Bump toggle
    juce::Label tapeBumpLabel;
    juce::ToggleButton tapeBumpButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tapeBumpAttachment;

    // PPM Meter
    juce::Rectangle<float> meterBounds;
    float meterLevel = 0.0f;
    juce::Colour getMeterColour (float levelDB) const;

    // Styling
    juce::Colour backgroundColour;
    juce::Colour accentColour;
    juce::Colour textColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LowTHDTapeSimulatorAudioProcessorEditor)
};

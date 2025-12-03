#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LowTHDTapeSimulatorAudioProcessorEditor::LowTHDTapeSimulatorAudioProcessorEditor (LowTHDTapeSimulatorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Define color scheme (vintage tape aesthetic)
    backgroundColour = juce::Colour (0xff2b2b2b);  // Dark grey
    accentColour = juce::Colour (0xffcc8844);      // Warm copper/gold
    textColour = juce::Colour (0xffeaeaea);        // Light grey

    // Title Label
    titleLabel.setText ("LOW THD TAPE SIMULATOR", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, accentColour);
    addAndMakeVisible (titleLabel);

    // Machine Mode ComboBox
    machineModeLabel.setText ("Mode", juce::dontSendNotification);
    machineModeLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    machineModeLabel.setJustificationType (juce::Justification::centredLeft);
    machineModeLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (machineModeLabel);

    machineModeCombo.addItem ("Master", 1);
    machineModeCombo.addItem ("Tracks", 2);
    machineModeCombo.setSelectedId (1, juce::dontSendNotification);
    machineModeCombo.setColour (juce::ComboBox::backgroundColourId, backgroundColour.brighter (0.2f));
    machineModeCombo.setColour (juce::ComboBox::textColourId, textColour);
    machineModeCombo.setColour (juce::ComboBox::outlineColourId, accentColour);
    machineModeCombo.setColour (juce::ComboBox::arrowColourId, accentColour);
    addAndMakeVisible (machineModeCombo);

    machineModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        audioProcessor.getValueTreeState(),
        LowTHDTapeSimulatorAudioProcessor::PARAM_MACHINE_MODE,
        machineModeCombo
    );

    // Input Trim Slider (labeled as "Drive" for clarity)
    inputTrimLabel.setText ("Drive", juce::dontSendNotification);
    inputTrimLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    inputTrimLabel.setJustificationType (juce::Justification::centredLeft);
    inputTrimLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (inputTrimLabel);

    inputTrimSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    inputTrimSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    inputTrimSlider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    inputTrimSlider.setColour (juce::Slider::rotarySliderOutlineColourId, backgroundColour.brighter (0.3f));
    inputTrimSlider.setColour (juce::Slider::textBoxTextColourId, textColour);
    inputTrimSlider.setColour (juce::Slider::textBoxBackgroundColourId, backgroundColour.brighter (0.1f));
    inputTrimSlider.setColour (juce::Slider::textBoxOutlineColourId, accentColour.withAlpha (0.5f));
    addAndMakeVisible (inputTrimSlider);

    inputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getValueTreeState(),
        LowTHDTapeSimulatorAudioProcessor::PARAM_INPUT_TRIM,
        inputTrimSlider
    );

    // Output Trim Slider (labeled as "Volume" for clarity)
    outputTrimLabel.setText ("Volume", juce::dontSendNotification);
    outputTrimLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    outputTrimLabel.setJustificationType (juce::Justification::centredLeft);
    outputTrimLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (outputTrimLabel);

    outputTrimSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputTrimSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    outputTrimSlider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    outputTrimSlider.setColour (juce::Slider::rotarySliderOutlineColourId, backgroundColour.brighter (0.3f));
    outputTrimSlider.setColour (juce::Slider::textBoxTextColourId, textColour);
    outputTrimSlider.setColour (juce::Slider::textBoxBackgroundColourId, backgroundColour.brighter (0.1f));
    outputTrimSlider.setColour (juce::Slider::textBoxOutlineColourId, accentColour.withAlpha (0.5f));
    addAndMakeVisible (outputTrimSlider);

    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getValueTreeState(),
        LowTHDTapeSimulatorAudioProcessor::PARAM_OUTPUT_TRIM,
        outputTrimSlider
    );

    // Tape Bump toggle
    tapeBumpLabel.setText ("Tape Bump", juce::dontSendNotification);
    tapeBumpLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    tapeBumpLabel.setJustificationType (juce::Justification::centredLeft);
    tapeBumpLabel.setColour (juce::Label::textColourId, textColour);
    addAndMakeVisible (tapeBumpLabel);

    tapeBumpButton.setButtonText ("");
    tapeBumpButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    tapeBumpButton.setColour (juce::ToggleButton::tickDisabledColourId, backgroundColour.brighter (0.3f));
    addAndMakeVisible (tapeBumpButton);

    tapeBumpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getValueTreeState(),
        LowTHDTapeSimulatorAudioProcessor::PARAM_TAPE_BUMP,
        tapeBumpButton
    );

    // Set window size
    setSize (500, 400);

    // Start timer for meter updates (30 fps)
    startTimerHz (30);
}

LowTHDTapeSimulatorAudioProcessorEditor::~LowTHDTapeSimulatorAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
juce::Colour LowTHDTapeSimulatorAudioProcessorEditor::getMeterColour (float levelDB) const
{
    // Meter shows INPUT to tape (after trim, before saturation)
    // Map tape operating levels based on typical calibration:
    // -3 VU ≈ -21 dBFS (comfortable operating level, very clean)
    // 0 VU (tape operating level) ≈ -18 dBFS digital standard
    // +3 dB ≈ -15 dBFS (0.166% THD)
    // +6 VU ≈ -12 dBFS (0.389% THD)
    // Clipping territory ≈ -6 dBFS and above

    // Grey/uncolored for levels below -3 VU - stays dim
    if (levelDB < -21.0f)
        return backgroundColour.brighter (0.4f);  // Subtle grey - doesn't draw attention

    // Green for -3 VU to 0 VU (comfortable operating level, very clean)
    if (levelDB < -18.0f)
        return juce::Colour (0xff00cc44);  // Green - good operating level

    // Yellow for 0 VU (nominal tape operating level, ~0.075% THD) - starts to light up
    if (levelDB < -15.0f)
        return juce::Colour::fromHSV (0.166f, 0.9f, 0.9f, 1.0f);  // Yellow

    // Orange for +3 dB tape level (~0.166% THD) - getting hot
    if (levelDB < -12.0f)
        return juce::Colour (0xffff8800);  // Orange

    // Red for +6 VU and above (~0.389% THD and higher) - danger zone
    return juce::Colour (0xffff0000);  // Red
}

void LowTHDTapeSimulatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient gradient (
        backgroundColour.brighter (0.1f), 0.0f, 0.0f,
        backgroundColour.darker (0.2f), 0.0f, static_cast<float> (getHeight()),
        false
    );
    g.setGradientFill (gradient);
    g.fillAll();

    // Decorative border
    g.setColour (accentColour.withAlpha (0.3f));
    g.drawRect (getLocalBounds().reduced (2), 2);

    // Section dividers
    g.setColour (accentColour.withAlpha (0.2f));
    g.drawLine (20.0f, 70.0f, static_cast<float> (getWidth() - 20), 70.0f, 1.0f);

    // Draw PPM meter if bounds are set
    if (!meterBounds.isEmpty())
    {
        // Meter background
        g.setColour (backgroundColour.darker (0.3f));
        g.fillRoundedRectangle (meterBounds, 4.0f);

        // Meter border
        g.setColour (accentColour.withAlpha (0.4f));
        g.drawRoundedRectangle (meterBounds, 4.0f, 2.0f);

        // Meter fill (based on level) - scale from -48dB to -6dB range
        // This covers well below 0 VU (-18dBFS) up to hot digital levels
        const float minDB = -48.0f;
        const float maxDB = -6.0f;
        float normalizedLevel = juce::jmap (meterLevel, minDB, maxDB, 0.0f, 1.0f);
        normalizedLevel = juce::jlimit (0.0f, 1.0f, normalizedLevel);

        if (normalizedLevel > 0.001f)
        {
            g.setColour (getMeterColour (meterLevel));
            auto fillBounds = meterBounds.reduced (4.0f);
            float fillWidth = fillBounds.getWidth() * normalizedLevel;
            fillBounds.setWidth (fillWidth);
            g.fillRoundedRectangle (fillBounds, 2.0f);
        }

        // Draw level marker text
        g.setColour (textColour.withAlpha (0.8f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (meterLevel, 1) + " dB",
                    meterBounds.toNearestInt(),
                    juce::Justification::centred);
    }
}

void LowTHDTapeSimulatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    const int margin = 20;
    const int controlHeight = 25;
    const int knobSize = 100;

    // Title at top
    titleLabel.setBounds (area.removeFromTop (60).reduced (margin, 10));

    // Main area
    area.removeFromTop (20);  // Spacing after divider
    auto controlArea = area.reduced (margin, 0);

    // Machine mode selector
    auto machineModeArea = controlArea.removeFromTop (controlHeight + 10);
    machineModeLabel.setBounds (machineModeArea.removeFromLeft (80));
    machineModeCombo.setBounds (machineModeArea.removeFromLeft (120));

    // Tape Bump toggle (same row, right side)
    machineModeArea.removeFromLeft (40);  // Spacing
    tapeBumpLabel.setBounds (machineModeArea.removeFromLeft (90));
    tapeBumpButton.setBounds (machineModeArea.removeFromLeft (30).reduced (0, 2));

    controlArea.removeFromTop (15);  // Spacing

    // Knobs area - side by side
    auto knobsRow = controlArea.removeFromTop (knobSize + 30);

    // Input Trim knob (left side)
    auto inputKnobArea = knobsRow.removeFromLeft (knobsRow.getWidth() / 2);
    inputTrimLabel.setBounds (inputKnobArea.removeFromTop (20).withSizeKeepingCentre (100, 20));
    inputTrimSlider.setBounds (inputKnobArea.withSizeKeepingCentre (knobSize, knobSize));

    // Output Trim knob (right side)
    auto outputKnobArea = knobsRow;
    outputTrimLabel.setBounds (outputKnobArea.removeFromTop (20).withSizeKeepingCentre (100, 20));
    outputTrimSlider.setBounds (outputKnobArea.withSizeKeepingCentre (knobSize, knobSize));

    controlArea.removeFromTop (15);  // Spacing

    // PPM Meter (horizontal bar)
    auto meterArea = controlArea.removeFromTop (40);
    meterBounds = meterArea.reduced (10, 5).toFloat();
}

void LowTHDTapeSimulatorAudioProcessorEditor::timerCallback()
{
    // Get actual level from processor
    float currentLevel = audioProcessor.getCurrentLevelDB();

    // PPM-style ballistics: 10ms integration time (attack), 2s return time (release)
    // At 30 fps (33.3ms per frame):
    // Attack: reach 99% in ~10ms → coefficient ≈ 1.0 (instant attack)
    // Release: reach 50% in ~2s → 60 frames → coefficient ≈ 0.988
    if (currentLevel > meterLevel)
        meterLevel = currentLevel;  // Instant attack (10ms integration)
    else
        meterLevel = meterLevel * 0.988f + currentLevel * (1.0f - 0.988f);  // 2s return time

    repaint();
}

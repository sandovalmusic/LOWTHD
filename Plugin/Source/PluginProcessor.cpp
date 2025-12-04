#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LowTHDTapeSimulatorAudioProcessor::LowTHDTapeSimulatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#else
    :
#endif
      parameters (*this, nullptr, juce::Identifier ("LowTHDTapeSimulator"),
                  createParameterLayout())
{
    // Get atomic parameter pointers for efficient access
    machineModeParam = parameters.getRawParameterValue (PARAM_MACHINE_MODE);
    inputTrimParam = parameters.getRawParameterValue (PARAM_INPUT_TRIM);
    outputTrimParam = parameters.getRawParameterValue (PARAM_OUTPUT_TRIM);

    // Register parameter listener for auto-gain linking
    parameters.addParameterListener (PARAM_INPUT_TRIM, this);
    lastInputTrimValue = 0.5f;  // Match default
}

LowTHDTapeSimulatorAudioProcessor::~LowTHDTapeSimulatorAudioProcessor()
{
    parameters.removeParameterListener (PARAM_INPUT_TRIM, this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
LowTHDTapeSimulatorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Machine Mode (0 = Master, 1 = Tracks)
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        PARAM_MACHINE_MODE,
        "Machine Mode",
        juce::StringArray { "Master", "Tracks" },
        0  // Default: Master
    ));

    // Input Trim (-12dB to +18dB, default -6dB = 0.5x)
    // Range: 0.25x (quiet) to 8.0x (really hot)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        PARAM_INPUT_TRIM,
        "Input Trim",
        juce::NormalisableRange<float> (0.25f, 8.0f, 0.01f, 0.4f),  // Skew for finer control in lower range
        0.5f,  // Default -6dB - clean starting point with headroom
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            float db = 20.0f * std::log10(value);
            return juce::String(db, 1) + " dB";
        }
    ));

    // Output Trim (-20dB to +9.5dB, default 0dB = 1.0x)
    // Range: 0.1x (quiet) to 3.0x (louder)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        PARAM_OUTPUT_TRIM,
        "Output Trim",
        juce::NormalisableRange<float> (0.1f, 3.0f, 0.01f, 0.5f),  // Skew for finer control near 1.0
        1.0f,  // Default 0dB - unity gain
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) {
            float db = 20.0f * std::log10(value);
            return juce::String(db, 1) + " dB";
        }
    ));

    return layout;
}

//==============================================================================
const juce::String LowTHDTapeSimulatorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LowTHDTapeSimulatorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LowTHDTapeSimulatorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LowTHDTapeSimulatorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LowTHDTapeSimulatorAudioProcessor::getTailLengthSeconds() const
{
    return 0.05;  // 50ms tail for DC blocker and filter decay
}

int LowTHDTapeSimulatorAudioProcessor::getNumPrograms()
{
    return 1;
}

int LowTHDTapeSimulatorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LowTHDTapeSimulatorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String LowTHDTapeSimulatorAudioProcessor::getProgramName (int index)
{
    return {};
}

void LowTHDTapeSimulatorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void LowTHDTapeSimulatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize 2x minimum phase oversampling
    // filterHalfBandPolyphaseIIR = minimum phase IIR filters (no linear phase latency)
    constexpr int oversamplingOrder = 1;  // 2^1 = 2x oversampling
    oversampler = std::make_unique<Oversampler> (
        2,  // numChannels (stereo)
        oversamplingOrder,
        Oversampler::filterHalfBandPolyphaseIIR,  // Minimum phase IIR
        false  // Not using maximum quality (faster)
    );
    oversampler->initProcessing (static_cast<size_t> (samplesPerBlock));

    // Report latency to DAW (oversampler adds some latency)
    setLatencySamples (static_cast<int> (oversampler->getLatencyInSamples()));

    // Initialize tape processors at OVERSAMPLED sample rate (2x)
    const double oversampledRate = sampleRate * 2.0;
    tapeProcessorLeft.setSampleRate (oversampledRate);
    tapeProcessorRight.setSampleRate (oversampledRate);

    tapeProcessorLeft.reset();
    tapeProcessorRight.reset();

    // Set default Ampex ATR-102 parameters (Master mode)
    const double defaultBias = 0.65;
    tapeProcessorLeft.setParameters (defaultBias, 1.0);
    tapeProcessorRight.setParameters (defaultBias, 1.0);

    // Initialize crosstalk filter at base sample rate (applied after downsampling)
    crosstalkFilter.prepare (static_cast<float> (sampleRate));

    // Initialize head bump modulator (default to Ampex, will update in processBlock)
    headBumpModulator.prepare (static_cast<float> (sampleRate), true);

    // Initialize tolerance EQ (randomized per instance)
    // Stereo mode = different tolerances per channel, Mono = same for both
    // Default to Ampex mode, will update in processBlock if needed
    bool isStereo = (getTotalNumInputChannels() >= 2);
    toleranceEQ.prepare (static_cast<float> (sampleRate), isStereo, true);

    // Initialize print-through (Studer mode only, but prepare always)
    printThrough.prepare (static_cast<float> (sampleRate));
}

void LowTHDTapeSimulatorAudioProcessor::releaseResources()
{
    // Reset processors when playback stops
    tapeProcessorLeft.reset();
    tapeProcessorRight.reset();
    oversampler->reset();
    crosstalkFilter.reset();
    headBumpModulator.reset();
    toleranceEQ.reset();
    printThrough.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LowTHDTapeSimulatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Support mono and stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output layouts must match
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void LowTHDTapeSimulatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have input
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Get parameter values
    const int machineMode = static_cast<int> (*machineModeParam);
    const float inputTrimValue = *inputTrimParam;
    const float outputTrimValue = *outputTrimParam;

    // Update processor parameters based on machine mode
    // Master mode (0) = Ampex ATR-102: bias=0.65, ultra-clean, E/O ~0.5
    // Tracks mode (1) = Studer A820: bias=0.82, warmer saturation, E/O ~1.0
    // The bias value determines which internal parameters are used (threshold at 0.74)
    const double bias = (machineMode == 0) ? 0.65 : 0.82;

    // Set processor parameters (input gain = 1.0, we apply drive externally via inputTrim)
    tapeProcessorLeft.setParameters (bias, 1.0);
    tapeProcessorRight.setParameters (bias, 1.0);

    const int numSamples = buffer.getNumSamples();
    float peakLevel = 0.0f;

    // Apply input trim (Drive) BEFORE oversampling and measure level for metering
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] *= inputTrimValue;
            peakLevel = std::max (peakLevel, std::abs (channelData[sample]));
        }
    }

    // === OVERSAMPLING: Upsample to 2x rate ===
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<float> oversampledBlock = oversampler->processSamplesUp (block);

    // Process at oversampled rate (2x sample rate)
    const int oversampledNumSamples = static_cast<int> (oversampledBlock.getNumSamples());

    for (int sample = 0; sample < oversampledNumSamples; ++sample)
    {
        // Process left channel
        float leftSample = oversampledBlock.getSample (0, sample);
        float leftProcessed = static_cast<float> (tapeProcessorLeft.processSample (leftSample));
        oversampledBlock.setSample (0, sample, leftProcessed);

        // Process right channel (with azimuth delay)
        if (oversampledBlock.getNumChannels() > 1)
        {
            float rightSample = oversampledBlock.getSample (1, sample);
            float rightProcessed = static_cast<float> (tapeProcessorRight.processRightChannel (rightSample));
            oversampledBlock.setSample (1, sample, rightProcessed);
        }
    }

    // === OVERSAMPLING: Downsample back to original rate ===
    oversampler->processSamplesDown (block);

    // === CROSSTALK: Studer mode only ===
    // Simulates adjacent track bleed on 24-track tape machines
    // Adds bandpassed mono signal at -55dB to both channels
    if (machineMode == 1 && totalNumInputChannels >= 2)  // Studer mode, stereo only
    {
        float* leftData = buffer.getWritePointer (0);
        float* rightData = buffer.getWritePointer (1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Sum to mono
            float mono = (leftData[sample] + rightData[sample]) * 0.5f;
            // Bandpass and attenuate
            float crosstalk = crosstalkFilter.process (mono);
            // Add to both channels
            leftData[sample] += crosstalk;
            rightData[sample] += crosstalk;
        }
    }

    // === HEAD BUMP MODULATION: Both modes ===
    // Simulates wow-induced amplitude variation in the head bump frequency region
    // Updates LFO once per block (efficient), applies sample-by-sample
    {
        // Update modulator and tolerance EQ settings if machine mode changed
        static int lastMachineMode = -1;
        if (machineMode != lastMachineMode)
        {
            bool isAmpex = (machineMode == 0);
            headBumpModulator.prepare (static_cast<float> (getSampleRate()), isAmpex);
            toleranceEQ.prepare (static_cast<float> (getSampleRate()),
                                 totalNumInputChannels >= 2, isAmpex);
            lastMachineMode = machineMode;
        }

        // Get LFO value for this block (block-rate update)
        float modGain = headBumpModulator.updateLFO (numSamples);

        // Apply modulation to head bump region
        if (totalNumInputChannels >= 2)
        {
            float* leftData = buffer.getWritePointer (0);
            float* rightData = buffer.getWritePointer (1);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                headBumpModulator.processSample (leftData[sample], rightData[sample], modGain);
            }
        }
        else if (totalNumInputChannels == 1)
        {
            float* monoData = buffer.getWritePointer (0);
            float dummy = 0.0f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                headBumpModulator.processSample (monoData[sample], dummy, modGain);
            }
        }
    }

    // === TOLERANCE EQ: Both modes, machine-specific ===
    // Models subtle channel-to-channel frequency response variations
    // due to tape head manufacturing tolerances on freshly calibrated machines
    // Ampex ATR-102: ±0.10dB low (60Hz), ±0.12dB high (16kHz) - precision mastering
    // Studer A820:   ±0.15dB low (75Hz), ±0.18dB high (15kHz) - multitrack variation
    // Stereo instances get different L/R tolerances; mono instances use same for both
    {
        if (totalNumInputChannels >= 2)
        {
            float* leftData = buffer.getWritePointer (0);
            float* rightData = buffer.getWritePointer (1);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                toleranceEQ.processSample (leftData[sample], rightData[sample]);
            }
        }
        else if (totalNumInputChannels == 1)
        {
            float* monoData = buffer.getWritePointer (0);
            float dummy = 0.0f;

            for (int sample = 0; sample < numSamples; ++sample)
            {
                toleranceEQ.processSample (monoData[sample], dummy);
            }
        }
    }

    // === PRINT-THROUGH: Studer mode only ===
    // Simulates magnetic bleed between tape layers creating subtle pre-echo
    // Signal-dependent: louder passages create proportionally more print-through
    // Real-world multitrack tape (more layers, more print-through than 2-track)
    // 65ms delay represents tape layer spacing at 30 IPS
    if (machineMode == 1 && totalNumInputChannels >= 2)  // Studer mode, stereo only
    {
        float* leftData = buffer.getWritePointer (0);
        float* rightData = buffer.getWritePointer (1);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            printThrough.processSample (leftData[sample], rightData[sample]);
        }
    }
    else if (machineMode == 1 && totalNumInputChannels == 1)  // Studer mode, mono
    {
        float* monoData = buffer.getWritePointer (0);
        float dummy = 0.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            printThrough.processSample (monoData[sample], dummy);
        }
    }

    // Apply output trim (Volume) and final makeup gain
    // Auto-gain is handled by parameter linking: when Drive changes,
    // Output Trim is automatically adjusted to compensate
    //
    // Final +6dB makeup compensates for default Input Trim of 0.5 (-6dB)
    // This ensures unity gain with default settings
    constexpr float finalMakeupGain = 2.0f;  // +6dB

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* channelData = buffer.getWritePointer (ch);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] *= outputTrimValue * finalMakeupGain;
    }

    // Update meter level (convert to dB)
    if (peakLevel > 0.0001f)
        currentLevelDB.store (20.0f * std::log10 (peakLevel));
    else
        currentLevelDB.store (-96.0f);
}

//==============================================================================
bool LowTHDTapeSimulatorAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* LowTHDTapeSimulatorAudioProcessor::createEditor()
{
    return new LowTHDTapeSimulatorAudioProcessorEditor (*this);
}

//==============================================================================
void LowTHDTapeSimulatorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save parameter state
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void LowTHDTapeSimulatorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameter state
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// Parameter listener callback for auto-gain linking
void LowTHDTapeSimulatorAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == PARAM_INPUT_TRIM && !isUpdatingOutputTrim)
    {
        // Auto-gain: When Drive (input trim) changes, adjust Output Trim to compensate
        // This keeps monitoring level constant while allowing saturation to increase
        //
        // Logic: If Drive goes from 0.5 to 2.0 (4x increase = +12dB),
        //        Output should go from current to current/4 (-12dB compensation)
        //
        // We calculate the RATIO of change and apply it inversely to output trim

        float ratio = lastInputTrimValue / newValue;  // Inverse ratio
        float currentOutputTrim = *outputTrimParam;
        float newOutputTrim = currentOutputTrim * ratio;

        // Clamp to valid output trim range (0.1 to 3.0)
        newOutputTrim = std::clamp (newOutputTrim, 0.1f, 3.0f);

        // Update output trim parameter (with recursion guard)
        isUpdatingOutputTrim = true;
        if (auto* param = parameters.getParameter (PARAM_OUTPUT_TRIM))
            param->setValueNotifyingHost (param->convertTo0to1 (newOutputTrim));
        isUpdatingOutputTrim = false;

        // Remember current input trim for next delta calculation
        lastInputTrimValue = newValue;
    }
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LowTHDTapeSimulatorAudioProcessor();
}

cmake_minimum_required(VERSION 3.22)

project(TapeHysteresis VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE (8.0.4 for macOS 15 compatibility)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Determine plugin formats based on platform
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

# Low THD Tape Sim (Ampex ATR-102 + Studer A820 modes)
# NOTE: Target name "LowTHDTape" and codes must match original release
# for DAWs (like Reaper) to recognize as the same plugin
juce_add_plugin(LowTHDTape
    COMPANY_NAME "TapeHysteresis"
    PLUGIN_MANUFACTURER_CODE TpHs
    PLUGIN_CODE LwTH
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Low THD Tape Sim"
    DESCRIPTION "Ampex ATR-102 + Studer A820 - Precision tape emulation"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{HOME}/Desktop"
    AU_COPY_DIR "$ENV{HOME}/Desktop"
    VST3_CATEGORIES "Fx" "Distortion"
    AU_MAIN_TYPE kAudioUnitType_Effect
)

# Source files
target_sources(LowTHDTape PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ../Source/DSP/HybridTapeProcessor.cpp
    ../Source/DSP/PreEmphasis.cpp
    ../Source/DSP/MachineEQ.cpp
)

# Include directories
target_include_directories(LowTHDTape PRIVATE
    Source
    ../Source
)

# Link JUCE modules
target_link_libraries(LowTHDTape PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Compiler definitions
target_compile_definitions(LowTHDTape PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

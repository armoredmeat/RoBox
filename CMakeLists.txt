cmake_minimum_required(VERSION 3.22)

project(RoBox VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build Microsoft's C++ runtime into RoBox itself (static), so it runs on any Windows PC
# without the "Visual C++ Redistributable" being installed. Must come before JUCE is added.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Download JUCE at configure time (pinned to a release tag).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(RoBox
    COMPANY_NAME              "RoBox Audio"
    PRODUCT_NAME              "RoBox"
    PLUGIN_MANUFACTURER_CODE  Rbxa
    PLUGIN_CODE               Rbx1
    FORMATS                   VST3 Standalone
    IS_SYNTH                  FALSE   # it's an effect on a vocal track...
    NEEDS_MIDI_INPUT          TRUE    # ...that can optionally take MIDI for the carrier pitch
    NEEDS_MIDI_OUTPUT         FALSE
    VST3_CATEGORIES           Fx Modulation
    COPY_PLUGIN_AFTER_BUILD   FALSE   # copying into Program Files needs admin; we copy manually
)

# Factory presets, built into the plugin as binary data (BinaryData.h).
file(GLOB ROBOX_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Presets/*.xml")
juce_add_binary_data(RoBoxPresetData SOURCES ${ROBOX_FACTORY_PRESETS})

target_sources(RoBox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/CarrierOsc.cpp
    Source/dsp/VocoderEngine.cpp
    Source/dsp/Character.cpp
    Source/dsp/TalkboxEngine.cpp
    Source/dsp/Equalizer.cpp
    Source/ui/RoBoxLookAndFeel.cpp
    Source/ui/Theme.cpp
    Source/ui/KeyboardDisplay.cpp
    Source/ui/LiveDisplay.cpp
    Source/ui/EqSection.cpp
    Source/ui/PresetBar.cpp
    Source/presets/PresetManager.cpp
)

target_include_directories(RoBox PRIVATE Source)

target_compile_definitions(RoBox PUBLIC
    ROBOX_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0 # RoBox only uses MIDI notes; without this, VST3 adds 2080 hidden CC parameters
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(RoBox
    PRIVATE
        RoBoxPresetData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- DSP unit tests (console app) ----
juce_add_console_app(RoBoxTests PRODUCT_NAME "RoBoxTests")

target_sources(RoBoxTests PRIVATE
    Tests/TestMain.cpp
    Tests/CarrierTests.cpp
    Tests/VocoderTests.cpp
    Tests/CharacterTests.cpp
    Tests/NoteStackTests.cpp
    Tests/TalkboxTests.cpp
    Tests/TubeTests.cpp
    Tests/EqualizerTests.cpp
    Tests/PresetTests.cpp
    Tests/ThemeTests.cpp
    Tests/PluginTests.cpp
    Tests/UiTests.cpp
    # The whole plugin, so tests can run presets through the real processor.
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/CarrierOsc.cpp
    Source/dsp/VocoderEngine.cpp
    Source/dsp/Character.cpp
    Source/dsp/TalkboxEngine.cpp
    Source/dsp/Equalizer.cpp
    Source/ui/RoBoxLookAndFeel.cpp
    Source/ui/Theme.cpp
    Source/ui/KeyboardDisplay.cpp
    Source/ui/LiveDisplay.cpp
    Source/ui/EqSection.cpp
    Source/ui/PresetBar.cpp
    Source/presets/PresetManager.cpp
)

target_include_directories(RoBoxTests PRIVATE Source)

target_compile_definitions(RoBoxTests PRIVATE
    ROBOX_VERSION="${PROJECT_VERSION}"
    JucePlugin_Name="RoBox"
    JUCE_UNIT_TESTS=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(RoBoxTests
    PRIVATE
        RoBoxPresetData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_test(NAME RoBoxTests COMMAND RoBoxTests)

# ---- UI snapshot tool: renders the editor to PNGs (for checking the layout without a DAW) ----
juce_add_console_app(RoBoxSnapshot PRODUCT_NAME "RoBoxSnapshot")

target_sources(RoBoxSnapshot PRIVATE
    Tools/Snapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/CarrierOsc.cpp
    Source/dsp/VocoderEngine.cpp
    Source/dsp/Character.cpp
    Source/dsp/TalkboxEngine.cpp
    Source/dsp/Equalizer.cpp
    Source/ui/RoBoxLookAndFeel.cpp
    Source/ui/Theme.cpp
    Source/ui/KeyboardDisplay.cpp
    Source/ui/LiveDisplay.cpp
    Source/ui/EqSection.cpp
    Source/ui/PresetBar.cpp
    Source/presets/PresetManager.cpp
)

target_include_directories(RoBoxSnapshot PRIVATE Source)

target_compile_definitions(RoBoxSnapshot PRIVATE
    ROBOX_VERSION="${PROJECT_VERSION}"
    JucePlugin_Name="RoBox"
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(RoBoxSnapshot
    PRIVATE
        RoBoxPresetData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ---- Host test: loads the built RoBox.vst3 like a DAW and runs validator-style checks ----
juce_add_console_app(RoBoxHostTest PRODUCT_NAME "RoBoxHostTest")

target_sources(RoBoxHostTest PRIVATE Tools/HostTest.cpp)

target_compile_definitions(RoBoxHostTest PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

target_link_libraries(RoBoxHostTest
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

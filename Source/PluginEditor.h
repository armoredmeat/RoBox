#pragma once

#include "PluginProcessor.h"
#include "ui/RoBoxLookAndFeel.h"
#include "ui/Knob.h"
#include "ui/SegmentedControl.h"
#include "ui/KeyboardDisplay.h"
#include "ui/LiveDisplay.h"
#include "ui/EqSection.h"
#include "ui/Help.h"
#include "ui/PresetBar.h"
#include "ui/ThemeButton.h"

// Everything you see, laid out at a fixed design size (designWidth x designHeight).
// The editor scales this panel as a whole, so the layout never reflows when resized.
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel (RoBoxAudioProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    // Shows the help text of a control (or of its parent) in the status bar; nullptr clears it.
    void showHelpFor (const juce::Component*);

    static constexpr int designWidth = 860, designHeight = 536;

private:
    // The panel listens to mouse movement over all its controls to fill the status bar.
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void setHelpTexts();

    void timerCallback() override;
    void recolour(); // the theme changed: refresh every control's colours   // ~30 Hz: live display, meter, readout, dimming
    void updateModeDependentControls();

    RoBoxAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // Top bar
    SegmentedControl modeSelector;
    PresetBar presetBar;
    ThemeButton themeButton;
    juce::Component titleArea; // invisible: gives the "RoBox" title its status-bar text
    int seenThemeVersion = Theme::version();

    // Carrier
    SegmentedControl waveformSelector;
    juce::TextButton midiButton { "MIDI" }, soloButton { "Solo" };
    KeyboardDisplay keyboard;
    SegmentedControl octaveSelector;
    juce::ComboBox keyBox, chordBox;
    Knob detune, pulseWidth, noise;

    // Centre
    LiveDisplay liveDisplay;

    // Voice
    Knob bands, formant, attack, release, sibilance, tube;

    // EFX + Output
    Knob drive, bitCrush, sampleRate, mix, gain;
    OutputMeter meter;
    EqSection eq;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiAttachment, soloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment, chordAttachment;

    juce::String readoutNote, readoutChord, readoutHz;
    bool midiNoteHeld = false;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class RoBoxAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit RoBoxAudioProcessorEditor (RoBoxAudioProcessor&);
    ~RoBoxAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RoBoxLookAndFeel lookAndFeel; // declared first so it outlives every control that uses it
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoBoxAudioProcessorEditor)
};

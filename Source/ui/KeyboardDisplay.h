#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// One-octave keyboard showing the carrier's root note. Click a key to set the Key
// (the Note parameter); it stays in sync with the Key dropdown and with automation.
// While MIDI Follow is driving the pitch, it shows the note being played instead.
class KeyboardDisplay final : public juce::Component
{
public:
    explicit KeyboardDisplay (juce::RangedAudioParameter& noteParam);

    // Called by the editor's timer: the note to highlight (0 = C .. 11 = B).
    void setShownKey (int pitchClass);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr int labelHeight = 13;

private:
    int keyAt (juce::Point<float>) const;
    juce::Rectangle<float> keyBounds (int pitchClass) const;
    static bool isBlack (int pitchClass) noexcept;

    std::unique_ptr<juce::ParameterAttachment> attachment;
    int shownKey = 0;
    int hoverKey = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KeyboardDisplay)
};

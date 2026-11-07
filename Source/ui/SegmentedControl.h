#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// A row of buttons where exactly one is selected, connected to a choice or integer parameter
// (Mode, Waveform, Octave). Clicking a button sets the parameter as one undoable,
// automatable change; automation moving the parameter updates the buttons.
class SegmentedControl final : public juce::Component
{
public:
    // accent / accentInk must be live Theme colours (e.g. Theme::carrier), so they follow theme changes.
    SegmentedControl (juce::RangedAudioParameter& param, const juce::StringArray& labels,
                      const juce::Colour& accentColour, const juce::Colour& accentInkColour, bool joined, int gapBetween = 3)
        : accent (accentColour), accentInk (accentInkColour), connected (joined), gap (gapBetween)
    {
        firstValue = param.getNormalisableRange().start;

        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            if (connected)
                b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                      | (i < labels.size() - 1 ? juce::Button::ConnectedOnRight : 0));
            b->onClick = [this, i] { attachment->setValueAsCompleteGesture (firstValue + (float) i); };
            addAndMakeVisible (b);
        }

        attachment = std::make_unique<juce::ParameterAttachment> (param,
            [this] (float value) { select ((int) std::lround (value - firstValue)); }, nullptr);
        attachment->sendInitialUpdate();
        lookAndFeelChanged();
    }

    void lookAndFeelChanged() override // also called when the theme changes
    {
        for (auto* b : buttons)
        {
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->setColour (juce::TextButton::textColourOnId, accentInk);
        }
    }

    int getSelectedIndex() const noexcept { return selected; }

    void resized() override
    {
        const int n = buttons.size();
        if (n == 0) return;
        const int g = connected ? 0 : gap;
        const float w = (float) (getWidth() - g * (n - 1)) / (float) n;
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds (juce::roundToInt ((w + (float) g) * (float) i), 0,
                                   // Joined buttons overlap by 1 px so borders don't double up; the last one mustn't poke outside.
                                   juce::roundToInt (w + (connected && i < n - 1 ? 1.0f : 0.0f)), getHeight());
    }

private:
    void select (int index)
    {
        selected = index;
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    }

    const juce::Colour& accent;
    const juce::Colour& accentInk;
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float firstValue = 0.0f;
    int selected = 0;
    bool connected;
    int gap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};

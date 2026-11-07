#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// The slider inside every knob. Holding Shift while dragging or scrolling moves it 10x more finely.
// Dragging is relative (each mouse movement nudges the value), so pressing or letting go of
// Shift in the middle of a drag never makes the knob jump.
class FineSlider final : public juce::Slider
{
public:
    static constexpr double pixelsForFullRange = 250.0; // normal: 250 px of mouse movement = whole range
    static constexpr double fineFactor = 10.0;          // with Shift: 2500 px

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastPosition = e.position;
        proportion = valueToProportionOfLength (getValue());
        juce::Slider::mouseDown (e); // starts the undoable / automation "gesture"
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        const auto moved = e.position - lastPosition;
        lastPosition = e.position;
        const double pixels = moved.x - moved.y; // right or up turns it up
        const double scale = pixelsForFullRange * (e.mods.isShiftDown() ? fineFactor : 1.0);
        // Kept unrounded, so tiny fine moves add up even on stepped knobs like Bands.
        proportion = juce::jlimit (0.0, 1.0, proportion + pixels / scale);
        setValue (proportionOfLengthToValue (proportion), juce::sendNotificationSync);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        auto details = wheel;
        if (e.mods.isShiftDown())
        {
            details.deltaX /= (float) fineFactor;
            details.deltaY /= (float) fineFactor;
        }
        juce::Slider::mouseWheelMove (e, details);
    }

private:
    juce::Point<float> lastPosition;
    double proportion = 0.0;
};

// A rotary knob with its value underneath and its name below that, connected to one parameter.
// Drag up/down (or left/right) to turn it, hold Shift for fine control; double-click to reset it.
class Knob final : public juce::Component
{
public:
    using Formatter = std::function<juce::String (double)>;

    enum class Arc { fromStart, fromCentre, fromEnd };

    // accent must be one of the live Theme colours (e.g. Theme::carrier), so it follows theme changes.
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name,
          const juce::Colour& accentColour, Formatter formatter, Arc arc = Arc::fromStart)
        : accent (accentColour), format (std::move (formatter))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.getProperties().set ("bipolar", arc == Arc::fromCentre);
        slider.getProperties().set ("fromEnd", arc == Arc::fromEnd);
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);

        if (auto* p = apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

        for (auto* l : { &valueLabel, &nameLabel })
        {
            l->setJustificationType (juce::Justification::centred);
            l->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*l);
        }
        valueLabel.setFont (Theme::font (12.0f));
        nameLabel.setFont (Theme::font (11.0f));
        nameLabel.setText (name, juce::dontSendNotification);

        slider.onValueChange = [this] { updateValueText(); };
        updateValueText();
        applyColours();
    }

    void lookAndFeelChanged() override { applyColours(); } // theme changed

    // Dimmed = this control doesn't affect the sound right now (e.g. Tube in Vocoder mode).
    // It stays adjustable, so you can set it up before switching.
    void setDimmed (bool shouldDim)
    {
        if (shouldDim == isDimmed)
            return;
        isDimmed = shouldDim;
        applyColours();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int size = juce::jmin (knobSize, r.getWidth());
        slider.setBounds (r.removeFromTop (size).withSizeKeepingCentre (size, size));
        valueLabel.setBounds (r.removeFromTop (15));
        nameLabel.setBounds (r.removeFromTop (14));
    }

    static constexpr int knobSize = 46;
    static constexpr int preferredHeight = knobSize + 15 + 14;

private:
    void applyColours()
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.getProperties().set ("dimmed", isDimmed);
        valueLabel.setColour (juce::Label::textColourId, isDimmed ? Theme::dimmed : Theme::text);
        nameLabel.setColour (juce::Label::textColourId, isDimmed ? Theme::dimmed : Theme::subtext);
        slider.repaint();
    }

    void updateValueText() { valueLabel.setText (format (slider.getValue()), juce::dontSendNotification); }

    const juce::Colour& accent;
    FineSlider slider;
    juce::Label valueLabel, nameLabel;
    Formatter format;
    bool isDimmed = false;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

// Value text for the knobs.
namespace Format
{
    inline juce::String percent (double v) { return juce::String (juce::roundToInt (v)) + "%"; }
    inline juce::String ms (double v)      { return (v < 10.0 ? juce::String (v, 1) : juce::String (juce::roundToInt (v))) + " ms"; }
    inline juce::String signedValue (double v, const char* unit)
    {
        if (std::abs (v) < 0.05) return juce::String ("0 ") + unit;
        return (v > 0 ? "+" : "") + juce::String (v, 1) + " " + unit;
    }
    inline juce::String semitones (double v) { return signedValue (v, "st"); }
    inline juce::String decibels (double v)  { return signedValue (v, "dB"); }
    inline juce::String bits (double v)      { return v >= 15.99 ? juce::String ("16 bit") : juce::String (v, 1) + " bit"; }
    inline juce::String rate (double v)      { return v >= 47999.0 ? juce::String ("Off") : juce::String (v / 1000.0, 1) + " kHz"; }
}

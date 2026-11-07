#pragma once

#include "Theme.h"

// Draws JUCE's standard controls in RoBox's flat style: arc knobs, outlined buttons
// that fill with the accent colour when selected, and dark menus.
class RoBoxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RoBoxLookAndFeel();

    // Copies the current Theme colours into JUCE's colour slots (menus, combo boxes, dialogs...).
    // Called at startup and whenever the theme changes.
    void applyTheme();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};

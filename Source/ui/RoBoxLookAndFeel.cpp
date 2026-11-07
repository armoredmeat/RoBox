#include "RoBoxLookAndFeel.h"

RoBoxLookAndFeel::RoBoxLookAndFeel()
{
    Theme::loadSaved(); // your saved colour scheme (first window only)
    applyTheme();
}

void RoBoxLookAndFeel::applyTheme()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::background);

    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, Theme::carrier);
    setColour (juce::TextButton::textColourOffId, Theme::buttonText);
    setColour (juce::TextButton::textColourOnId, Theme::carrierInk);

    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId, Theme::line);
    setColour (juce::ComboBox::textColourId, Theme::buttonText);
    setColour (juce::ComboBox::arrowColourId, Theme::subtext);

    setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setColour (juce::PopupMenu::textColourId, Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::carrier);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::carrierInk);
    setColour (juce::PopupMenu::headerTextColourId, Theme::subtext);

    setColour (juce::Label::textColourId, Theme::text);

    // Dialogs (Save preset...)
    setColour (juce::AlertWindow::backgroundColourId, Theme::background);
    setColour (juce::AlertWindow::textColourId, Theme::text);
    setColour (juce::AlertWindow::outlineColourId, Theme::line);
    setColour (juce::TextEditor::backgroundColourId, Theme::panel);
    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::outlineColourId, Theme::line);
    setColour (juce::TextEditor::focusedOutlineColourId, Theme::carrier);
    setColour (juce::TextEditor::highlightColourId, Theme::carrier.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Theme::text);
    setColour (juce::TextEditor::highlightedTextColourId, Theme::text);
}

void RoBoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const bool dimmed  = slider.getProperties()["dimmed"];
    const bool bipolar = slider.getProperties()["bipolar"]; // e.g. Formant: arc grows from the centre
    const bool fromEnd = slider.getProperties()["fromEnd"]; // e.g. Bit Crush: "off" is at the top, so the arc grows downward

    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
    const auto c = bounds.getCentre();
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    const float fromAngle  = bipolar ? (startAngle + endAngle) * 0.5f : (fromEnd ? endAngle : startAngle);
    const juce::PathStrokeType stroke (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::line);
    g.strokePath (track, stroke);

    if (std::abs (valueAngle - fromAngle) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f,
                             juce::jmin (fromAngle, valueAngle), juce::jmax (fromAngle, valueAngle), true);
        g.setColour (dimmed ? Theme::line : slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (value, stroke);
    }

    const float inner = radius - 7.0f;
    g.setColour (Theme::panel);
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2.0f, inner * 2.0f);

    const auto tip = c.getPointOnCircumference (radius - 6.0f, valueAngle);
    g.setColour (dimmed ? Theme::dimmed : Theme::text);
    g.drawLine ({ c, tip }, 2.0f);
}

void RoBoxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isHighlighted, bool isDown)
{
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    constexpr float corner = 6.0f;

    // Segmented controls join their buttons edge to edge, so only the outer corners are rounded.
    const bool left = ! button.isConnectedOnLeft(), right = ! button.isConnectedOnRight();
    juce::Path shape;
    shape.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), corner, corner, left, right, left, right);

    if (on)
        g.setColour (button.findColour (juce::TextButton::buttonOnColourId));
    else if (isDown)
        g.setColour (Theme::hover (0.14f));
    else if (isHighlighted)
        g.setColour (Theme::hover (0.07f));
    else
        g.setColour (juce::Colours::transparentBlack);
    g.fillPath (shape);

    g.setColour (on ? button.findColour (juce::TextButton::buttonOnColourId) : Theme::line);
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}

juce::Font RoBoxLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return Theme::font (juce::jmin (12.0f, (float) buttonHeight * 0.55f));
}

void RoBoxLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? Theme::hover (0.07f) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Theme::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // Small downward triangle on the right.
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font RoBoxLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Theme::font (12.0f); }

void RoBoxLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font RoBoxLookAndFeel::getPopupMenuFont() { return Theme::font (13.0f); }

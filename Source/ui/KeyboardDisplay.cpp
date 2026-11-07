#include "KeyboardDisplay.h"

namespace
{
    constexpr int whitePitches[7] = { 0, 2, 4, 5, 7, 9, 11 };
    constexpr const char* whiteNames[7] = { "C", "D", "E", "F", "G", "A", "B" };

    // Index of the white key a black key sits just to the right of (C# after C, etc.).
    int whiteIndexBefore (int blackPitch) noexcept
    {
        switch (blackPitch) { case 1: return 0; case 3: return 1; case 6: return 3; case 8: return 4; default: return 5; }
    }
}

KeyboardDisplay::KeyboardDisplay (juce::RangedAudioParameter& noteParam)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    // The attachment keeps clicks undoable/automatable. The highlight itself comes from setShownKey(),
    // so it can follow MIDI notes too.
    attachment = std::make_unique<juce::ParameterAttachment> (noteParam, [] (float) {}, nullptr);
}

bool KeyboardDisplay::isBlack (int pc) noexcept
{
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void KeyboardDisplay::setShownKey (int pitchClass)
{
    if (pitchClass != shownKey)
    {
        shownKey = pitchClass;
        repaint();
    }
}

juce::Rectangle<float> KeyboardDisplay::keyBounds (int pc) const
{
    const float keysH = (float) (getHeight() - labelHeight);
    const float ww = (float) getWidth() / 7.0f;

    if (isBlack (pc))
    {
        const float x = (float) (whiteIndexBefore (pc) + 1) * ww - ww * 0.3f;
        return { x, 0.0f, ww * 0.6f, keysH * 0.6f };
    }

    int index = 0;
    while (whitePitches[index] != pc) ++index;
    return { (float) index * ww, 0.0f, ww, keysH };
}

int KeyboardDisplay::keyAt (juce::Point<float> p) const
{
    for (int pc : { 1, 3, 6, 8, 10 }) // black keys sit on top, so test them first
        if (keyBounds (pc).contains (p))
            return pc;
    for (int pc : whitePitches)
        if (keyBounds (pc).contains (p))
            return pc;
    return -1;
}

void KeyboardDisplay::paint (juce::Graphics& g)
{
    // White keys, with their names underneath.
    for (int i = 0; i < 7; ++i)
    {
        const int pc = whitePitches[i];
        const auto r = keyBounds (pc).reduced (0.5f, 0.0f);
        const bool root = pc == shownKey;

        g.setColour (root ? Theme::carrier : (pc == hoverKey ? Theme::whiteKey.interpolatedWith (Theme::carrier, 0.3f) : Theme::whiteKey));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (Theme::line); // keeps the keys apart on light themes
        g.drawRoundedRectangle (r, 2.0f, 0.75f);

        if (root)
        {
            g.setColour (Theme::carrierInk);
            g.fillEllipse (r.getCentreX() - 3.0f, r.getBottom() - 11.0f, 6.0f, 6.0f);
        }

        g.setColour (root ? Theme::carrier : Theme::subtext);
        g.setFont (Theme::font (10.0f));
        g.drawText (whiteNames[i], juce::Rectangle<float> (r.getX(), r.getBottom(), r.getWidth(), (float) labelHeight),
                    juce::Justification::centred);
    }

    // Black keys on top.
    for (int pc : { 1, 3, 6, 8, 10 })
    {
        const auto r = keyBounds (pc);
        const bool root = pc == shownKey;
        g.setColour (root ? Theme::carrier : (pc == hoverKey ? Theme::blackKey.interpolatedWith (Theme::carrier, 0.35f) : Theme::blackKey));
        g.fillRoundedRectangle (r, 1.5f);
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r, 1.5f, 0.75f);
        if (root)
        {
            g.setColour (Theme::carrierInk);
            g.fillEllipse (r.getCentreX() - 2.5f, r.getBottom() - 9.0f, 5.0f, 5.0f);
        }
    }
}

void KeyboardDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int key = keyAt (e.position);
    if (key != hoverKey)
    {
        hoverKey = key;
        repaint();
    }
}

void KeyboardDisplay::mouseExit (const juce::MouseEvent&)
{
    hoverKey = -1;
    repaint();
}

void KeyboardDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int key = keyAt (e.position);
    if (key >= 0)
    {
        attachment->setValueAsCompleteGesture ((float) key); // Note parameter: 0 = C .. 11 = B
        setShownKey (key);
    }
}

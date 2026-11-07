#pragma once

#include "Theme.h"
#include "Help.h"

// Small round button in the top bar (a half-filled circle) that opens the theme menu:
// the 5 colour schemes, each with a preview of its accent colours, then Dark / Light.
// Picking one recolours every open RoBox window and is remembered for next time.
class ThemeButton final : public juce::Button
{
public:
    ThemeButton() : juce::Button ("Theme")
    {
        Help::set (*this, "Theme: pick one of 5 colour schemes, each in a dark and a light version.");
        onClick = [this] { showMenu(); };
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        if (highlighted || down)
        {
            g.setColour (Theme::hover (down ? 0.14f : 0.07f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        // A circle, half filled: the universal "dark / light" symbol.
        const auto circle = juce::Rectangle<float> (12.0f, 12.0f).withCentre (r.getCentre());
        juce::Path half;
        half.addPieSegment (circle, 0.0f, juce::MathConstants<float>::pi, 0.0f);
        g.setColour (Theme::buttonText);
        g.fillPath (half);
        g.drawEllipse (circle, 1.2f);
    }

private:
    // A little strip showing a scheme's background and its five accent colours.
    static std::unique_ptr<juce::Drawable> swatch (const Theme::Palette& p)
    {
        juce::Image image (juce::Image::ARGB, 66, 16, true);
        juce::Graphics g (image);
        g.setColour (p.background);
        g.fillRoundedRectangle (0.5f, 0.5f, 65.0f, 15.0f, 4.0f);
        g.setColour (p.line);
        g.drawRoundedRectangle (0.5f, 0.5f, 65.0f, 15.0f, 4.0f, 1.0f);
        int i = 0;
        for (auto c : { p.carrier, p.voice, p.efx, p.output, p.eq })
        {
            g.setColour (c);
            g.fillEllipse (6.0f + (float) i++ * 11.5f, 3.5f, 9.0f, 9.0f);
        }
        auto drawable = std::make_unique<juce::DrawableImage>();
        drawable->setImage (image);
        return drawable;
    }

    void showMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Colour scheme");
        for (int i = 0; i < (int) Theme::schemes().size(); ++i)
        {
            const auto& s = Theme::schemes()[(size_t) i];
            juce::PopupMenu::Item item (s.name);
            item.setTicked (i == Theme::currentScheme())
                .setImage (swatch (Theme::isLight() ? s.light : s.dark))
                .setAction ([i] { Theme::select (i, Theme::isLight()); });
            menu.addItem (std::move (item));
        }

        menu.addSeparator();
        menu.addItem ("Dark", true, ! Theme::isLight(), [] { Theme::select (Theme::currentScheme(), false); });
        menu.addItem ("Light", true, Theme::isLight(), [] { Theme::select (Theme::currentScheme(), true); });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }
};

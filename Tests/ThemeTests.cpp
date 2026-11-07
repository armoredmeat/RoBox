// Tests for the colour schemes: every scheme stays readable, in dark and light.
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"

namespace
{
    // WCAG contrast ratio between two colours: 1 (same) .. 21 (black on white).
    double contrast (juce::Colour a, juce::Colour b)
    {
        auto luminance = [] (juce::Colour c)
        {
            auto channel = [] (juce::uint8 v)
            {
                const double x = v / 255.0;
                return x <= 0.04045 ? x / 12.92 : std::pow ((x + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue());
        };
        const double la = luminance (a), lb = luminance (b);
        return (juce::jmax (la, lb) + 0.05) / (juce::jmin (la, lb) + 0.05);
    }
}

class ThemeTests final : public juce::UnitTest
{
public:
    ThemeTests() : juce::UnitTest ("Themes", "RoBox") {}

    void runTest() override
    {
        beginTest ("5 schemes, each with a dark and a light version");
        expectEquals ((int) Theme::schemes().size(), 5);
        for (const auto& s : Theme::schemes())
        {
            expect (s.dark.background.getPerceivedBrightness() < 0.3f, juce::String (s.name) + " dark isn't dark");
            expect (s.light.background.getPerceivedBrightness() > 0.8f, juce::String (s.name) + " light isn't light");
        }

        beginTest ("Every scheme is readable (WCAG contrast)");
        for (const auto& s : Theme::schemes())
        {
            for (bool light : { false, true })
            {
                const auto& p = light ? s.light : s.dark;
                const auto label = juce::String (s.name) + (light ? " light" : " dark");
                auto check = [&] (const char* what, juce::Colour a, juce::Colour b, double needed)
                {
                    const double ratio = contrast (a, b);
                    expect (ratio >= needed, label + ": " + what + " contrast " + juce::String (ratio, 2) + " < " + juce::String (needed));
                };

                check ("text on background", p.text, p.background, 7.0);
                check ("text on panel", p.text, p.panel, 4.5);
                check ("subtext on background", p.subtext, p.background, 4.5);
                check ("subtext on panel", p.subtext, p.panel, 3.0);
                check ("button text on background", p.buttonText, p.background, 4.5);
                check ("dimmed on background", p.dimmed, p.background, 2.0);
                check ("carrier ink on carrier", p.carrierInk, p.carrier, 4.5);
                check ("voice ink on voice", p.voiceInk, p.voice, 4.5);
                check ("EQ ink on EQ", p.eqInk, p.eq, 4.5);
                check ("white keys vs black keys", p.whiteKey, p.blackKey, 3.0);
                check ("meter red on panel", p.danger, p.panel, 3.0);
                for (auto [role, accent] : { std::pair { "carrier", p.carrier }, std::pair { "voice", p.voice },
                                             std::pair { "efx", p.efx }, std::pair { "output", p.output }, std::pair { "eq", p.eq } })
                {
                    check ((juce::String (role) + " on background").toRawUTF8(), accent, p.background, 3.0);
                    check ((juce::String (role) + " on panel").toRawUTF8(), accent, p.panel, 2.6);
                }
            }
        }

        beginTest ("Selecting a scheme changes the live colours");
        {
            const int before = Theme::version();
            Theme::select (1, true, false); // Neon light, not saved
            expect (Theme::version() > before);
            expect (Theme::background == Theme::schemes()[1].light.background);
            expect (Theme::carrier == Theme::schemes()[1].light.carrier);
            expect (Theme::isLight() && Theme::currentScheme() == 1);
            Theme::select (0, false, false); // back to Studio dark
            expect (Theme::background == juce::Colour (0xff2C2C2A));
        }
    }
};

static ThemeTests themeTests;

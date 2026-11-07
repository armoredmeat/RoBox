#include "Theme.h"

namespace Theme
{
namespace
{
    int schemeIndex = 0;
    bool light = false;
    int changeCount = 0;
    bool loaded = false;

    // %APPDATA%\RoBox\RoBox.settings on Windows (~/Library/Application Support/RoBox on a Mac).
    juce::PropertiesFile::Options settingsOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "RoBox";
        o.folderName = "RoBox";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    void apply (const Palette& p)
    {
        background = p.background; panel = p.panel; line = p.line;
        text = p.text; subtext = p.subtext; dimmed = p.dimmed; buttonText = p.buttonText;
        whiteKey = p.whiteKey; blackKey = p.blackKey;
        carrier = p.carrier; carrierInk = p.carrierInk;
        voice = p.voice; voiceInk = p.voiceInk;
        efx = p.efx; output = p.output;
        eq = p.eq; eqInk = p.eqInk;
        danger = p.danger;
    }

    // Builds a palette from 18 hex colours, in the order of the roles in Palette:
    // background panel line | text subtext dimmed buttonText | whiteKey blackKey |
    // carrier carrierInk | voice voiceInk | efx output | eq eqInk | danger
    Palette fromHex (const char* list)
    {
        const auto hex = juce::StringArray::fromTokens (list, false);
        jassert (hex.size() == 18);
        auto c = [&hex] (int i) { return juce::Colour (0xff000000 | (juce::uint32) hex[i].getHexValue32()); };
        return { c (0), c (1), c (2), c (3), c (4), c (5), c (6), c (7), c (8),
                 c (9), c (10), c (11), c (12), c (13), c (14), c (15), c (16), c (17) };
    }
}

// Every pair of colours here was checked for readable contrast (WCAG): text on the background
// at least 7:1, accents at least 3:1, "ink" on its accent at least 4.5:1. ThemeTests re-checks them.
const std::vector<Scheme>& schemes()
{
    static const std::vector<Scheme> list {
        // Studio: warm greys with soft pastel accents (the original look)
        { "Studio",
          /* dark  */ fromHex ("2C2C2A 444441 5F5E5A F1EFE8 B4B2A9 888780 D3D1C7 D3D1C7 2C2C2A 5DCAA5 04342C AFA9EC 26215C F0997B EF9F27 85B7EB 042C53 FF6B6B"),
          /* light */ fromHex ("F1EFE8 E2E0D8 C4C2B9 2C2C2A 5F5E5A 8E8C84 444441 FFFFFF 444441 0E7F62 FFFFFF 5B50C8 FFFFFF C4532C A86400 2C6CB0 FFFFFF CC2E2E") },
        // Neon: electro night: near-black indigo with cyan, magenta and gold
        { "Neon",
          /* dark  */ fromHex ("12101C 221E33 3A3452 F3F0FF A9A3C7 6E6890 D6D1F0 D6D1F0 12101C 22E6D1 04201D FF4FD8 3A0631 FF8A3D FFE14D 7C8CFF 0B1040 FF3B5C"),
          /* light */ fromHex ("F6F4FC E7E3F5 C9C3E0 1A1630 5A5478 948EB0 2E2948 FFFFFF 2E2948 007E70 FFFFFF C0189A FFFFFF D2561A 8F7000 4450D8 FFFFFF D81F3C") },
        // Sunset: Miami dusk: plum with aqua, hot pink and peach
        { "Sunset",
          /* dark  */ fromHex ("1E1424 33223B 4E3958 FFF1EA D2B8C4 8F7590 EBD6DE F0DCE2 1E1424 3FD0E0 06343A FF6FA3 4A0A24 FF9E5E FFC94D B48CFF 2A1355 FF4A4A"),
          /* light */ fromHex ("FFF4EE F7E2DA E3C2B8 2B1A2E 6E4F66 A38898 46304A FFFFFF 46304A 077A8A FFFFFF CC306E FFFFFF C85A1A 946600 7046D0 FFFFFF CC2C2C") },
        // Retro: 80s hardware: brown-black and cream with orange, olive and teal
        { "Retro",
          /* dark  */ fromHex ("211D17 352E25 524739 F3E9D2 C4B597 857862 DCCFB2 E9DFC6 211D17 E8A33D 3A2504 C9D16A 2F3305 E0663A F2D35C 6FB7A8 0D3029 E2483A"),
          /* light */ fromHex ("EFE6D2 E2D6BC C7B796 2A241B 5E5240 958768 3E362A FBF6EA 3E362A A8600C FFFFFF 5F6A0E FFFFFF AE4019 806800 2A7366 FFFFFF BC3027") },
        // Phosphor: green-screen terminal: black-green with neon greens and amber
        { "Phosphor",
          /* dark  */ fromHex ("0C140F 15241A 25402D D8F5DE 8FC29C 4F7A5B B8E6C2 B8E6C2 0C140F 39FF88 03220F A6FF4D 1E3303 FFB347 4DFFD2 7BD3FF 052333 FF5544"),
          /* light */ fromHex ("EEF6EF DDEBDF B5CFBA 0F2416 3F6249 82A089 1E3A26 FFFFFF 1E3A26 0B8442 FFFFFF 487E08 FFFFFF B05F00 087C67 1D70A4 FFFFFF C42F22") },
    };
    return list;
}

void select (int scheme, bool useLight, bool remember)
{
    schemeIndex = juce::jlimit (0, (int) schemes().size() - 1, scheme);
    light = useLight;
    const auto& s = schemes()[(size_t) schemeIndex];
    apply (light ? s.light : s.dark);
    ++changeCount;

    if (remember)
    {
        juce::PropertiesFile settings (settingsOptions());
        settings.setValue ("themeScheme", juce::String (s.name));
        settings.setValue ("themeLight", light);
        settings.saveIfNeeded();
    }
}

int currentScheme() { return schemeIndex; }
bool isLight()      { return light; }
int version()       { return changeCount; }

void loadSaved()
{
    if (std::exchange (loaded, true))
        return;

    juce::PropertiesFile settings (settingsOptions());
    const auto name = settings.getValue ("themeScheme", "Studio"); // stored by name, so reordering is safe
    int index = 0;
    for (size_t i = 0; i < schemes().size(); ++i)
        if (name == schemes()[i].name)
            index = (int) i;
    select (index, settings.getBoolValue ("themeLight", false), false);
}
}

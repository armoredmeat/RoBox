#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// RoBox colours and fonts, in one place so the whole UI stays consistent.
//
// There are 5 colour schemes, each with a dark and a light version (see Theme.cpp).
// The colours below are the *live* ones that everything draws with; Theme::select() swaps
// them for another scheme. The choice is a personal preference, saved in RoBox's settings
// file (not in projects), so every RoBox you open uses the same theme.
namespace Theme
{
    // The roles every scheme fills in.
    struct Palette
    {
        juce::Colour background, panel, line;          // window, display/knob centres, borders and tracks
        juce::Colour text, subtext, dimmed, buttonText;
        juce::Colour whiteKey, blackKey;
        juce::Colour carrier, carrierInk;              // one accent per section, with an "ink" colour
        juce::Colour voice, voiceInk;                  // for text drawn on top of the accent
        juce::Colour efx, output;
        juce::Colour eq, eqInk;
        juce::Colour danger;                           // the meter near clipping
    };

    struct Scheme
    {
        const char* name;
        Palette dark, light;
    };

    const std::vector<Scheme>& schemes();

    // Live colours (Studio, dark until the saved choice is loaded). Only change via select().
    inline juce::Colour background { 0xff2C2C2A };
    inline juce::Colour panel      { 0xff444441 };
    inline juce::Colour line       { 0xff5F5E5A };
    inline juce::Colour text       { 0xffF1EFE8 };
    inline juce::Colour subtext    { 0xffB4B2A9 };
    inline juce::Colour dimmed     { 0xff888780 }; // controls that don't apply right now
    inline juce::Colour buttonText { 0xffD3D1C7 };
    inline juce::Colour whiteKey   { 0xffD3D1C7 };
    inline juce::Colour blackKey   { 0xff2C2C2A };
    inline juce::Colour carrier    { 0xff5DCAA5 };
    inline juce::Colour carrierInk { 0xff04342C };
    inline juce::Colour voice      { 0xffAFA9EC };
    inline juce::Colour voiceInk   { 0xff26215C };
    inline juce::Colour efx        { 0xffF0997B };
    inline juce::Colour output     { 0xffEF9F27 };
    inline juce::Colour eq         { 0xff85B7EB };
    inline juce::Colour eqInk      { 0xff042C53 };
    inline juce::Colour danger     { 0xffFF6B6B };

    // A see-through wash for hover / pressed states that works on dark and light themes.
    inline juce::Colour hover (float amount) { return text.withAlpha (amount); }

    // Switch scheme (index into schemes()) and dark/light. remember = save it as your preference.
    void select (int scheme, bool light, bool remember = true);
    int currentScheme();
    bool isLight();
    int version();     // goes up on every change, so open windows know to recolour
    void loadSaved();  // apply the saved preference (only does anything the first time)

    inline juce::Font font (float size, bool bold = false)
    {
        juce::Font f { juce::FontOptions (size) };
        return bold ? f.boldened() : f;
    }
}

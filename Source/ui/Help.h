#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Help text for the status bar at the bottom of the window.
// Each control stores its description as a property; when the mouse is over any part of a
// control (say, the slider inside a Knob), we walk up the parents until we find one.
namespace Help
{
    inline const juce::Identifier property { "helpText" };

    inline void set (juce::Component& c, const juce::String& text) { c.getProperties().set (property, text); }

    inline juce::String find (const juce::Component* c)
    {
        for (; c != nullptr; c = c->getParentComponent())
            if (auto* text = c->getProperties().getVarPointer (property))
                return text->toString();
        return {};
    }
}

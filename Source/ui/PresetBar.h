#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../presets/PresetManager.h"

// The preset controls in the top bar: [<] [ preset name  v ] [>] [Save]
// Click the name for the full list (factory and your own), plus Save As / Delete / Open Folder.
// A "*" after the name means you've changed something since loading it.
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);
    ~PresetBar() override;

    void refresh(); // call regularly: updates the name and the "changed" mark
    void resized() override;

private:
    // Small outlined button with a chevron, matching the other buttons.
    struct ArrowButton final : juce::Button
    {
        explicit ArrowButton (bool pointsRight) : juce::Button (pointsRight ? "Next" : "Previous"), right (pointsRight) {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        bool right;
    };

    // The name box: looks like a drop-down, opens the preset menu.
    struct NameBox final : juce::Button
    {
        NameBox() : juce::Button ("Preset") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
        juce::String text;
    };

    void showMenu();
    void askToSave();
    void saveAs (const juce::String& name);
    void askToDelete();

    // Dialogs use this window's look. They're closed if the plugin window closes first,
    // so they never outlive it.
    juce::AlertWindow& makeDialog (const juce::String& title, const juce::String& message,
                                   const juce::String& okText, juce::MessageBoxIconType);
    juce::Component::SafePointer<juce::AlertWindow> dialog;

    PresetManager& manager;
    ArrowButton previous { false }, next { true };
    NameBox nameBox;
    juce::TextButton saveButton { "Save" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

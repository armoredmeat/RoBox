#include "PresetBar.h"
#include "Theme.h"
#include "Help.h"

namespace
{
    void outline (juce::Graphics& g, juce::Rectangle<float> r, bool highlighted, bool down)
    {
        r = r.reduced (0.5f);
        if (down || highlighted)
        {
            g.setColour (Theme::hover (down ? 0.14f : 0.07f));
            g.fillRoundedRectangle (r, 6.0f);
        }
        g.setColour (Theme::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
}

PresetBar::PresetBar (PresetManager& m) : manager (m)
{
    for (auto* c : std::initializer_list<juce::Component*> { &previous, &nameBox, &next, &saveButton })
        addAndMakeVisible (c);

    previous.onClick   = [this] { manager.loadPrevious(); refresh(); };
    next.onClick       = [this] { manager.loadNext(); refresh(); };
    nameBox.onClick    = [this] { showMenu(); };
    saveButton.onClick = [this] { askToSave(); };

    Help::set (previous,   "Previous preset: load the one before this in the list.");
    Help::set (next,       "Next preset: load the one after this in the list.");
    Help::set (nameBox,    "Preset: click for the full list. A * means you've changed something since loading it.");
    Help::set (saveButton, "Save: store the current sound as your own preset (in Documents/RoBox/Presets).");

    refresh();
}

PresetBar::~PresetBar()
{
    delete dialog.getComponent();
}

juce::AlertWindow& PresetBar::makeDialog (const juce::String& title, const juce::String& message,
                                          const juce::String& okText, juce::MessageBoxIconType icon)
{
    auto* window = new juce::AlertWindow (title, message, icon, this);
    window->setLookAndFeel (&getLookAndFeel());
    if (okText.isNotEmpty())
    {
        window->addButton (okText, 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    }
    dialog = window;
    return *window;
}

void PresetBar::refresh()
{
    const auto text = manager.getCurrentName() + (manager.isModified() ? " *" : "");
    if (text != nameBox.text)
    {
        nameBox.text = text;
        nameBox.repaint();
    }
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    saveButton.setBounds (r.removeFromRight (46));
    r.removeFromRight (4);
    next.setBounds (r.removeFromRight (26));
    r.removeFromRight (4);
    previous.setBounds (r.removeFromLeft (26));
    r.removeFromLeft (4);
    nameBox.setBounds (r);
}

void PresetBar::showMenu()
{
    manager.rescan(); // pick up presets added or removed outside the plugin
    const auto& presets = manager.getPresets();
    const int current = manager.getCurrentIndex();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Factory");
    for (int i = 0; i < manager.getNumFactoryPresets(); ++i)
        menu.addItem (presets[(size_t) i].name, true, i == current, [this, i] { manager.load (i); refresh(); });

    menu.addSectionHeader ("Your presets");
    if ((int) presets.size() == manager.getNumFactoryPresets())
        menu.addItem ("None yet: click Save to make one", false, false, nullptr);
    for (int i = manager.getNumFactoryPresets(); i < (int) presets.size(); ++i)
        menu.addItem (presets[(size_t) i].name, true, i == current, [this, i] { manager.load (i); refresh(); });

    menu.addSeparator();
    menu.addItem ("Save as new preset...", [this] { askToSave(); });
    if (manager.isCurrentUserPreset() && current >= 0)
        menu.addItem ("Delete \"" + manager.getCurrentName() + "\"...", [this] { askToDelete(); });
    menu.addItem ("Open presets folder", [this]
    {
        manager.getUserFolder().createDirectory();
        manager.getUserFolder().startAsProcess(); // opens it in Explorer / Finder
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (nameBox).withMinimumWidth (nameBox.getWidth()));
}

void PresetBar::askToSave()
{
    // Suggest the current name for your own presets (so Save updates it), a fresh one for factory presets.
    const auto suggestion = manager.isCurrentUserPreset() ? manager.getCurrentName() : manager.getCurrentName() + " (mine)";

    auto* window = &makeDialog ("Save preset", "Name your preset:", "Save", juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("name", suggestion);
    if (auto* editor = window->getTextEditor ("name"))
        editor->selectAll();

    window->enterModalState (true, juce::ModalCallbackFunction::create (
        [safeThis = juce::Component::SafePointer<PresetBar> (this), window] (int result)
        {
            if (result != 1 || safeThis == nullptr)
                return;
            const auto name = PresetManager::cleanName (window->getTextEditorContents ("name"));
            if (name.isEmpty())
                return;

            if (! safeThis->manager.userPresetExists (name) || (safeThis->manager.isCurrentUserPreset() && safeThis->manager.getCurrentName() == name))
            {
                safeThis->saveAs (name); // new name, or re-saving the preset you're on
                return;
            }

            // Saving over a different preset you made earlier: check first.
            safeThis->makeDialog ("Replace preset?", "You already have a preset called \"" + name + "\". Replace it?",
                                  "Replace", juce::MessageBoxIconType::QuestionIcon)
                .enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, name] (int choice)
                {
                    if (choice == 1 && safeThis != nullptr)
                        safeThis->saveAs (name);
                }), true);
        }), true);
}

void PresetBar::saveAs (const juce::String& name)
{
    if (! manager.save (name))
    {
        auto& window = makeDialog ("Couldn't save", "RoBox couldn't write to " + manager.getUserFolder().getFullPathName(),
                                   {}, juce::MessageBoxIconType::WarningIcon);
        window.addButton ("OK", 0, juce::KeyPress (juce::KeyPress::returnKey));
        window.enterModalState (true, nullptr, true);
    }
    refresh();
}

void PresetBar::askToDelete()
{
    const auto name = manager.getCurrentName();
    makeDialog ("Delete preset?", "Move \"" + name + "\" to the Recycle Bin? Your current sound stays as it is.",
                "Delete", juce::MessageBoxIconType::QuestionIcon)
        .enterModalState (true, juce::ModalCallbackFunction::create (
            [safeThis = juce::Component::SafePointer<PresetBar> (this)] (int choice)
            {
                if (choice == 1 && safeThis != nullptr)
                {
                    safeThis->manager.deleteCurrent();
                    safeThis->refresh();
                }
            }), true);
}

void PresetBar::ArrowButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    outline (g, r, highlighted, down);

    const auto c = r.getCentre();
    const float s = 3.5f, dir = right ? 1.0f : -1.0f;
    juce::Path chevron;
    chevron.startNewSubPath (c.x - dir * s * 0.5f, c.y - s);
    chevron.lineTo (c.x + dir * s * 0.5f, c.y);
    chevron.lineTo (c.x - dir * s * 0.5f, c.y + s);
    g.setColour (Theme::buttonText);
    g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PresetBar::NameBox::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat();
    outline (g, r, highlighted, down);

    g.setColour (Theme::text);
    g.setFont (Theme::font (12.5f));
    g.drawText (text, r.reduced (10.0f, 0.0f).withTrimmedRight (14.0f), juce::Justification::centredLeft, true);

    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    juce::Path arrow;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Theme::subtext);
    g.fillPath (arrow);
}

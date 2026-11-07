#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Loads, saves and browses presets.
//
// A preset is a small XML file listing parameter values in real units, e.g.
//     <RoBoxPreset name="Electro Robot" version="1">
//       <PARAM id="octave" value="2"/>
//     </RoBoxPreset>
// Any parameter a preset doesn't mention goes back to its default, so factory presets only
// list what they change. User presets list everything.
//
// Factory presets are built into the plugin (Presets/*.xml, embedded by CMake).
// User presets are .robox files in Documents/RoBox/Presets, so they're easy to back up and share.
//
// The current preset's name is stored in the plugin state, so it's remembered with your project.
// Everything here runs on the message thread (UI or host calls), never the audio thread.
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        bool isUser = false;
        juce::File file;      // user presets
        juce::String xmlText; // factory presets
    };

    PresetManager (juce::AudioProcessor&, juce::AudioProcessorValueTreeState&,
                   juce::File userFolder = defaultUserFolder());

    static juce::File defaultUserFolder();
    static constexpr const char* fileExtension = ".robox";

    void rescan(); // re-read the user preset folder
    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    int getNumFactoryPresets() const noexcept { return numFactory; }

    int getCurrentIndex() const;   // -1 if the current preset no longer exists (e.g. deleted)
    juce::String getCurrentName() const;
    bool isCurrentUserPreset() const;
    bool isModified() const;       // have any settings changed since the preset was loaded?

    bool load (int index);
    void loadNext();
    void loadPrevious();

    bool userPresetExists (const juce::String& name) const;
    bool save (const juce::String& name); // as a user preset; replaces one with the same name
    bool deleteCurrent();                 // user presets only

    void stateRestored(); // call after the host restores the plugin state

    const juce::File& getUserFolder() const noexcept { return userFolder; }
    static juce::String cleanName (const juce::String& name);
    static std::unique_ptr<juce::XmlElement> parse (const Preset&);

private:
    std::vector<juce::RangedAudioParameter*> presetParameters() const;
    std::vector<float> valuesFrom (const juce::XmlElement&) const; // normalised 0..1, one per preset parameter
    std::vector<float> currentValues() const;
    void setCurrent (const juce::String& name, bool isUser, std::vector<float> values);
    int indexOf (const juce::String& name, bool isUser) const;

    juce::AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;
    juce::File userFolder;
    std::vector<Preset> presets; // factory first (in file order), then user (A to Z)
    int numFactory = 0;
    std::vector<float> loadedValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

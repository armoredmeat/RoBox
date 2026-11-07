#include "PresetManager.h"
#include "../Parameters.h"
#include <BinaryData.h>

namespace
{
    const juce::Identifier nameProperty   { "presetName" };
    const juce::Identifier isUserProperty { "presetIsUser" };
    const juce::String rootTag = "RoBoxPreset", paramTag = "PARAM";

    // Carrier Solo is a listening aid, not part of a sound, so presets never store or change it.
    bool belongsInPresets (const juce::String& id) { return id != ParamID::carrierSolo; }
}

PresetManager::PresetManager (juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& state, juce::File folder)
    : processor (p), apvts (state), userFolder (std::move (folder))
{
    // Factory presets, sorted by file name ("01 Init.xml", "02 ...") so their order is fixed.
    std::vector<std::pair<juce::String, Preset>> factory;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String file = BinaryData::originalFilenames[i];
        if (! file.endsWithIgnoreCase (".xml"))
            continue;
        int size = 0;
        if (const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
        {
            Preset preset;
            preset.xmlText = juce::String::fromUTF8 (data, size);
            if (auto xml = parse (preset))
            {
                preset.name = xml->getStringAttribute ("name", file.upToLastOccurrenceOf (".", false, false));
                factory.emplace_back (file, preset);
            }
        }
    }
    std::sort (factory.begin(), factory.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& f : factory)
        presets.push_back (f.second);
    numFactory = (int) presets.size();

    rescan();
    stateRestored(); // a fresh instance starts on "Init"
}

juce::File PresetManager::defaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("RoBox").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    presets.resize ((size_t) numFactory);

    auto files = userFolder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });

    for (const auto& f : files)
    {
        Preset preset;
        preset.isUser = true;
        preset.file = f;
        preset.name = f.getFileNameWithoutExtension();
        presets.push_back (preset);
    }
}

std::unique_ptr<juce::XmlElement> PresetManager::parse (const Preset& preset)
{
    auto xml = preset.isUser ? juce::XmlDocument::parse (preset.file) : juce::XmlDocument::parse (preset.xmlText);
    if (xml == nullptr || ! xml->hasTagName (rootTag))
        return nullptr;
    return xml;
}

std::vector<juce::RangedAudioParameter*> PresetManager::presetParameters() const
{
    std::vector<juce::RangedAudioParameter*> result;
    for (auto* p : processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (belongsInPresets (ranged->getParameterID()))
                result.push_back (ranged);
    return result;
}

std::vector<float> PresetManager::valuesFrom (const juce::XmlElement& xml) const
{
    std::vector<float> values;
    for (auto* p : presetParameters())
    {
        float value = p->getDefaultValue();
        for (auto* e : xml.getChildWithTagNameIterator (paramTag))
            if (e->getStringAttribute ("id") == p->getParameterID())
                value = p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue ((float) e->getDoubleAttribute ("value")));
        values.push_back (value);
    }
    return values;
}

std::vector<float> PresetManager::currentValues() const
{
    std::vector<float> values;
    for (auto* p : presetParameters())
        values.push_back (p->getValue());
    return values;
}

void PresetManager::setCurrent (const juce::String& name, bool isUser, std::vector<float> values)
{
    apvts.state.setProperty (nameProperty, name, nullptr);
    apvts.state.setProperty (isUserProperty, isUser, nullptr);
    loadedValues = std::move (values);
}

int PresetManager::indexOf (const juce::String& name, bool isUser) const
{
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].isUser == isUser && presets[i].name == name)
            return (int) i;
    return -1;
}

int PresetManager::getCurrentIndex() const  { return indexOf (getCurrentName(), isCurrentUserPreset()); }
juce::String PresetManager::getCurrentName() const { return apvts.state.getProperty (nameProperty).toString(); }
bool PresetManager::isCurrentUserPreset() const { return (bool) apvts.state.getProperty (isUserProperty, false); }

bool PresetManager::isModified() const
{
    const auto now = currentValues();
    if (now.size() != loadedValues.size())
        return true;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::abs (now[i] - loadedValues[i]) > 1.0e-4f)
            return true;
    return false;
}

bool PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return false;

    const auto& preset = presets[(size_t) index];
    auto xml = parse (preset);
    if (xml == nullptr)
        return false;

    // Set every parameter as its own undoable, host-visible change ("gesture").
    const auto values = valuesFrom (*xml);
    const auto params = presetParameters();
    for (size_t i = 0; i < params.size(); ++i)
    {
        params[i]->beginChangeGesture();
        params[i]->setValueNotifyingHost (values[i]);
        params[i]->endChangeGesture();
    }

    setCurrent (preset.name, preset.isUser, values);
    return true;
}

void PresetManager::loadNext()
{
    if (presets.empty()) return;
    load ((getCurrentIndex() + 1) % (int) presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.empty()) return;
    const int current = getCurrentIndex();
    load (current <= 0 ? (int) presets.size() - 1 : current - 1);
}

juce::String PresetManager::cleanName (const juce::String& name)
{
    // Only characters that are safe in a file name, and not too long for the preset box.
    return juce::File::createLegalFileName (name.trim()).substring (0, 40).trim();
}

bool PresetManager::userPresetExists (const juce::String& name) const
{
    return indexOf (cleanName (name), true) >= 0;
}

bool PresetManager::save (const juce::String& rawName)
{
    const auto name = cleanName (rawName);
    if (name.isEmpty() || ! userFolder.createDirectory())
        return false;

    juce::XmlElement xml (rootTag);
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (auto* p : presetParameters())
    {
        auto* e = xml.createNewChildElement (paramTag);
        e->setAttribute ("id", p->getParameterID());
        e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
    }

    const auto file = userFolder.getChildFile (name + fileExtension);
    if (! xml.writeTo (file))
        return false;

    rescan();
    setCurrent (name, true, currentValues());
    return true;
}

bool PresetManager::deleteCurrent()
{
    const int index = getCurrentIndex();
    if (index < 0 || ! presets[(size_t) index].isUser)
        return false;
    if (! presets[(size_t) index].file.moveToTrash() && ! presets[(size_t) index].file.deleteFile())
        return false;

    rescan();
    // The sound doesn't change; it's just no longer saved anywhere.
    setCurrent (getCurrentName(), true, loadedValues);
    return true;
}

void PresetManager::stateRestored()
{
    auto name = getCurrentName();
    bool isUser = isCurrentUserPreset();
    if (name.isEmpty()) // new instance, or a project saved before presets existed
    {
        name = numFactory > 0 ? presets.front().name : juce::String ("Init");
        isUser = false;
    }

    // Remember what the preset itself sounds like, so we can tell if the project changed it.
    std::vector<float> values = currentValues();
    const int index = indexOf (name, isUser);
    if (index >= 0)
        if (auto xml = parse (presets[(size_t) index]))
            values = valuesFrom (*xml);

    setCurrent (name, isUser, values);
}

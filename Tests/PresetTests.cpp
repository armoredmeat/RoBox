// Tests for presets: the factory set, loading, saving, browsing, and that every factory preset
// actually sounds sane (not silent, not clipping) through the whole plugin.
#include <juce_audio_processors/juce_audio_processors.h>
#include <set>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "TestSignals.h"

namespace
{
    float value (RoBoxAudioProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    void set (RoBoxAudioProcessor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    int indexOfName (const PresetManager& m, const juce::String& name)
    {
        for (size_t i = 0; i < m.getPresets().size(); ++i)
            if (m.getPresets()[i].name == name)
                return (int) i;
        return -1;
    }

    struct Level { float peak = 0.0f, rmsDb = -100.0f; bool finite = true; };

    // Two seconds of a sung "ah" (-20 dBFS RMS) through the whole plugin; measures the last second.
    Level render (RoBoxAudioProcessor& p)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 512;
        p.prepareToPlay (rate, block);
        const auto voice = TestSignals::vowel ((int) rate * 2, rate);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        Level level;
        double sum = 0.0;
        int counted = 0;
        for (int start = 0; start + block <= (int) voice.size(); start += block)
        {
            for (int ch = 0; ch < 2; ++ch)
                buffer.copyFrom (ch, 0, voice.data() + start, block);
            p.processBlock (buffer, midi);
            if (start < (int) rate)
                continue;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    level.finite = level.finite && std::isfinite (v);
                    level.peak = juce::jmax (level.peak, std::abs (v));
                    sum += (double) v * v;
                    ++counted;
                }
        }
        level.rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / juce::jmax (1, counted)), -100.0f);
        return level;
    }
}

class PresetTests final : public juce::UnitTest
{
public:
    PresetTests() : juce::UnitTest ("Presets", "RoBox") {}

    void runTest() override
    {
        const auto tempFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("RoBoxPresetTests");
        tempFolder.deleteRecursively();

        beginTest ("Factory presets: 12, Init first, unique names, every value valid");
        {
            RoBoxAudioProcessor p;
            const auto& m = p.presets;
            expectEquals (m.getNumFactoryPresets(), 12);
            expectEquals (m.getPresets().front().name, juce::String ("Init"));

            std::set<juce::String> names;
            for (int i = 0; i < m.getNumFactoryPresets(); ++i)
            {
                const auto& preset = m.getPresets()[(size_t) i];
                expect (names.insert (preset.name).second, "duplicate name " + preset.name);
                auto xml = PresetManager::parse (preset);
                expect (xml != nullptr, preset.name + " doesn't parse");
                if (xml == nullptr)
                    continue;
                for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
                {
                    const auto id = e->getStringAttribute ("id");
                    auto* param = p.apvts.getParameter (id);
                    expect (param != nullptr, preset.name + ": unknown parameter " + id);
                    if (param == nullptr)
                        continue;
                    const auto v = (float) e->getDoubleAttribute ("value");
                    const auto& range = param->getNormalisableRange();
                    expect (v >= range.start && v <= range.end, preset.name + ": " + id + " out of range: " + juce::String (v));
                }
            }
        }

        beginTest ("A new instance starts on Init, unchanged");
        {
            RoBoxAudioProcessor p;
            expectEquals (p.presets.getCurrentName(), juce::String ("Init"));
            expect (! p.presets.isModified());
        }

        beginTest ("Loading sets the preset's values and resets everything else to default");
        {
            RoBoxAudioProcessor p;
            auto& m = p.presets;
            expect (m.load (indexOfName (m, "Lo-Fi Bot")));
            expectEquals (value (p, ParamID::crushBits), 8.0f);
            expect (m.load (indexOfName (m, "Electro Robot")));
            expectEquals (value (p, ParamID::crushBits), 16.0f, "Bit Crush wasn't reset to default");
            expectEquals (value (p, ParamID::octave), 2.0f);
            expectEquals (value (p, ParamID::attack), 2.0f);
            expectEquals (m.getCurrentName(), juce::String ("Electro Robot"));
        }

        beginTest ("The * mark: changed after a knob moves, clear after reloading");
        {
            RoBoxAudioProcessor p;
            auto& m = p.presets;
            m.load (indexOfName (m, "Bass Talker"));
            expect (! m.isModified());
            set (p, ParamID::drive, 60.0f);
            expect (m.isModified());
            m.load (m.getCurrentIndex());
            expect (! m.isModified());
        }

        beginTest ("Carrier Solo is never stored in or changed by a preset");
        {
            RoBoxAudioProcessor p;
            set (p, ParamID::carrierSolo, 1.0f);
            p.presets.load (1);
            expectEquals (value (p, ParamID::carrierSolo), 1.0f);
            expect (! p.presets.isModified(), "Solo shouldn't count as a change to the preset");
        }

        beginTest ("Next / previous step through the list and wrap around");
        {
            RoBoxAudioProcessor p;
            auto& m = p.presets;
            const int count = (int) m.getPresets().size();
            m.load (0);
            m.loadPrevious();
            expectEquals (m.getCurrentIndex(), count - 1);
            m.loadNext();
            expectEquals (m.getCurrentIndex(), 0);
            m.loadNext();
            expectEquals (m.getCurrentIndex(), 1);
        }

        beginTest ("Save, find, reload and delete a user preset");
        {
            RoBoxAudioProcessor p;
            PresetManager m (p, p.apvts, tempFolder);
            m.load (indexOfName (m, "Chipmunk Bot"));
            set (p, ParamID::drive, 42.0f);

            expect (m.save ("  My: Bot/1  "));
            const auto saved = PresetManager::cleanName ("  My: Bot/1  ");
            expect (! saved.containsAnyOf (":/\\"), "unsafe file name: " + saved);
            expect (tempFolder.getChildFile (saved + PresetManager::fileExtension).existsAsFile());
            expect (m.userPresetExists (saved));
            expect (m.isCurrentUserPreset() && ! m.isModified());
            expect (m.getCurrentIndex() >= m.getNumFactoryPresets(), "user presets come after the factory ones");

            m.load (0); // Init
            expectEquals (value (p, ParamID::drive), 0.0f);
            m.load (indexOfName (m, saved));
            expectEquals (value (p, ParamID::drive), 42.0f);
            expectEquals (value (p, ParamID::formant), 7.0f);

            expect (m.deleteCurrent());
            expect (! m.userPresetExists (saved));
            expectEquals ((int) m.getPresets().size(), m.getNumFactoryPresets());
        }

        beginTest ("The preset name is saved with the project");
        {
            juce::MemoryBlock state;
            {
                RoBoxAudioProcessor p;
                p.presets.load (indexOfName (p.presets, "Drone Vox"));
                p.getStateInformation (state);
            }
            RoBoxAudioProcessor restored;
            restored.setStateInformation (state.getData(), (int) state.getSize());
            expectEquals (restored.presets.getCurrentName(), juce::String ("Drone Vox"));
            expect (! restored.presets.isModified());
            expectEquals (value (restored, ParamID::bands), 5.0f);
        }

        beginTest ("Every factory preset makes sound without clipping (-20 dBFS voice in)");
        {
            RoBoxAudioProcessor p;
            auto& m = p.presets;
            for (int i = 0; i < m.getNumFactoryPresets(); ++i)
            {
                m.load (i);
                const auto level = render (p);
                const auto& presetName = m.getPresets()[(size_t) i].name;
                logMessage ("  " + presetName.paddedRight (' ', 16) + " RMS " + juce::String (level.rmsDb, 1)
                            + " dBFS, peak " + juce::String (juce::Decibels::gainToDecibels (level.peak), 1) + " dBFS");
                expect (level.finite, presetName + " produced NaN/inf");
                expect (level.peak < 1.0f, presetName + " clips: peak " + juce::String (level.peak));
                expect (level.rmsDb > -32.0f, presetName + " is too quiet: " + juce::String (level.rmsDb, 1) + " dBFS");
            }
        }

        tempFolder.deleteRecursively();
    }
};

static PresetTests presetTests;

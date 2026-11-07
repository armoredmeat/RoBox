// RoBoxHostTest: loads the built RoBox.vst3 the way a DAW does and runs it through the kind of
// checks a plugin validator makes (like pluginval). It tests the real VST3 file, including the
// JUCE wrapper around our code, not just the processor class.
//
// Usage: RoBoxHostTest <path to RoBox.vst3>
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
#include "../Tests/TestSignals.h"

namespace
{
    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::cout << (ok ? "  [pass] " : "  [FAIL] ") << what << std::endl;
        if (! ok) ++failures;
    }

    void section (const juce::String& name) { std::cout << std::endl << name << std::endl; }

    struct Result { bool finite = true; float peak = 0.0f; double sumSquares = 0.0; size_t count = 0;
                    float rmsDb() const { return juce::Decibels::gainToDecibels ((float) std::sqrt (sumSquares / (double) juce::jmax<size_t> (1, count)), -120.0f); } };

    // Feeds 'seconds' of a sung vowel through the plugin. beforeBlock lets a test poke parameters.
    Result process (juce::AudioPluginInstance& plugin, double rate, int block, double seconds,
                    std::function<void()> beforeBlock = {})
    {
        const int channels = juce::jmax (plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
        const auto voice = TestSignals::vowel ((int) (rate * seconds), rate);
        juce::AudioBuffer<float> buffer (channels, block);
        juce::MidiBuffer midi;
        Result r;
        for (size_t start = 0; start + (size_t) block <= voice.size(); start += (size_t) block)
        {
            for (int ch = 0; ch < channels; ++ch)
                buffer.copyFrom (ch, 0, voice.data() + start, block);
            if (beforeBlock) beforeBlock();
            plugin.processBlock (buffer, midi);
            for (int ch = 0; ch < plugin.getTotalNumOutputChannels(); ++ch)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    r.finite = r.finite && std::isfinite (v);
                    r.peak = juce::jmax (r.peak, std::abs (v));
                    r.sumSquares += (double) v * v;
                    ++r.count;
                }
        }
        return r;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::cout << "Usage: RoBoxHostTest <path to RoBox.vst3>" << std::endl;
        return 2;
    }

    const auto path = juce::File (juce::String (argv[1])).getFullPathName();
    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
    auto* vst3 = formats.getFormat (0);

    section ("Finding the plugin: " + path);
    juce::OwnedArray<juce::PluginDescription> found;
    vst3->findAllTypesForFile (found, path);
    check (found.size() == 1, "the VST3 contains exactly one plugin (" + juce::String (found.size()) + ")");
    if (found.isEmpty())
        return 1;
    const auto description = *found[0];
    check (description.name == "RoBox", "name is RoBox (" + description.name + ")");
    check (! description.isInstrument, "it's an effect, not an instrument");
    check (description.version.isNotEmpty(), "has a version (" + description.version + ")");

    auto create = [&] (double rate = 48000.0, int block = 512)
    {
        juce::String error;
        auto instance = formats.createPluginInstance (description, rate, block, error);
        if (instance == nullptr)
            check (false, "couldn't create an instance: " + error);
        return instance;
    };

    auto plugin = create();
    if (plugin == nullptr)
        return 1;

    section ("Basics");
    check (plugin->acceptsMidi(), "accepts MIDI (for MIDI Follow)");
    check (! plugin->producesMidi(), "doesn't send MIDI");
    plugin->prepareToPlay (48000.0, 512);
    check (plugin->getLatencySamples() == 0, "no latency");
    {
        juce::AudioProcessor::BusesLayout stereo, mono;
        stereo.inputBuses.add (juce::AudioChannelSet::stereo()); stereo.outputBuses.add (juce::AudioChannelSet::stereo());
        mono.inputBuses.add (juce::AudioChannelSet::mono());     mono.outputBuses.add (juce::AudioChannelSet::mono());
        check (plugin->checkBusesLayoutSupported (stereo), "stereo track supported");
        check (plugin->checkBusesLayoutSupported (mono), "mono track supported");
    }

    section ("Parameters");
    const auto params = plugin->getParameters();
    check (params.size() >= 30 && params.size() < 100, juce::String (params.size()) + " parameters exposed to the host (no hidden MIDI CC ones)");
    {
        int unnamed = 0, noText = 0, badDefault = 0;
        juce::StringArray names;
        for (auto* p : params)
        {
            const auto name = p->getName (64);
            if (name.isEmpty()) ++unnamed;
            names.add (name);
            for (float v : { 0.0f, 0.5f, 1.0f })
                if (p->getText (v, 32).isEmpty()) ++noText;
            if (p->getDefaultValue() < 0.0f || p->getDefaultValue() > 1.0f) ++badDefault;
        }
        names.removeDuplicates (false);
        check (unnamed == 0, "every parameter has a name");
        check (names.size() == params.size(), "parameter names are unique");
        check (noText == 0, "every parameter shows its value as text");
        check (badDefault == 0, "every default is in range");
    }

    section ("Sample rates and buffer sizes");
    const int failuresBefore = failures;
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int block : { 16, 64, 333, 1024, 4096 })
        {
            plugin->releaseResources();
            plugin->prepareToPlay (rate, block);
            const auto r = process (*plugin, rate, block, 1.0);
            if (! r.finite || r.rmsDb() < -50.0f || r.peak > 1.0f)
                check (false, juce::String (rate) + " Hz / " + juce::String (block) + ": finite " + juce::String ((int) r.finite)
                              + ", " + juce::String (r.rmsDb(), 1) + " dBFS, peak " + juce::String (r.peak, 3));
        }
    check (failures == failuresBefore, "44.1 to 192 kHz with 16 to 4096-sample buffers: sound, no NaN, no clipping");

    section ("Automation");
    {
        plugin->prepareToPlay (48000.0, 128);
        juce::Random rng (7);
        const auto r = process (*plugin, 48000.0, 128, 5.0, [&]
        {
            for (int k = 0; k < 8; ++k)
                params[rng.nextInt (params.size())]->setValueNotifyingHost (rng.nextFloat());
        });
        check (r.finite, "random automation of every parameter: no NaN (peak " + juce::String (juce::Decibels::gainToDecibels (r.peak), 1) + " dBFS)");
    }

    section ("Saving and restoring (what a DAW does with your project)");
    {
        juce::Random rng (11);
        for (auto* p : params)
            p->setValueNotifyingHost (rng.nextFloat());
        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        check (state.getSize() > 0, "state saved (" + juce::String ((int) state.getSize()) + " bytes)");

        // Compared as the text the DAW shows, so a switch set to 0.88 ("On") matches 1.0 ("On").
        auto same = [] (juce::AudioProcessorParameter* a, juce::AudioProcessorParameter* b)
        {
            return a->getCurrentValueAsText() == b->getCurrentValueAsText();
        };
        auto second = create();
        second->setStateInformation (state.getData(), (int) state.getSize());
        int mismatches = 0;
        for (int i = 0; i < params.size(); ++i)
            if (! same (params[i], second->getParameters()[i]))
                ++mismatches;
        check (mismatches == 0, "a new instance restores every parameter (" + juce::String (mismatches) + " different)");

        juce::MemoryBlock again;
        second->getStateInformation (again);
        auto third = create();
        third->setStateInformation (again.getData(), (int) again.getSize());
        mismatches = 0;
        for (int i = 0; i < params.size(); ++i)
            if (! same (params[i], third->getParameters()[i]))
                ++mismatches;
        check (mismatches == 0, "save -> load -> save -> load keeps everything");

        // Garbage state (a corrupted project) must not crash or break the plugin.
        juce::MemoryBlock junk (300);
        juce::Random (5).fillBitsRandomly (junk.getData(), junk.getSize());
        third->setStateInformation (junk.getData(), (int) junk.getSize());
        third->setStateInformation (nullptr, 0);
        third->prepareToPlay (48000.0, 512);
        check (process (*third, 48000.0, 512, 0.5).finite, "survives a corrupted / empty project state");
    }

    section ("Editor (the plugin window)");
    {
        bool ok = true;
        for (int i = 0; i < 3; ++i)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorAndMakeActive());
            ok = ok && editor != nullptr;
            if (editor == nullptr) break;
            editor->setSize (645, 402);   // 75 %
            editor->setSize (1720, 1072); // 200 %
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
        } // the editor closes here
        check (ok, "opens, resizes and closes 3 times");
    }

    section ("Audio and UI at the same time (threading)");
    {
        plugin->prepareToPlay (48000.0, 256);
        std::atomic<bool> running { true };
        std::atomic<bool> finite { true };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;
            juce::Random rng (3);
            while (running)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                        buffer.setSample (ch, i, rng.nextFloat() * 0.2f - 0.1f);
                midi.clear();
                midi.addEvent (juce::MidiMessage::noteOn (1, 40 + rng.nextInt (24), 0.8f), rng.nextInt (256));
                midi.addEvent (juce::MidiMessage::noteOff (1, 40 + rng.nextInt (24)), rng.nextInt (256));
                plugin->processBlock (buffer, midi);
                for (int i = 0; i < 256; ++i)
                    if (! std::isfinite (buffer.getSample (0, i))) finite = false;
            }
        });

        juce::Random rng (4);
        for (int round = 0; round < 6; ++round)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorAndMakeActive());
            for (int k = 0; k < 200; ++k)
                params[rng.nextInt (params.size())]->setValueNotifyingHost (rng.nextFloat());
            juce::MemoryBlock state;
            plugin->getStateInformation (state);
            plugin->setStateInformation (state.getData(), (int) state.getSize());
            juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        }
        running = false;
        audio.join();
        check (finite, "audio keeps running cleanly while the UI opens/closes, automates and saves state");
    }

    section ("Several instances at once");
    {
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> instances;
        for (int i = 0; i < 4; ++i)
            instances.push_back (create (44100.0, 512));
        bool ok = true;
        for (auto& inst : instances)
        {
            inst->prepareToPlay (44100.0, 512);
            ok = ok && inst != nullptr && process (*inst, 44100.0, 512, 0.5).finite;
        }
        check (ok, "4 instances side by side");
    }

    section ("CPU (defaults, 48 kHz, 128-sample buffers)");
    {
        plugin->setStateInformation (nullptr, 0);
        auto fresh = create (48000.0, 128);
        fresh->prepareToPlay (48000.0, 128);
        const auto t0 = juce::Time::getHighResolutionTicks();
        process (*fresh, 48000.0, 128, 10.0);
        const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        // The test signal is generated inside process(), so this slightly overstates the plugin's share.
        check (seconds / 10.0 < 0.05, juce::String (100.0 * seconds / 10.0, 2) + " % of one core");
    }

    std::cout << std::endl << (failures == 0 ? "HOST TEST PASSED" : "HOST TEST FAILED: " + juce::String (failures) + " problem(s)") << std::endl;
    return failures == 0 ? 0 : 1;
}

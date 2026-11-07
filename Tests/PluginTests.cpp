// Whole-plugin tests (Milestone 9): sample rates, buffer sizes, automation, mode switching,
// state save/restore and CPU, all through the real RoBoxAudioProcessor.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "TestSignals.h"

namespace
{
    void set (RoBoxAudioProcessor& p, const char* id, float v)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    // Runs a mono voice signal through the plugin (copied to every input channel) and returns
    // the left output. Blocks are 'block' samples; the plugin was prepared for 'prepared'.
    std::vector<float> run (RoBoxAudioProcessor& p, const std::vector<float>& voice, double rate, int block,
                            int prepared = -1, int channels = 2, std::function<void (int)> beforeBlock = {},
                            bool prepare = true)
    {
        if (prepare)
            p.prepareToPlay (rate, prepared > 0 ? prepared : block);
        std::vector<float> out (voice.size());
        juce::AudioBuffer<float> buffer (channels, block);
        juce::MidiBuffer midi;
        for (size_t start = 0; start < voice.size(); start += (size_t) block)
        {
            const int n = (int) juce::jmin ((size_t) block, voice.size() - start);
            buffer.setSize (channels, n, false, false, true);
            for (int ch = 0; ch < channels; ++ch)
                buffer.copyFrom (ch, 0, voice.data() + start, n);
            if (beforeBlock)
                beforeBlock ((int) start);
            p.processBlock (buffer, midi);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + n, out.begin() + (std::ptrdiff_t) start);
        }
        return out;
    }

    bool allFinite (const std::vector<float>& x)
    {
        return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); });
    }

    float levelDb (const std::vector<float>& x, size_t from)
    {
        return juce::Decibels::gainToDecibels (TestSignals::rms (x, from), -120.0f);
    }

    // Pitch by autocorrelation: the lag (in samples) where the signal best matches itself.
    double pitchHz (const std::vector<float>& x, double rate, size_t from)
    {
        const int minLag = (int) (rate / 200.0), maxLag = (int) (rate / 40.0);
        const size_t n = (size_t) (rate * 0.2);
        int bestLag = minLag;
        double best = -1.0e30;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double sum = 0.0;
            for (size_t i = from; i < from + n; ++i)
                sum += (double) x[i] * x[i + (size_t) lag];
            if (sum > best) { best = sum; bestLag = lag; }
        }
        return rate / bestLag;
    }
}

class PluginTests final : public juce::UnitTest
{
public:
    PluginTests() : juce::UnitTest ("Whole plugin", "RoBox") {}

    void runTest() override
    {
        beginTest ("Same loudness and stable output at every sample rate and buffer size");
        for (int mode : { 0, 1 })
        {
            float reference = 0.0f;
            for (double rate : { 48000.0, 44100.0, 88200.0, 96000.0 })
                for (int block : { 512, 32, 61, 2048 })
                {
                    RoBoxAudioProcessor p;
                    set (p, ParamID::mode, (float) mode);
                    const auto voice = TestSignals::vowel ((int) rate * 2, rate);
                    const auto out = run (p, voice, rate, block);
                    const float db = levelDb (out, (size_t) rate);
                    if (rate == 48000.0 && block == 512)
                        reference = db;
                    expect (allFinite (out), "NaN/inf at " + juce::String (rate) + " Hz, block " + juce::String (block));
                    expect (std::abs (db - reference) < 3.0f, juce::String (mode == 0 ? "Vocoder" : "Talkbox") + " at "
                            + juce::String (rate) + " Hz / " + juce::String (block) + ": " + juce::String (db, 1)
                            + " dBFS vs " + juce::String (reference, 1) + " at 48 kHz");
                    if (block == 512)
                        logMessage ("  " + juce::String (mode == 0 ? "Vocoder" : "Talkbox") + " at " + juce::String (rate / 1000.0, 1)
                                    + " kHz: " + juce::String (db, 1) + " dBFS");
                }
        }

        beginTest ("The carrier is in tune at every sample rate (C2 = 65.4 Hz)");
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            RoBoxAudioProcessor p;
            set (p, ParamID::carrierSolo, 1.0f);
            set (p, ParamID::noise, 0.0f);
            const auto out = run (p, std::vector<float> ((size_t) rate, 0.0f), rate, 512);
            const double hz = pitchHz (out, rate, (size_t) (rate * 0.5));
            expect (std::abs (1200.0 * std::log2 (hz / 65.406)) < 10.0, "pitch " + juce::String (hz, 2) + " Hz at " + juce::String (rate));
        }

        beginTest ("Hosts that send bigger blocks than promised still work");
        {
            RoBoxAudioProcessor p;
            const auto voice = TestSignals::vowel (96000, 48000.0);
            const auto out = run (p, voice, 48000.0, 4096, 256); // prepared for 256, gets 4096
            expect (allFinite (out) && levelDb (out, 48000) > -40.0f, "oversized blocks broke the output");
        }

        beginTest ("Mono track (1 in, 1 out) works");
        {
            RoBoxAudioProcessor p;
            juce::AudioProcessor::BusesLayout mono;
            mono.inputBuses.add (juce::AudioChannelSet::mono());
            mono.outputBuses.add (juce::AudioChannelSet::mono());
            expect (p.setBusesLayout (mono), "mono layout refused");
            const auto out = run (p, TestSignals::vowel (96000, 48000.0), 48000.0, 512, -1, 1);
            expect (allFinite (out) && levelDb (out, 48000) > -40.0f, "mono output broken");
        }

        beginTest ("Switching Vocoder <-> Talkbox crossfades smoothly");
        for (int from : { 0, 1 })
        {
            RoBoxAudioProcessor p;
            set (p, ParamID::mode, (float) from);
            constexpr double rate = 48000.0;
            const int switchAt = (int) rate; // after 1 s
            const auto voice = TestSignals::vowel ((int) rate * 2, rate);
            const auto out = run (p, voice, rate, 64, -1, 2, [&] (int start)
            {
                if (start == switchAt)
                    set (p, ParamID::mode, (float) (1 - from));
            });

            // Loudness measured one voice cycle (1/120 s) at a time, so a steady sound reads steady.
            // Through the switch it should stay between the quietest and loudest cycles of the
            // steady sound before and after, give or take 3 dB.
            const int cycle = (int) (rate / 120.0);
            auto cycleRms = [&] (int s) { return TestSignals::rms (std::vector<float> (out.begin() + s, out.begin() + s + cycle)); };
            float steadyLow = 1.0e9f, steadyHigh = 0.0f;
            for (int s : { switchAt - (int) (rate * 0.3), switchAt + (int) (rate * 0.2) })
                for (int k = 0; k < 24; ++k)
                {
                    steadyLow = juce::jmin (steadyLow, cycleRms (s + k * cycle));
                    steadyHigh = juce::jmax (steadyHigh, cycleRms (s + k * cycle));
                }
            float lowest = 1.0e9f, highest = 0.0f;
            for (int s = switchAt; s < switchAt + (int) (rate * 0.1); s += cycle)
            {
                lowest = juce::jmin (lowest, cycleRms (s));
                highest = juce::jmax (highest, cycleRms (s));
            }
            logMessage ("  " + juce::String (from == 0 ? "Vocoder -> Talkbox" : "Talkbox -> Vocoder") + ": during the switch "
                        + "quietest cycle " + juce::String (juce::Decibels::gainToDecibels (lowest / steadyLow), 1) + " dB vs steady quietest, "
                        + "loudest " + juce::String (juce::Decibels::gainToDecibels (highest / steadyHigh), 1) + " dB vs steady loudest");
            expect (allFinite (out));
            expect (lowest > steadyLow * 0.7f, "level dropped out during the switch");
            expect (highest < steadyHigh * 1.4f, "level spiked during the switch");
        }

        beginTest ("Wild automation of every parameter: no crash, no NaN, recovers afterwards");
        {
            RoBoxAudioProcessor p;
            constexpr double rate = 48000.0;
            juce::Random rng (2024);
            const auto voice = TestSignals::vowel ((int) rate * 5, rate);
            auto params = p.getParameters();
            const auto out = run (p, voice, rate, 128, -1, 2, [&] (int)
            {
                for (int k = 0; k < 6; ++k)
                    params[rng.nextInt (params.size())]->setValueNotifyingHost (rng.nextFloat());
            });
            float peak = 0.0f;
            for (auto v : out) peak = juce::jmax (peak, std::abs (v));
            logMessage ("  peak during random automation: " + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dBFS");
            expect (allFinite (out), "NaN/inf during automation");
            expect (peak < 30.0f, "output blew up: peak " + juce::String (peak));

            // Back to defaults (without resetting the plugin): should sound like a fresh instance again.
            p.presets.load (0);
            const auto after = run (p, TestSignals::vowel ((int) rate * 2, rate), rate, 128, -1, 2, {}, false);
            RoBoxAudioProcessor fresh;
            const auto reference = run (fresh, TestSignals::vowel ((int) rate * 2, rate), rate, 512);
            expect (std::abs (levelDb (after, (size_t) rate) - levelDb (reference, (size_t) rate)) < 1.0f, "didn't recover after automation");
        }

        beginTest ("Low and High EQ bands reach 20 Hz to 20 kHz");
        {
            RoBoxAudioProcessor p;
            for (auto* id : { ParamID::eqLowFreq, ParamID::eqHighFreq })
            {
                const auto& range = p.apvts.getParameter (id)->getNormalisableRange();
                expectEquals (range.start, 20.0f);
                expectEquals (range.end, 20000.0f);
            }
            expectEquals (p.apvts.getParameter (ParamID::eqLowFreq)->convertFrom0to1 (p.apvts.getParameter (ParamID::eqLowFreq)->getDefaultValue()), 120.0f);
            expectEquals (p.apvts.getParameter (ParamID::eqHighFreq)->convertFrom0to1 (p.apvts.getParameter (ParamID::eqHighFreq)->getDefaultValue()), 8000.0f);
        }

        beginTest ("Saving and restoring keeps every parameter exactly");
        {
            RoBoxAudioProcessor a;
            juce::Random rng (99);
            for (auto* param : a.getParameters())
                param->setValueNotifyingHost (rng.nextFloat());
            juce::MemoryBlock state;
            a.getStateInformation (state);

            RoBoxAudioProcessor b;
            b.setStateInformation (state.getData(), (int) state.getSize());
            // Compare values as the plugin reads them: a switch set to 0.88 *is* "on", and comes back as 1.0.
            auto asUsed = [] (juce::AudioProcessorParameter* p)
            {
                auto* r = dynamic_cast<juce::RangedAudioParameter*> (p);
                return r != nullptr ? r->convertTo0to1 (r->convertFrom0to1 (p->getValue())) : p->getValue();
            };
            int mismatches = 0;
            for (int i = 0; i < a.getParameters().size(); ++i)
                if (std::abs (asUsed (a.getParameters()[i]) - asUsed (b.getParameters()[i])) > 1.0e-5f)
                {
                    ++mismatches;
                    logMessage ("  differs after restore: " + a.getParameters()[i]->getName (40) + " "
                                + juce::String (a.getParameters()[i]->getValue(), 5) + " vs " + juce::String (b.getParameters()[i]->getValue(), 5));
                }
            expectEquals (mismatches, 0);
        }

        beginTest ("Worst-case CPU stays low (everything on, 96 kHz, 64-sample buffers)");
        for (int mode : { 0, 1 })
        {
            RoBoxAudioProcessor p;
            set (p, ParamID::mode, (float) mode);
            set (p, ParamID::bands, 5.0f);       // 32 bands
            set (p, ParamID::waveform, 3.0f);    // Supersaw
            set (p, ParamID::chord, 3.0f);       // Oct + 5th
            set (p, ParamID::noise, 50.0f);
            set (p, ParamID::drive, 50.0f);
            set (p, ParamID::crushBits, 10.0f);
            set (p, ParamID::crushRate, 16000.0f);
            set (p, ParamID::eqLowGain, 6.0f);
            set (p, ParamID::eqMidGain, -6.0f);
            set (p, ParamID::eqHighType, 1.0f);
            constexpr double rate = 96000.0;
            const auto voice = TestSignals::vowel ((int) rate * 4, rate);
            run (p, std::vector<float> (voice.begin(), voice.begin() + (std::ptrdiff_t) rate), rate, 64); // warm up
            const auto t0 = juce::Time::getHighResolutionTicks();
            run (p, voice, rate, 64);
            const double seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            const double percent = 100.0 * seconds / 4.0;
            logMessage ("  " + juce::String (mode == 0 ? "Vocoder" : "Talkbox") + " worst case: " + juce::String (percent, 1) + " % of one core");
            expect (percent < 15.0, "too slow: " + juce::String (percent, 1) + " %");
        }
    }
};

static PluginTests pluginTests;

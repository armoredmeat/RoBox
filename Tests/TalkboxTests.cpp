// Tests for TalkboxEngine (LPC).
#include <juce_core/juce_core.h>
#include "dsp/TalkboxEngine.h"
#include "dsp/CarrierOsc.h"
#include "TestSignals.h"

using TestSignals::rms;
using TestSignals::vowel;

namespace
{
    float bandLevel (const std::vector<float>& x, double hz, double rate)
    {
        Biquad a, b;
        a.setBandpass (rate, hz, 6.0);
        b.setBandpass (rate, hz, 6.0);
        std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
        return rms (y, y.size() / 2);
    }

    std::vector<float> whiteNoise (int n, juce::int64 seed, float amp = 0.3f)
    {
        juce::Random r (seed);
        std::vector<float> s ((size_t) n);
        for (auto& v : s) v = (r.nextFloat() * 2.0f - 1.0f) * amp;
        return s;
    }

    std::vector<float> sawCarrier (int n, double rate)
    {
        CarrierOsc osc;
        osc.prepare (rate);
        osc.setSettings ({});
        std::vector<float> out ((size_t) n);
        osc.process (out.data(), n);
        return out;
    }

    std::vector<float> talk (const std::vector<float>& voice, const std::vector<float>& carrier,
                             TalkboxEngine::Settings s, double rate)
    {
        TalkboxEngine t;
        t.prepare (rate);
        t.setSettings (s);
        std::vector<float> out (voice.size());
        constexpr int block = 480;
        for (size_t i = 0; i < voice.size(); i += block)
        {
            const int n = (int) juce::jmin<size_t> (block, voice.size() - i);
            t.process (voice.data() + i, carrier.data() + i, out.data() + i, n);
        }
        return out;
    }
}

class TalkboxTests final : public juce::UnitTest
{
public:
    TalkboxTests() : juce::UnitTest ("TalkboxEngine", "RoBox") {}

    void runTest() override
    {
        constexpr double rate = 48000.0;
        constexpr int n = 48000;
        TalkboxEngine::Settings noSib;
        noSib.sibilance = 0.0f;

        beginTest ("Analysis recovers a known filter (Levinson and lattice agree)");
        {
            const std::array<float, 4> kTrue { 0.6f, -0.4f, 0.25f, -0.1f };
            TalkboxEngine::Lattice lattice;
            auto x = whiteNoise (96000, 7);
            for (auto& v : x) v = lattice.process (v, kTrue.data(), 4);

            std::array<double, 5> R {};
            for (int lag = 0; lag <= 4; ++lag)
                for (size_t i = (size_t) lag; i < x.size(); ++i)
                    R[(size_t) lag] += (double) x[i] * x[i - (size_t) lag];

            std::array<double, 4> k {};
            expect (TalkboxEngine::levinson (R.data(), 4, k.data()));
            for (size_t i = 0; i < 4; ++i)
                expectWithinAbsoluteError ((float) k[i], kTrue[i], 0.03f);
        }

        beginTest ("Filter order follows sample rate");
        for (double r : { 44100.0, 48000.0, 96000.0 })
        {
            TalkboxEngine t;
            t.prepare (r);
            logMessage ("  " + juce::String (r, 0) + " Hz: order " + juce::String (t.getOrder())
                        + ", frame " + juce::String (t.getFrameSize()) + ", hop " + juce::String (t.getHopSize()));
        }
        {
            TalkboxEngine t;
            t.prepare (48000.0);
            expectEquals (t.getOrder(), 24);
        }

        beginTest ("Silent voice gives silent output");
        {
            const auto out = talk (std::vector<float> ((size_t) n, 0.0f), sawCarrier (n, rate), {}, rate);
            expect (rms (out) < 1.0e-6f, "not silent: " + juce::String (rms (out)));
        }

        beginTest ("The carrier takes on the vowel's formants");
        {
            const auto out = talk (vowel (n, rate), whiteNoise (n, 3), noSib, rate);
            const float f1 = bandLevel (out, 700.0, rate), f2 = bandLevel (out, 1200.0, rate), off = bandLevel (out, 3000.0, rate);
            logMessage ("  700 Hz " + juce::String (f1, 4) + ", 1200 Hz " + juce::String (f2, 4) + ", 3000 Hz " + juce::String (off, 4));
            expect (f1 > off * 5.0f, "first formant missing");
            expect (f2 > off * 3.0f, "second formant missing");
        }

        beginTest ("Formant shift +12 moves the vowel up an octave");
        {
            auto s = noSib;
            s.formantSemitones = 12.0f;
            const auto out = talk (vowel (n, rate), whiteNoise (n, 3), s, rate);
            const float at700 = bandLevel (out, 700.0, rate), at1400 = bandLevel (out, 1400.0, rate);
            logMessage ("  700 Hz " + juce::String (at700, 4) + ", 1400 Hz " + juce::String (at1400, 4));
            expect (at1400 > at700 * 2.0f, "formants did not move up");
        }

        beginTest ("Output loudness matches the voice (-20 dBFS in)");
        {
            const auto out = talk (vowel (n, rate), sawCarrier (n, rate), noSib, rate);
            const float changeDb = juce::Decibels::gainToDecibels (rms (out, (size_t) n / 2) / 0.1f);
            logMessage ("  level change " + juce::String (changeDb, 1) + " dB");
            expect (std::abs (changeDb) < 3.0f, "level off by " + juce::String (changeDb) + " dB");
        }

        beginTest ("Stays stable at every sample rate and formant extreme");
        for (double r : { 44100.0, 48000.0, 96000.0 })
            for (float formant : { -12.0f, 0.0f, 12.0f })
            {
                const int len = (int) (r * 2.0);
                auto voice = vowel (len, r, 400.0, 2200.0);
                const auto burst = whiteNoise (len, 11, 0.8f);
                for (int i = len / 2; i < len * 3 / 4; ++i) voice[(size_t) i] = burst[(size_t) i]; // harsh noise section
                for (int i = len * 3 / 4; i < len; ++i) voice[(size_t) i] = 0.0f;                  // then silence

                TalkboxEngine::Settings s;
                s.formantSemitones = formant;
                const auto out = talk (voice, sawCarrier (len, r), s, r);
                bool finite = true;
                float peak = 0.0f;
                for (auto v : out) { finite = finite && std::isfinite (v); peak = juce::jmax (peak, std::abs (v)); }
                expect (finite, "non-finite at " + juce::String (r) + " Hz, formant " + juce::String (formant));
                expect (peak < 8.0f, "peak " + juce::String (peak) + " at " + juce::String (r) + " Hz, formant " + juce::String (formant));
            }
    }
};

static TalkboxTests talkboxTests;

#include "dsp/VocoderEngine.h"

// Rough CPU check: how much of one core each engine needs to keep up in real time.
class CpuTests final : public juce::UnitTest
{
public:
    CpuTests() : juce::UnitTest ("CPU", "RoBox") {}

    void runTest() override
    {
        beginTest ("Worst-case settings use well under 10 % of one core");
        for (double rate : { 48000.0, 96000.0 })
        {
            const int seconds = 5, n = (int) rate * seconds, block = 128;
            const auto voice = vowel (n, rate);

            // Heaviest carrier: supersaw with a 3-note chord = 21 oscillators.
            CarrierOsc osc;
            osc.prepare (rate);
            CarrierOsc::Settings cs;
            cs.waveform = CarrierOsc::Supersaw;
            cs.chord = CarrierOsc::OctFifth;
            osc.setSettings (cs);

            VocoderEngine voc;
            voc.prepare (rate);
            VocoderEngine::Settings vs;
            vs.numBands = 32;
            voc.setSettings (vs);

            TalkboxEngine tb;
            tb.prepare (rate);

            std::vector<float> car ((size_t) block), out ((size_t) block);
            double vocoderSecs = 0, talkboxSecs = 0, carrierSecs = 0;

            for (int i = 0; i + block <= n; i += block)
            {
                auto t0 = juce::Time::getHighResolutionTicks();
                osc.process (car.data(), block);
                auto t1 = juce::Time::getHighResolutionTicks();
                voc.process (voice.data() + i, car.data(), out.data(), block);
                auto t2 = juce::Time::getHighResolutionTicks();
                tb.process (voice.data() + i, car.data(), out.data(), block);
                auto t3 = juce::Time::getHighResolutionTicks();
                carrierSecs += juce::Time::highResolutionTicksToSeconds (t1 - t0);
                vocoderSecs += juce::Time::highResolutionTicksToSeconds (t2 - t1);
                talkboxSecs += juce::Time::highResolutionTicksToSeconds (t3 - t2);
            }

            auto pct = [&] (double s) { return 100.0 * s / seconds; };
            logMessage ("  " + juce::String (rate, 0) + " Hz: carrier " + juce::String (pct (carrierSecs), 2)
                        + " %, vocoder (32 bands) " + juce::String (pct (vocoderSecs), 2)
                        + " %, talkbox " + juce::String (pct (talkboxSecs), 2) + " % of one core");
            expect (pct (carrierSecs + vocoderSecs) < 10.0, "vocoder mode too slow");
            expect (pct (carrierSecs + talkboxSecs) < 10.0, "talkbox mode too slow");
        }
    }
};

static CpuTests cpuTests;

// Switching Mode at default settings shouldn't jump in volume.
class ModeLevelTests final : public juce::UnitTest
{
public:
    ModeLevelTests() : juce::UnitTest ("Mode levels", "RoBox") {}

    void runTest() override
    {
        beginTest ("Vocoder and Talkbox are equally loud at default settings");
        constexpr double rate = 48000.0;
        constexpr int n = 48000;
        double vocoderSum = 0, talkboxSum = 0;

        for (const auto& v : TestSignals::vowels)
        {
            const auto voice = vowel (n, rate, v.f1, v.f2);
            const auto car = sawCarrier (n, rate);

            VocoderEngine voc;
            voc.prepare (rate);
            voc.setSettings ({});
            std::vector<float> vocOut ((size_t) n);
            for (int i = 0; i < n; i += 480)
                voc.process (voice.data() + i, car.data() + i, vocOut.data() + i, juce::jmin (480, n - i));

            TalkboxEngine::Settings ts;
            ts.tube = 0.6f; // the plugin's default Tube setting
            const auto tbOut = talk (voice, car, ts, rate);

            const float vocDb = juce::Decibels::gainToDecibels (rms (vocOut, (size_t) n / 2));
            const float tbDb  = juce::Decibels::gainToDecibels (rms (tbOut,  (size_t) n / 2));
            vocoderSum += vocDb;
            talkboxSum += tbDb;
            logMessage (juce::String ("  ") + v.name + ": vocoder " + juce::String (vocDb, 1) + " dB, talkbox " + juce::String (tbDb, 1) + " dB");
        }

        const double diff = (vocoderSum - talkboxSum) / (double) TestSignals::vowels.size();
        logMessage ("  average difference (vocoder - talkbox): " + juce::String (diff, 1) + " dB");
        // The -14 dB vocoder calibration came from listening to real vocals; on these synthetic
        // vowels it measures ~2.4 dB under the talkbox. 3 dB catches real regressions
        // (the old calibration was ~12 dB off) without overriding the by-ear setting.
        expect (std::abs (diff) < 3.0, "modes differ by " + juce::String (diff, 1) + " dB on average");
    }
};

static ModeLevelTests modeLevelTests;

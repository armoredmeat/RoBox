// Tests for TubeVoice (guitar-talkbox voicing) and its effect inside the talkbox.
#include <juce_core/juce_core.h>
#include "dsp/TubeVoice.h"
#include "dsp/TalkboxEngine.h"
#include "dsp/CarrierOsc.h"
#include "TestSignals.h"

namespace
{
    constexpr double rate = 48000.0;

    // Exact level of one frequency (the signal must contain a whole number of its cycles).
    float dftLevel (const std::vector<float>& x, double hz)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double ph = juce::MathConstants<double>::twoPi * hz * (double) i / rate;
            re += x[i] * std::cos (ph);
            im += x[i] * std::sin (ph);
        }
        return (float) (2.0 * std::sqrt (re * re + im * im) / (double) x.size());
    }

    float bandLevel (const std::vector<float>& x, double hz)
    {
        Biquad a, b;
        a.setBandpass (rate, hz, 4.0);
        b.setBandpass (rate, hz, 4.0);
        double sum = 0;
        for (size_t i = 0; i < x.size(); ++i) { const float y = b.process (a.process (x[i])); if (i > x.size() / 2) sum += y * y; }
        return (float) std::sqrt (sum / (double) (x.size() / 2));
    }

    std::vector<float> voiced (std::vector<float> x, float amount)
    {
        TubeVoice t;
        t.prepare (rate);
        t.setAmount (amount);
        for (auto& v : x) v = t.process (v);
        return x;
    }

    float db (float x) { return juce::Decibels::gainToDecibels (x, -200.0f); }
}

class TubeTests final : public juce::UnitTest
{
public:
    TubeTests() : juce::UnitTest ("TubeVoice", "RoBox") {}

    void runTest() override
    {
        constexpr int n = 48000; // 1 s: a whole number of cycles of 220 Hz

        std::vector<float> sine ((size_t) n);
        for (int i = 0; i < n; ++i) sine[(size_t) i] = 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / rate);

        beginTest ("Tube 0 % leaves the carrier untouched");
        {
            const auto out = voiced (sine, 0.0f);
            bool same = true;
            for (size_t i = 0; i < sine.size(); ++i) same = same && out[i] == sine[i];
            expect (same, "signal changed at 0 %");
        }

        beginTest ("Tube adds warm (even) and gritty (odd) harmonics");
        for (float amount : { 0.3f, 0.6f, 1.0f })
        {
            const auto out = voiced (sine, amount);
            const float f = dftLevel (out, 220.0);
            const float h2 = db (dftLevel (out, 440.0) / f), h3 = db (dftLevel (out, 660.0) / f);
            logMessage ("  tube " + juce::String (amount * 100.0f, 0) + " %: 2nd harmonic " + juce::String (h2, 1)
                        + " dB, 3rd " + juce::String (h3, 1) + " dB (relative to the note)");
            expect (h2 > -40.0f, "too little even harmonic");
            expect (h3 > -30.0f, "too little odd harmonic");
        }

        beginTest ("Full Tube band-limits like a horn and tube, with a mid honk");
        {
            juce::Random r (5);
            std::vector<float> noise ((size_t) n);
            for (auto& v : noise) v = (r.nextFloat() * 2.0f - 1.0f) * 0.01f; // quiet, so the clipper stays nearly linear
            const auto out = voiced (noise, 1.0f);
            const float lows = bandLevel (out, 50.0), mids = bandLevel (out, 1200.0), highs = bandLevel (out, 10000.0);
            logMessage ("  50 Hz " + juce::String (db (lows / mids), 1) + " dB, 10 kHz " + juce::String (db (highs / mids), 1) + " dB (relative to 1.2 kHz)");
            expect (lows  < mids * 0.18f, "lows not cut enough");   // > 15 dB down
            expect (highs < mids * 0.18f, "highs not cut enough");
        }

        beginTest ("Talkbox loudness still matches the voice with Tube up");
        for (float amount : { 0.0f, 0.6f, 1.0f })
        {
            const auto voice = TestSignals::vowel (n, rate);
            CarrierOsc osc;
            osc.prepare (rate);
            CarrierOsc::Settings cs;
            cs.chord = CarrierOsc::Power; // the case where clipping does the most
            osc.setSettings (cs);
            std::vector<float> car ((size_t) n);
            osc.process (car.data(), n);

            TalkboxEngine tb;
            tb.prepare (rate);
            TalkboxEngine::Settings s;
            s.sibilance = 0.0f;
            s.tube = amount;
            tb.setSettings (s);
            std::vector<float> out ((size_t) n);
            for (int i = 0; i < n; i += 480)
                tb.process (voice.data() + i, car.data() + i, out.data() + i, juce::jmin (480, n - i));

            bool finite = true;
            for (auto v : out) finite = finite && std::isfinite (v);
            const float change = db (TestSignals::rms (out, (size_t) n / 2) / 0.1f);
            logMessage ("  tube " + juce::String (amount * 100.0f, 0) + " %: talkbox level change " + juce::String (change, 1) + " dB");
            expect (finite, "non-finite output");
            expect (std::abs (change) < 3.0f, "level off by " + juce::String (change, 1) + " dB");
        }
    }
};

static TubeTests tubeTests;

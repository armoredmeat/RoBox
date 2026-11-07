// Tests for the 3-band EQ.
#include <juce_core/juce_core.h>
#include "dsp/Equalizer.h"

namespace
{
    constexpr double rate = 48000.0;

    std::vector<float> sine (double hz, int n)
    {
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = 0.25f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
        return s;
    }

    float rms (const std::vector<float>& x, size_t from)
    {
        double sum = 0;
        for (size_t i = from; i < x.size(); ++i) sum += (double) x[i] * x[i];
        return (float) std::sqrt (sum / (double) (x.size() - from));
    }

    // Measured gain in dB of a sine through the EQ (skipping the first 0.25 s while it settles).
    float measureDb (const Equalizer::Settings& s, double hz)
    {
        Equalizer eq;
        eq.setSettings (s);
        eq.prepare (rate);
        auto x = sine (hz, 24000);
        const auto in = x;
        eq.process (x.data(), (int) x.size());
        return juce::Decibels::gainToDecibels (rms (x, 12000) / rms (in, 12000));
    }
}

class EqualizerTests final : public juce::UnitTest
{
public:
    EqualizerTests() : juce::UnitTest ("Equalizer", "RoBox") {}

    void runTest() override
    {
        beginTest ("Flat EQ leaves the signal unchanged");
        {
            Equalizer eq;
            eq.prepare (rate);
            auto x = sine (440.0, 4800);
            const auto in = x;
            eq.process (x.data(), (int) x.size());
            float maxDiff = 0.0f;
            for (size_t i = 0; i < x.size(); ++i) maxDiff = juce::jmax (maxDiff, std::abs (x[i] - in[i]));
            expect (maxDiff < 1.0e-5f, "flat EQ changed the signal by " + juce::String (maxDiff));
        }

        beginTest ("Each band boosts where it should, and the graph matches what you hear");
        {
            struct Case { int band; float freq, gainDb; double testHz; const char* name; };
            for (auto c : { Case { Equalizer::lowShelf,  150.0f,  9.0f,    40.0, "low shelf +9 dB, at 40 Hz" },
                            Case { Equalizer::mid,      1000.0f, -9.0f,  1000.0, "mid -9 dB, at 1 kHz" },
                            Case { Equalizer::highShelf, 5000.0f, 6.0f, 15000.0, "high shelf +6 dB, at 15 kHz" } })
            {
                Equalizer::Settings s;
                s.bands[(size_t) c.band].freq = c.freq;
                s.bands[(size_t) c.band].gainDb = c.gainDb;
                const float measured = measureDb (s, c.testHz);
                const float drawn = (float) Equalizer::responseDb (s, c.testHz, rate);
                logMessage ("  " + juce::String (c.name) + ": measured " + juce::String (measured, 2) + " dB, graph " + juce::String (drawn, 2) + " dB");
                expect (std::abs (measured - c.gainDb) < 1.0f, juce::String (c.name) + " measured " + juce::String (measured));
                expect (std::abs (measured - drawn) < 0.2f, juce::String (c.name) + " graph doesn't match the sound");
            }
        }

        beginTest ("Low Cut and High Cut remove 12 dB per octave beyond the cutoff");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf]  = { 200.0f, 0.71f, 0.0f, true };
            s.bands[Equalizer::highShelf] = { 3000.0f, 0.71f, 0.0f, true };
            struct Probe { double hz; float expectedDb, tolerance; const char* name; };
            for (auto p : { Probe {   50.0, -24.0f, 1.5f, "50 Hz (2 octaves below a 200 Hz Low Cut)" },
                            Probe {  200.0,  -3.0f, 0.5f, "200 Hz (at the Low Cut)" },
                            Probe {  800.0,   0.0f, 0.5f, "800 Hz (between the cuts)" },
                            Probe { 3000.0,  -3.0f, 0.5f, "3 kHz (at the High Cut)" },
                            Probe { 12000.0, -27.0f, 3.0f, "12 kHz (2 octaves above a 3 kHz High Cut; digital low-pass filters fall a bit faster near the top)" } })
            {
                const float measured = measureDb (s, p.hz);
                const float drawn = (float) Equalizer::responseDb (s, p.hz, rate);
                logMessage ("  " + juce::String (p.name) + ": measured " + juce::String (measured, 2) + " dB, graph " + juce::String (drawn, 2) + " dB");
                expect (std::abs (measured - p.expectedDb) < p.tolerance, juce::String (p.name) + " measured " + juce::String (measured));
                expect (std::abs (measured - drawn) < 0.3f, juce::String (p.name) + ": graph doesn't match the sound");
            }
        }

        beginTest ("Cuts work across the full range (a Low Cut high up, a High Cut low down)");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf] = { 8000.0f, 0.71f, 0.0f, true };   // Low Cut at 8 kHz
            const float lowCutAt1k = measureDb (s, 1000.0);
            s = {};
            s.bands[Equalizer::highShelf] = { 120.0f, 0.71f, 0.0f, true };   // High Cut at 120 Hz
            const float highCutAt1k = measureDb (s, 1000.0);
            logMessage ("  Low Cut at 8 kHz: 1 kHz down " + juce::String (-lowCutAt1k, 1) + " dB; High Cut at 120 Hz: 1 kHz down "
                        + juce::String (-highCutAt1k, 1) + " dB");
            expect (lowCutAt1k < -30.0f, "Low Cut at 8 kHz didn't remove 1 kHz: " + juce::String (lowCutAt1k));
            expect (highCutAt1k < -30.0f, "High Cut at 120 Hz didn't remove 1 kHz: " + juce::String (highCutAt1k));
        }

        beginTest ("Cuts at the very ends (20 Hz, 20 kHz) stay stable at 44.1 kHz");
        for (auto [band, hz] : { std::pair { (int) Equalizer::lowShelf, 20000.0f }, std::pair { (int) Equalizer::lowShelf, 20.0f },
                                 std::pair { (int) Equalizer::highShelf, 20.0f }, std::pair { (int) Equalizer::highShelf, 20000.0f } })
        {
            Equalizer::Settings s;
            s.bands[(size_t) band] = { hz, 2.0f, 0.0f, true };
            Equalizer eq;
            eq.setSettings (s);
            eq.prepare (44100.0);
            juce::Random rng (3);
            std::vector<float> x (88200);
            for (auto& v : x) v = rng.nextFloat() - 0.5f;
            eq.process (x.data(), (int) x.size());
            float peak = 0.0f;
            bool finite = true;
            for (auto v : x) { finite = finite && std::isfinite (v); peak = juce::jmax (peak, std::abs (v)); }
            expect (finite && peak < 10.0f, (band == Equalizer::lowShelf ? "Low" : "High") + juce::String (" Cut at ") + juce::String (hz) + " Hz unstable");
        }

        beginTest ("A cut ignores the Level knob");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf] = { 100.0f, 0.71f, 15.0f, true };
            expect (std::abs (measureDb (s, 2000.0)) < 0.3f, "Level changed a Low Cut's passband");
        }

        beginTest ("Switching Shelf to Cut fades instead of jumping (no click)");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf] = { 200.0f, 0.71f, 12.0f, false };
            Equalizer eq;
            eq.setSettings (s);
            eq.prepare (rate);
            auto x = sine (60.0, 9600);
            eq.process (x.data(), 4800);
            s.bands[Equalizer::lowShelf].cut = true; // 60 Hz drops by ~20 dB
            eq.setSettings (s);
            eq.process (x.data() + 4800, 4800);
            float maxStep = 0.0f;
            for (size_t i = 4700; i < 6000; ++i) maxStep = juce::jmax (maxStep, std::abs (x[i + 1] - x[i]));
            // The boosted 60 Hz sine moves ~0.01 per sample; an instant switch would jump much more.
            logMessage ("  largest sample-to-sample step around the switch: " + juce::String (maxStep, 4));
            expect (maxStep < 0.05f, "jumped when switching to Cut: " + juce::String (maxStep));
        }

        beginTest ("The Mid band can't be switched to a cut");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::mid].cut = true;
            expect (std::abs (measureDb (s, 200.0)) < 0.1f, "Mid band turned into a filter");
        }

        beginTest ("A band leaves frequencies far away alone");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf].gainDb = 12.0f;
            s.bands[Equalizer::lowShelf].freq = 100.0f;
            expect (std::abs (measureDb (s, 5000.0)) < 0.3f, "low shelf leaked into 5 kHz");
        }

        beginTest ("Off = untouched, even with big settings");
        {
            Equalizer::Settings s;
            s.enabled = false;
            for (auto& b : s.bands) b.gainDb = 15.0f;
            Equalizer eq;
            eq.setSettings (s);
            eq.prepare (rate);
            auto x = sine (100.0, 4800);
            const auto in = x;
            eq.process (x.data(), (int) x.size());
            expect (x == in, "bypassed EQ changed the signal");
            expectEquals (Equalizer::responseDb (s, 100.0, rate), 0.0);
        }

        beginTest ("Extreme settings stay stable at 44.1 and 96 kHz (high shelf at 20 kHz, max Q)");
        for (double sr : { 44100.0, 96000.0 })
        {
            Equalizer::Settings s;
            s.bands[Equalizer::lowShelf]  = { 20.0f, 2.0f, 15.0f };
            s.bands[Equalizer::mid]       = { 10000.0f, 10.0f, -15.0f };
            s.bands[Equalizer::highShelf] = { 20000.0f, 2.0f, 15.0f };
            Equalizer eq;
            eq.setSettings (s);
            eq.prepare (sr);
            juce::Random rng (7);
            std::vector<float> x ((size_t) sr * 2);
            for (auto& v : x) v = rng.nextFloat() * 0.5f - 0.25f;
            eq.process (x.data(), (int) x.size());
            float peak = 0.0f;
            bool finite = true;
            for (auto v : x) { finite = finite && std::isfinite (v); peak = juce::jmax (peak, std::abs (v)); }
            expect (finite && peak < 20.0f, "unstable at " + juce::String (sr) + " Hz: peak " + juce::String (peak));
        }

        beginTest ("Switching off fades out instead of jumping (no click)");
        {
            Equalizer::Settings s;
            s.bands[Equalizer::mid].gainDb = 15.0f;
            Equalizer eq;
            eq.setSettings (s);
            eq.prepare (rate);
            auto x = sine (1000.0, 9600);
            eq.process (x.data(), 4800);
            s.enabled = false;
            eq.setSettings (s);
            eq.process (x.data() + 4800, 4800);
            float maxStep = 0.0f;
            for (size_t i = 4790; i < 4900; ++i) maxStep = juce::jmax (maxStep, std::abs (x[i + 1] - x[i]));
            // A 1 kHz sine at the boosted level moves at most ~0.15 per sample; a jump would be far bigger.
            expect (maxStep < 0.2f, "level jumped when switching off: " + juce::String (maxStep));
        }
    }
};

static EqualizerTests equalizerTests;

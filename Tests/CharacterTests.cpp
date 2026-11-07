// Tests for Character (drive + bit crush + sample-rate reduction).
#include <juce_core/juce_core.h>
#include <set>
#include "dsp/Character.h"

namespace
{
    constexpr double rate = 48000.0;

    std::vector<float> sine (double hz, float amp, int n)
    {
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
        return s;
    }

    float rms (const std::vector<float>& x)
    {
        double sum = 0;
        for (auto v : x) sum += (double) v * v;
        return (float) std::sqrt (sum / (double) x.size());
    }

    std::vector<float> run (std::vector<float> x, Character::Settings s)
    {
        Character c;
        c.prepare (rate);
        c.setSettings (s);
        c.reset(); // start already at the target drive, no ramp
        c.process (x.data(), (int) x.size());
        return x;
    }
}

class CharacterTests final : public juce::UnitTest
{
public:
    CharacterTests() : juce::UnitTest ("Character", "RoBox") {}

    void runTest() override
    {
        constexpr int n = 48000;

        beginTest ("All off = untouched signal");
        {
            const auto in = sine (220.0, 0.5f, n);
            const auto out = run (in, {});
            float maxDiff = 0.0f;
            for (size_t i = 0; i < in.size(); ++i) maxDiff = juce::jmax (maxDiff, std::abs (in[i] - out[i]));
            expect (maxDiff == 0.0f, "signal changed with everything off: " + juce::String (maxDiff));
        }

        beginTest ("Drive keeps loudness steady at the working level (-20 dBFS RMS)");
        for (float drive : { 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto in = sine (220.0, 0.14f, n); // 0.14 peak = -20 dBFS RMS
            Character::Settings s;
            s.drive = drive;
            const float changeDb = juce::Decibels::gainToDecibels (rms (run (in, s)) / rms (in));
            logMessage ("  drive " + juce::String (drive * 100.0f, 0) + " %: level change " + juce::String (changeDb, 1) + " dB");
            expect (std::abs (changeDb) < 3.0f, "drive " + juce::String (drive) + " changed level by " + juce::String (changeDb) + " dB");
        }

        beginTest ("Full drive never exceeds full scale, whatever goes in");
        for (float amp : { 0.05f, 0.5f, 1.0f, 4.0f })
        {
            Character::Settings s;
            s.drive = 1.0f;
            const auto in = sine (220.0, amp, n);
            const auto out = run (in, s);
            float peak = 0.0f;
            for (auto v : out) peak = juce::jmax (peak, std::abs (v));
            logMessage ("  input peak " + juce::String (amp) + ": level change "
                        + juce::String (juce::Decibels::gainToDecibels (rms (out) / rms (in)), 1) + " dB (compression is expected)");
            expect (peak <= 1.0f, "output above full scale: " + juce::String (peak));
        }

        beginTest ("4 bits leaves at most 17 distinct levels");
        {
            Character::Settings s;
            s.bits = 4.0f;
            const auto out = run (sine (220.0, 0.9f, n), s);
            std::set<float> levels (out.begin(), out.end());
            expect (levels.size() <= 17, "found " + juce::String ((int) levels.size()) + " levels");
        }

        beginTest ("Rate 4 kHz takes ~4000 new samples per second");
        {
            Character::Settings s;
            s.rateHz = 4000.0f;
            const auto out = run (sine (1234.0, 0.9f, n), s);
            int changes = 0;
            for (size_t i = 1; i < out.size(); ++i) if (out[i] != out[i - 1]) ++changes;
            expect (std::abs (changes - 4000) <= 40, "value changed " + juce::String (changes) + " times");
        }
    }
};

static CharacterTests characterTests;

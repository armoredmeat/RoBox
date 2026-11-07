// Tests for VocoderEngine.
#include <juce_core/juce_core.h>
#include "dsp/VocoderEngine.h"
#include "dsp/CarrierOsc.h"
#include "TestSignals.h"

namespace
{
    constexpr double rate = 48000.0;

    float rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
    {
        if (to == 0) to = x.size();
        double sum = 0;
        for (size_t i = from; i < to; ++i) sum += (double) x[i] * x[i];
        return (float) std::sqrt (sum / (double) juce::jmax<size_t> (1, to - from));
    }

    // Level of a signal in a narrow band around hz (band-pass, then RMS of the second half).
    float bandLevel (const std::vector<float>& x, double hz)
    {
        Biquad a, b;
        a.setBandpass (rate, hz, 6.0);
        b.setBandpass (rate, hz, 6.0);
        std::vector<float> y (x.size());
        for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (a.process (x[i]));
        return rms (y, y.size() / 2);
    }

    std::vector<float> sine (double hz, float amp, int n)
    {
        std::vector<float> s ((size_t) n);
        for (int i = 0; i < n; ++i) s[(size_t) i] = amp * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / rate);
        return s;
    }

    std::vector<float> noise (float rmsLevel, int n, juce::int64 seed = 1)
    {
        juce::Random r (seed);
        std::vector<float> s ((size_t) n);
        const float amp = rmsLevel * std::sqrt (3.0f); // uniform noise: rms = amp / sqrt(3)
        for (auto& v : s) v = (r.nextFloat() * 2.0f - 1.0f) * amp;
        return s;
    }

    std::vector<float> sawCarrier (int n)
    {
        CarrierOsc osc;
        osc.prepare (rate);
        CarrierOsc::Settings s; // saw, C2
        osc.setSettings (s);
        std::vector<float> out ((size_t) n);
        osc.process (out.data(), n);
        return out;
    }

    std::vector<float> vocode (const std::vector<float>& mod, const std::vector<float>& car, VocoderEngine::Settings s)
    {
        VocoderEngine v;
        v.prepare (rate);
        v.setSettings (s);
        std::vector<float> out (mod.size());
        constexpr int block = 512;
        for (size_t i = 0; i < mod.size(); i += block)
        {
            const int n = (int) juce::jmin<size_t> (block, mod.size() - i);
            v.process (mod.data() + i, car.data() + i, out.data() + i, n);
        }
        return out;
    }
}

class VocoderTests final : public juce::UnitTest
{
public:
    VocoderTests() : juce::UnitTest ("VocoderEngine", "RoBox") {}

    void runTest() override
    {
        constexpr int n = 48000; // 1 second

        beginTest ("Silent voice gives silent output");
        {
            const auto out = vocode (std::vector<float> (n, 0.0f), sawCarrier (n), {});
            expect (rms (out) < 1.0e-6f, "output not silent: " + juce::String (rms (out)));
        }

        beginTest ("Output appears where the voice has energy");
        {
            VocoderEngine::Settings s;
            s.sibilance = 0.0f;
            const auto out = vocode (sine (500.0, 0.3f, n), noise (0.3f, n), s);
            const float at500 = bandLevel (out, 500.0), at4k = bandLevel (out, 4000.0);
            logMessage ("  500 Hz: " + juce::String (at500) + "  4 kHz: " + juce::String (at4k));
            expect (at500 > at4k * 10.0f, "energy not concentrated at the voice frequency");
        }

        beginTest ("Formant shift +12 moves the voice character up an octave");
        {
            VocoderEngine::Settings s;
            s.sibilance = 0.0f;
            s.formantSemitones = 12.0f;
            const auto out = vocode (sine (500.0, 0.3f, n), noise (0.3f, n), s);
            const float at500 = bandLevel (out, 500.0), at1k = bandLevel (out, 1000.0);
            logMessage ("  500 Hz: " + juce::String (at500) + "  1 kHz: " + juce::String (at1k));
            expect (at1k > at500 * 2.0f, "formant shift did not move energy up");
        }

        beginTest ("Release: output fades after the voice stops");
        {
            auto mod = noise (0.1f, n);
            std::fill (mod.begin() + 9600, mod.end(), 0.0f); // 0.2 s of sound, then silence
            VocoderEngine::Settings s;
            s.releaseMs = 60.0f;
            const auto out = vocode (mod, sawCarrier (n), s);
            const float during = rms (out, 4800, 9600);
            const float after  = rms (out, 9600 + 28800, 9600 + 38400); // 0.6 - 0.8 s after stopping
            expect (after < during * 0.01f, "did not release: " + juce::String (after / during));
        }

        beginTest ("Speech-like input at -20 dBFS comes out near -20 dBFS at every band count");
        for (int bands : { 8, 12, 16, 20, 24, 32 })
        {
            VocoderEngine::Settings s;
            s.numBands = bands;
            s.sibilance = 0.0f;
            juce::String line = "  " + juce::String (bands) + " bands:";

            for (const auto& v : TestSignals::vowels)
            {
                const auto out = vocode (TestSignals::vowel (n, rate, v.f1, v.f2), sawCarrier (n), s);
                const float db = juce::Decibels::gainToDecibels (rms (out, (size_t) n / 2));
                line << " " << v.name << " " << juce::String (db, 1) << " dB";
                // Vowels naturally differ by a few dB in a vocoder (it follows the voice's spectrum,
                // it doesn't level it like the talkbox), so allow +/- 5 dB per vowel.
                expect (std::abs (db + 20.0f) < 5.0f, juce::String (bands) + " bands, " + v.name + ": " + juce::String (db, 1) + " dBFS");
            }

            // White noise spreads evenly over all bands, so it comes out quieter; just check it's sane.
            const auto noiseOut = vocode (noise (0.1f, n), sawCarrier (n), s);
            bool finite = true;
            for (auto x : noiseOut) finite = finite && std::isfinite (x);
            line << " | noise " << juce::String (juce::Decibels::gainToDecibels (rms (noiseOut, (size_t) n / 2)), 1) << " dB";
            expect (finite, "non-finite output");
            logMessage (line);
        }
    }
};

static VocoderTests vocoderTests;

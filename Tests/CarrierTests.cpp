// Tests for CarrierOsc (the built-in synth).
#include <juce_core/juce_core.h>
#include <iostream>
#include "dsp/CarrierOsc.h"

namespace
{
    constexpr double testRate = 48000.0;

    std::vector<float> render (CarrierOsc::Settings s, int numSamples)
    {
        CarrierOsc osc;
        osc.prepare (testRate);
        osc.setSettings (s);
        std::vector<float> out ((size_t) numSamples);
        constexpr int block = 256;
        for (int i = 0; i < numSamples; i += block)
        {
            osc.setSettings (s);
            osc.process (out.data() + i, juce::jmin (block, numSamples - i));
        }
        return out;
    }

    // Pitch detector: finds the first lag where the signal matches a shifted copy of itself
    // (autocorrelation), refined with parabolic interpolation. Works for any waveform shape.
    float estimateHz (const std::vector<float>& x, float minHz = 40.0f, float maxHz = 2000.0f)
    {
        const int minLag = (int) (testRate / maxHz);
        const int maxLag = (int) (testRate / minHz);
        const int n = (int) x.size() - maxLag - 1;

        auto corr = [&] (int lag)
        {
            double xy = 0, xx = 0, yy = 0;
            for (int i = 0; i < n; ++i)
            {
                xy += x[(size_t) i] * x[(size_t) (i + lag)];
                xx += x[(size_t) i] * x[(size_t) i];
                yy += x[(size_t) (i + lag)] * x[(size_t) (i + lag)];
            }
            return xy / std::sqrt (xx * yy + 1e-12);
        };

        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const double c0 = corr (lag - 1), c1 = corr (lag), c2 = corr (lag + 1);
            if (c1 > 0.9 && c1 >= c0 && c1 >= c2)
            {
                const double denom = c0 - 2.0 * c1 + c2;
                const double offset = std::abs (denom) > 1e-12 ? 0.5 * (c0 - c2) / denom : 0.0;
                return (float) (testRate / (lag + offset));
            }
        }
        return 0.0f;
    }
}

class CarrierTests final : public juce::UnitTest
{
public:
    CarrierTests() : juce::UnitTest ("CarrierOsc", "RoBox") {}

    void runTest() override
    {
        beginTest ("Note/octave mapping");
        expectEquals (CarrierOsc::noteToMidi (0, 2), 36);  // C2
        expectEquals (CarrierOsc::noteToMidi (9, 4), 69);  // A4
        expectWithinAbsoluteError (CarrierOsc::midiToHz (69.0f), 440.0f, 0.01f);
        expectWithinAbsoluteError (CarrierOsc::midiToHz (60.0f), 261.63f, 0.01f);

        beginTest ("Pitch is correct for every waveform (C4 = 261.6 Hz)");
        for (int wave : { CarrierOsc::Saw, CarrierOsc::Square, CarrierOsc::Pulse, CarrierOsc::Supersaw })
        {
            CarrierOsc::Settings s;
            s.waveform = wave;
            s.detune = 0.0f;       // detuned unison would blur the zero crossings
            s.pulseWidth = 0.3f;
            s.midiNote = 60.0f;
            const auto out = render (s, 9600);
            const float hz = estimateHz (out);
            expect (std::abs (hz - 261.63f) < 2.0f, "waveform " + juce::String (wave) + " measured " + juce::String (hz) + " Hz");
        }

        beginTest ("Output stays finite and in range for every waveform x chord, with noise");
        for (int wave = 0; wave < 4; ++wave)
            for (int chord = 0; chord < 8; ++chord)
                for (float note : { 12.0f, 36.0f, 84.0f })
                {
                    CarrierOsc::Settings s;
                    s.waveform = wave;
                    s.chord = chord;
                    s.detune = 1.0f;
                    s.noise = 0.5f;
                    s.pulseWidth = 0.05f;
                    s.midiNote = note;
                    const auto out = render (s, 24000);
                    float peak = 0.0f;
                    bool finite = true;
                    for (auto v : out)
                    {
                        finite = finite && std::isfinite (v);
                        peak = juce::jmax (peak, std::abs (v));
                    }
                    expect (finite, "non-finite output");
                    expect (peak < 4.0f, "peak too high: " + juce::String (peak));
                    expect (peak > 0.05f, "silent output");
                }

        beginTest ("Pitch changes glide instead of jumping");
        {
            CarrierOsc osc;
            osc.prepare (testRate);
            CarrierOsc::Settings s;
            s.midiNote = 36.0f;
            osc.setSettings (s);
            std::vector<float> buf (4800);
            osc.process (buf.data(), (int) buf.size());
            s.midiNote = 48.0f; // up one octave
            osc.setSettings (s);
            // Let the 30 ms glide finish, then measure 1 s at the target: should be ~130.8 Hz.
            osc.process (buf.data(), 4800);
            std::vector<float> settled (9600);
            osc.process (settled.data(), (int) settled.size());
            const float hz = estimateHz (settled);
            expect (std::abs (hz - 130.81f) < 1.5f, "after glide measured " + juce::String (hz) + " Hz");
        }
    }
};

static CarrierTests carrierTests;

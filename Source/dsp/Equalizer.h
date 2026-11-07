#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Biquad.h"

// A small 3-band EQ for the wet signal: low shelf, mid bell, high shelf.
// The low and high bands can each be switched from a shelf to a cut (a 12 dB/octave
// high-pass or low-pass filter, with Q setting the resonance at the cutoff).
// Each band is one biquad. Knob moves are smoothed (about 30 ms) and the filters are
// redesigned every few samples while a knob is moving, so sweeps don't zipper or click.
// Switching the EQ on/off, or a band between Shelf and Cut, crossfades over 20 ms instead of jumping.
class Equalizer
{
public:
    enum Band { lowShelf = 0, mid, highShelf, numBands };

    struct BandSettings
    {
        float freq   = 1000.0f; // Hz
        float q      = 0.71f;
        float gainDb = 0.0f;   // ignored when cut is on
        bool  cut    = false;  // low/high band only: true = high-pass / low-pass instead of a shelf
    };

    struct Settings
    {
        bool enabled = true;
        std::array<BandSettings, numBands> bands { { { 120.0f, 0.71f, 0.0f },
                                                     { 1000.0f, 1.0f, 0.0f },
                                                     { 8000.0f, 0.71f, 0.0f } } };
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& newSettings);
    void process (float* data, int numSamples); // mono, in place

    // Builds the filter for one band. Shared by the audio code and the UI graph, so the curve
    // you see is exactly the filter you hear.
    static void design (Biquad& filter, int band, const BandSettings& s, double sampleRate) noexcept;

    // Combined boost/cut of all three bands at freq, in dB (0 when the EQ is off).
    static double responseDb (const Settings& s, double freq, double sampleRate) noexcept;

private:
    void updateFilters() noexcept;

    double sampleRate = 48000.0;
    Settings settings;
    std::array<Biquad, numBands> filters;
    std::array<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative>, numBands> freq, q;
    std::array<juce::SmoothedValue<float>, numBands> gain;
    juce::SmoothedValue<float> wetAmount { 1.0f }; // 1 = EQ on, 0 = bypassed

    // When a band changes type, the old filters keep running for a moment and fade out.
    std::array<Biquad, numBands> oldFilters;
    juce::SmoothedValue<float> oldAmount { 0.0f }; // 1 = all old filters, 0 = all new

    static constexpr int updateInterval = 16; // samples between filter redesigns while smoothing
};

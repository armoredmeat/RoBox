#pragma once

#include <array>
#include "Biquad.h"

// Classic channel vocoder.
//
//  1. Split the voice (modulator) into N frequency bands with band-pass filters.
//  2. An envelope follower measures how loud each voice band is right now.
//     Attack = how fast it reacts to getting louder, Release = how fast it falls back.
//  3. Split the carrier synth into the same N bands.
//  4. Turn each carrier band up/down by the matching voice band's loudness, and add them up.
//     The carrier now has the voice's changing tone colour, which is what makes it "talk".
//
// Formant shift reads the voice bands at a shifted frequency, which moves the vocal
// character up (smaller/chipmunk) or down (bigger/monster) without changing the carrier pitch.
// Sibilance adds back the voice's own high end (above ~5 kHz) so "s" and "t" stay clear.
class VocoderEngine
{
public:
    static constexpr int maxBands = 32;
    static constexpr float lowestBandHz  = 80.0f;
    static constexpr float highestBandHz = 12000.0f;

    struct Settings
    {
        int   numBands         = 16;
        float formantSemitones = 0.0f;  // -12 .. +12
        float attackMs         = 5.0f;
        float releaseMs        = 60.0f;
        float sibilance        = 0.3f;  // 0 .. 1
    };

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& newSettings);   // once per block, before process()

    // All three pointers are mono; out may not alias modulator or carrier.
    void process (const float* modulator, const float* carrier, float* out, int numSamples);

    // For the live display.
    int getNumBands() const noexcept { return settings.numBands; }
    float getBandLevel (int band) const noexcept { return envelopes[(size_t) band]; }

private:
    void updateFilters();

    double sampleRate = 48000.0;
    Settings settings;
    bool filtersDirty = true;

    // Each band = two band-pass filters in series for steeper sides (less overlap between bands).
    std::array<std::array<Biquad, 2>, maxBands> modFilters {}, carFilters {};
    std::array<float, maxBands> envelopes {};

    float attackCoef = 0.0f, releaseCoef = 0.0f;
    float makeupGain = 1.0f;

    std::array<Biquad, 2> sibilanceFilters {}; // high-pass for the voice's "s" sounds
};

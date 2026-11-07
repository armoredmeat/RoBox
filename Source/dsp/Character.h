#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Grit for the wet signal, in this order:
//  1. Drive: tanh saturation. Pushing a signal into tanh rounds off its peaks, which adds
//     warm-to-fuzzy harmonics. Level is compensated so turning it up doesn't just make it louder.
//  2. Bits: rounds every sample to fewer volume steps (16 bits = clean, 4 bits = very crunchy).
//  3. Rate: sample-and-hold. Only takes a new sample N times per second and repeats it
//     in between, giving the aliased, metallic "old sampler" sound.
class Character
{
public:
    struct Settings
    {
        float drive  = 0.0f;      // 0 .. 1
        float bits   = 16.0f;     // 4 .. 16 (16 = off)
        float rateHz = 48000.0f;  // 4000 .. 48000 (48000 = off)
    };

    static constexpr float bitsOffThreshold = 15.99f;
    static constexpr float rateOffThreshold = 47999.0f;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& newSettings);
    void process (float* data, int numSamples); // mono, in place

private:
    double sampleRate = 48000.0;
    Settings settings;
    juce::SmoothedValue<float> driveGain { 1.0f };

    float holdPhase = 1.0f;  // >= 1 means "take a new sample now"
    float heldValue = 0.0f;
};

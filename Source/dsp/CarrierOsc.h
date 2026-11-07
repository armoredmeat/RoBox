#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// The built-in synth that the voice "talks through".
//
// How it works:
//  - Each oscillator is a phase counter going 0 -> 1 once per cycle.
//    Saw = a ramp, Pulse/Square = high for part of the cycle, low for the rest.
//  - Sharp jumps in a digital waveform cause aliasing (harsh, inharmonic fizz).
//    PolyBLEP smooths just the few samples around each jump to remove most of it.
//  - Supersaw = 7 saws, each slightly detuned, for a wide, thick sound.
//  - Chord stack = extra oscillators at fixed intervals above/below the root note.
//  - Noise = white noise mixed in, which helps consonants like "s" and "t" come through.
class CarrierOsc
{
public:
    enum Waveform { Saw = 0, Square, Pulse, Supersaw };
    enum Chord { Off = 0, Octave, Fifth, OctFifth, Minor, Major, Power, Sus4 };

    struct Settings
    {
        int   waveform   = Saw;
        float pulseWidth = 0.5f;  // 0.05 .. 0.95 (Pulse only)
        float detune     = 0.3f;  // 0 .. 1 (Supersaw only)
        int   chord      = Off;
        float noise      = 0.0f;  // 0 .. 1
        float midiNote   = 36.0f; // root pitch, fractional allowed
    };

    static constexpr int maxChordNotes = 3;
    static constexpr int maxUnison     = 7;

    // Note index 0..11 (C..B) + octave -> MIDI note number. C2 = 36, A4 = 69.
    static int noteToMidi (int noteIndex, int octave) noexcept { return (octave + 1) * 12 + noteIndex; }
    static float midiToHz (float midiNote) noexcept { return 440.0f * std::exp2 ((midiNote - 69.0f) / 12.0f); }

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& newSettings); // call once per block, before process()
    void process (float* output, int numSamples);   // mono, overwrites output

private:
    void rebuildVoices();
    float renderVoices (float baseDt);

    double sampleRate = 48000.0;
    Settings settings;
    bool snapPitchOnNextSettings = true;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequency { 65.4f };

    // Pre-allocated so nothing is allocated on the audio thread.
    static constexpr int maxVoices = maxChordNotes * maxUnison;
    std::array<double, maxVoices> phases {};
    std::array<float, maxVoices>  ratios {}; // frequency multiplier per voice
    int   numVoices  = 1;
    float voiceGain  = 1.0f;
    float pulseDcOffset = 0.0f;

    juce::Random random { 0x524F424F }; // fixed seed: renders are repeatable
};

#include "CarrierOsc.h"

namespace
{
    // PolyBLEP correction for a jump of size 2 at phase 0. t = phase (0..1), dt = phase step per sample.
    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    // Semitone offsets for each chord option. Unused slots are ignored.
    struct ChordShape { int count; std::array<int, CarrierOsc::maxChordNotes> semitones; };
    constexpr std::array<ChordShape, 8> chordShapes {{
        { 1, { 0,   0,  0 } },  // Off
        { 2, { 0,  12,  0 } },  // Octave
        { 2, { 0,   7,  0 } },  // Fifth
        { 3, { 0,   7, 12 } },  // Oct+5th
        { 3, { 0,   3,  7 } },  // Minor
        { 3, { 0,   4,  7 } },  // Major
        { 3, { -12, 0,  7 } },  // Power (sub-octave + root + fifth)
        { 3, { 0,   5,  7 } },  // Sus4
    }};

    // Supersaw voice spread (-1..1), roughly like classic hardware: denser near the centre.
    constexpr std::array<float, CarrierOsc::maxUnison> unisonSpread { -1.0f, -0.62f, -0.3f, 0.0f, 0.3f, 0.62f, 1.0f };
    constexpr float maxDetuneSemitones = 0.6f; // at Detune = 100 %
}

void CarrierOsc::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    frequency.reset (sampleRate, 0.03); // 30 ms glide between pitches so changes don't click
    reset();
}

void CarrierOsc::reset()
{
    // Random start phases stop the supersaw voices from lining up into one loud spike.
    for (auto& p : phases)
        p = random.nextDouble();
    snapPitchOnNextSettings = true;
}

void CarrierOsc::setSettings (const Settings& newSettings)
{
    settings = newSettings;
    settings.pulseWidth = juce::jlimit (0.05f, 0.95f, settings.pulseWidth);
    settings.chord      = juce::jlimit (0, (int) chordShapes.size() - 1, settings.chord);

    const auto hz = juce::jlimit (8.0f, (float) sampleRate * 0.45f, midiToHz (settings.midiNote));
    if (snapPitchOnNextSettings)
    {
        frequency.setCurrentAndTargetValue (hz);
        snapPitchOnNextSettings = false;
    }
    else
    {
        frequency.setTargetValue (hz);
    }

    rebuildVoices();
}

void CarrierOsc::rebuildVoices()
{
    const auto& shape = chordShapes[(size_t) settings.chord];
    const int unison = settings.waveform == Supersaw ? maxUnison : 1;
    const float detuneSemis = settings.detune * maxDetuneSemitones;

    numVoices = 0;
    for (int c = 0; c < shape.count; ++c)
        for (int u = 0; u < unison; ++u)
        {
            const float semis = (float) shape.semitones[(size_t) c]
                              + (unison > 1 ? unisonSpread[(size_t) u] * detuneSemis : 0.0f);
            ratios[(size_t) numVoices++] = std::exp2 (semis / 12.0f);
        }

    // Keep loudness roughly constant whatever the voice count.
    voiceGain = 1.0f / std::sqrt ((float) numVoices);

    // A pulse that isn't 50 % has a DC offset (average not zero); remove it.
    pulseDcOffset = (settings.waveform == Pulse || settings.waveform == Square)
                        ? 2.0f * (settings.waveform == Square ? 0.5f : settings.pulseWidth) - 1.0f
                        : 0.0f;
}

float CarrierOsc::renderVoices (float baseDt)
{
    const float width = settings.waveform == Square ? 0.5f : settings.pulseWidth;
    const bool isPulse = settings.waveform == Square || settings.waveform == Pulse;
    float sum = 0.0f;

    for (int v = 0; v < numVoices; ++v)
    {
        const float dt = juce::jmin (baseDt * ratios[(size_t) v], 0.45f);
        const float t  = (float) phases[(size_t) v];
        float value;

        if (isPulse)
        {
            value = t < width ? 1.0f : -1.0f;
            value += polyBlep (t, dt);                                   // rising edge at t = 0
            value -= polyBlep (std::fmod (t + 1.0f - width, 1.0f), dt);  // falling edge at t = width
            value -= pulseDcOffset;
        }
        else // Saw and Supersaw
        {
            value = 2.0f * t - 1.0f - polyBlep (t, dt);
        }

        sum += value;

        auto next = phases[(size_t) v] + (double) dt;
        if (next >= 1.0)
            next -= 1.0;
        phases[(size_t) v] = next;
    }

    return sum * voiceGain;
}

void CarrierOsc::process (float* output, int numSamples)
{
    const float noiseAmt = juce::jlimit (0.0f, 1.0f, settings.noise);
    const float toneAmt  = 1.0f - noiseAmt;
    const float invSr    = (float) (1.0 / sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float baseDt = frequency.getNextValue() * invSr;
        float s = renderVoices (baseDt) * toneAmt;

        if (noiseAmt > 0.0f)
            s += (random.nextFloat() * 2.0f - 1.0f) * noiseAmt;

        output[i] = s;
    }
}

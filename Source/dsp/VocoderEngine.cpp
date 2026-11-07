#include "VocoderEngine.h"

namespace
{
    // One-pole smoothing coefficient for a time constant in ms.
    float timeToCoef (float ms, double sampleRate) noexcept
    {
        const double samples = juce::jmax (1.0, (double) ms * 0.001 * sampleRate);
        return (float) std::exp (-1.0 / samples);
    }
}

void VocoderEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& f : sibilanceFilters)
        f.setHighpass (sampleRate, juce::jmin (5000.0, sampleRate * 0.4), 0.707);
    filtersDirty = true;
    updateFilters();
    reset();
}

void VocoderEngine::reset()
{
    for (auto& band : modFilters) for (auto& f : band) f.reset();
    for (auto& band : carFilters) for (auto& f : band) f.reset();
    for (auto& f : sibilanceFilters) f.reset();
    envelopes.fill (0.0f);
}

void VocoderEngine::setSettings (const Settings& newSettings)
{
    const int bands = juce::jlimit (2, maxBands, newSettings.numBands);
    if (bands != settings.numBands || std::abs (newSettings.formantSemitones - settings.formantSemitones) > 1.0e-3f)
        filtersDirty = true;

    settings = newSettings;
    settings.numBands = bands;
    settings.sibilance = juce::jlimit (0.0f, 1.0f, settings.sibilance);

    attackCoef  = timeToCoef (settings.attackMs,  sampleRate);
    releaseCoef = timeToCoef (settings.releaseMs, sampleRate);

    if (filtersDirty)
        updateFilters();
}

void VocoderEngine::updateFilters()
{
    const int n = settings.numBands;
    const double top = juce::jmin ((double) highestBandHz, sampleRate * 0.42);
    const double ratio = std::pow (top / lowestBandHz, 1.0 / (n - 1)); // spacing between band centres

    // Pick Q so neighbouring bands meet around their -3 dB points.
    // Two identical filters in series are narrower than one, hence the 0.65 factor.
    const double octaves = std::log2 (ratio);
    const double bandQ = std::sqrt (std::exp2 (octaves)) / (std::exp2 (octaves) - 1.0);
    const double stageQ = bandQ * 0.65;

    const double formantRatio = std::exp2 (-settings.formantSemitones / 12.0);

    for (int b = 0; b < n; ++b)
    {
        const double carrierHz = lowestBandHz * std::pow (ratio, b);
        const double voiceHz   = juce::jlimit (30.0, sampleRate * 0.45, carrierHz * formantRatio);

        for (auto& f : carFilters[(size_t) b]) f.setBandpass (sampleRate, carrierHz, stageQ);
        for (auto& f : modFilters[(size_t) b]) f.setBandpass (sampleRate, voiceHz, stageQ);
    }

    // More bands = each band carries less energy (both voice and carrier are split N ways),
    // so add back level in proportion to N.
    // Calibrated on speech-like vowels, not white noise: speech bunches its energy into a few
    // formant bands, so it comes out much louder than noise at the same input level.
    // 2.3 * N matched white noise; -14 dB from there brings vowels at -20 dBFS in to about
    // -20 dBFS out, the same loudness as Talkbox mode (see VocoderTests).
    constexpr float speechCalibration = 0.2f; // -14 dB
    makeupGain = 2.3f * speechCalibration * (float) n;
    filtersDirty = false;
}

void VocoderEngine::process (const float* modulator, const float* carrier, float* out, int numSamples)
{
    const int n = settings.numBands;
    const float sib = settings.sibilance;

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = modulator[i];
        const float c = carrier[i];
        float sum = 0.0f;

        for (int b = 0; b < n; ++b)
        {
            auto& mf = modFilters[(size_t) b];
            auto& cf = carFilters[(size_t) b];

            // Envelope follower: rectify the voice band, then smooth it.
            const float level = std::abs (mf[1].process (mf[0].process (m)));
            auto& env = envelopes[(size_t) b];
            const float coef = level > env ? attackCoef : releaseCoef;
            env = level + coef * (env - level);

            sum += cf[1].process (cf[0].process (c)) * env;
        }

        // Always run the sibilance filter so its memory stays current (no click when turned up).
        const float hiss = sibilanceFilters[1].process (sibilanceFilters[0].process (m));
        out[i] = sum * makeupGain + hiss * sib;
    }
}

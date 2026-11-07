#include "Equalizer.h"

void Equalizer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (int b = 0; b < numBands; ++b)
    {
        freq[(size_t) b].reset (sampleRate, 0.03);
        q[(size_t) b].reset (sampleRate, 0.03);
        gain[(size_t) b].reset (sampleRate, 0.03);
    }
    wetAmount.reset (sampleRate, 0.02);
    oldAmount.reset (sampleRate, 0.02);
    reset();
}

void Equalizer::reset()
{
    for (int b = 0; b < numBands; ++b)
    {
        const auto& s = settings.bands[(size_t) b];
        freq[(size_t) b].setCurrentAndTargetValue (s.freq);
        q[(size_t) b].setCurrentAndTargetValue (s.q);
        gain[(size_t) b].setCurrentAndTargetValue (s.gainDb);
        filters[(size_t) b].reset();
    }
    wetAmount.setCurrentAndTargetValue (settings.enabled ? 1.0f : 0.0f);
    oldAmount.setCurrentAndTargetValue (0.0f);
    updateFilters();
}

void Equalizer::setSettings (const Settings& newSettings)
{
    // Only the low and high bands have a Cut option.
    auto incoming = newSettings;
    incoming.bands[mid].cut = false;
    const bool typeChanged = incoming.bands[lowShelf].cut != settings.bands[lowShelf].cut
                          || incoming.bands[highShelf].cut != settings.bands[highShelf].cut;
    settings = incoming;
    for (int b = 0; b < numBands; ++b)
    {
        auto& s = settings.bands[(size_t) b];
        s.freq = juce::jlimit (10.0f, 22000.0f, s.freq);
        s.q    = juce::jlimit (0.1f, 20.0f, s.q);
        freq[(size_t) b].setTargetValue (s.freq);
        q[(size_t) b].setTargetValue (s.q);
        gain[(size_t) b].setTargetValue (s.gainDb);
    }

    // Coming back on from fully bypassed: start the filters from silence, then fade them in.
    if (settings.enabled && wetAmount.getCurrentValue() == 0.0f && ! wetAmount.isSmoothing())
        for (auto& f : filters)
            f.reset();
    wetAmount.setTargetValue (settings.enabled ? 1.0f : 0.0f);

    // Shelf <-> Cut swaps the filter type, which would click if done instantly.
    // Keep a copy of the current filters running and fade from them to the new ones.
    if (typeChanged)
    {
        oldFilters = filters;
        oldAmount.setCurrentAndTargetValue (1.0f);
        oldAmount.setTargetValue (0.0f);
        updateFilters();
    }
}

void Equalizer::design (Biquad& filter, int band, const BandSettings& s, double sr) noexcept
{
    // Keep the corner safely below Nyquist (half the sample rate), where the formulas break down.
    const double f = juce::jlimit (10.0, sr * 0.45, (double) s.freq);
    switch (band)
    {
        case lowShelf:  s.cut ? filter.setHighpass (sr, f, s.q) : filter.setLowShelf  (sr, f, s.q, s.gainDb); break;
        case highShelf: s.cut ? filter.setLowpass  (sr, f, s.q) : filter.setHighShelf (sr, f, s.q, s.gainDb); break;
        default:        filter.setPeak      (sr, f, s.q, s.gainDb); break;
    }
}

double Equalizer::responseDb (const Settings& s, double hz, double sr) noexcept
{
    if (! s.enabled)
        return 0.0;
    double total = 0.0;
    for (int b = 0; b < numBands; ++b)
    {
        Biquad f;
        design (f, b, s.bands[(size_t) b], sr);
        total += f.magnitudeDb (sr, hz);
    }
    return total;
}

void Equalizer::updateFilters() noexcept
{
    for (int b = 0; b < numBands; ++b)
        design (filters[(size_t) b], b,
                { freq[(size_t) b].getCurrentValue(), q[(size_t) b].getCurrentValue(), gain[(size_t) b].getCurrentValue(),
                  settings.bands[(size_t) b].cut },
                sampleRate);
}

void Equalizer::process (float* data, int numSamples)
{
    // Fully off and staying off: leave the signal untouched and save the CPU.
    if (! settings.enabled && ! wetAmount.isSmoothing())
        return;

    for (int start = 0; start < numSamples; start += updateInterval)
    {
        const int n = juce::jmin (updateInterval, numSamples - start);

        bool moving = false;
        for (int b = 0; b < numBands; ++b)
        {
            const auto i = (size_t) b;
            if (freq[i].isSmoothing() || q[i].isSmoothing() || gain[i].isSmoothing())
            {
                freq[i].skip (n);
                q[i].skip (n);
                gain[i].skip (n);
                moving = true;
            }
        }
        if (moving)
            updateFilters();

        for (int i = start; i < start + n; ++i)
        {
            const float dry = data[i];
            float y = dry;
            for (auto& f : filters)
                y = f.process (y);
            if (oldAmount.isSmoothing())
            {
                float old = dry;
                for (auto& f : oldFilters)
                    old = f.process (old);
                y += oldAmount.getNextValue() * (old - y);
            }
            const float w = wetAmount.getNextValue();
            data[i] = dry + w * (y - dry);
        }
    }
}

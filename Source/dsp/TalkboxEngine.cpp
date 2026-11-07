#include "TalkboxEngine.h"

namespace
{
    float timeToCoef (float ms, double sampleRate) noexcept
    {
        const double samples = juce::jmax (1.0, (double) ms * 0.001 * sampleRate);
        return (float) std::exp (-1.0 / samples);
    }

    // Pre-emphasis boosts highs before analysis so LPC models the formants rather than the
    // voice's natural downward tilt; the saw carrier then supplies that tilt instead.
    constexpr float preEmphasis = 0.97f;
}

bool TalkboxEngine::levinson (const double* R, int p, double* kOut) noexcept
{
    std::array<double, maxOrder + 1> a {}, prev {};
    a[0] = 1.0;
    double error = R[0];

    for (int i = 0; i < p; ++i)
        kOut[i] = 0.0;

    if (! (error > 1.0e-12))
        return false;

    for (int i = 1; i <= p; ++i)
    {
        double acc = R[i];
        for (int j = 1; j < i; ++j)
            acc += a[(size_t) j] * R[i - j];

        double ki = -acc / error;
        if (! std::isfinite (ki))
            return false;
        ki = juce::jlimit (-0.9999, 0.9999, ki); // keep the filter strictly stable

        prev = a;
        for (int j = 1; j < i; ++j)
            a[(size_t) j] = prev[(size_t) j] + ki * prev[(size_t) (i - j)];
        a[(size_t) i] = ki;

        error *= (1.0 - ki * ki);
        kOut[i - 1] = ki;

        if (error <= 1.0e-12)
            break; // perfectly predicted: higher orders add nothing
    }
    return true;
}

void TalkboxEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    // About one coefficient per 2 kHz of sample rate: 24 at 48 kHz, enough for 4-5 formants.
    order     = juce::jlimit (16, maxOrder, (int) std::lround (sampleRate * 0.0005));
    frameSize = (int) std::lround (sampleRate * 0.030); // 30 ms analysis window
    hopSize   = juce::jmax (16, (int) std::lround (sampleRate * 0.0025)); // re-analyse every 2.5 ms

    history.assign ((size_t) (frameSize * 2 + 8), 0.0f);
    frame.assign ((size_t) frameSize, 0.0f);
    window.resize ((size_t) frameSize);
    for (int i = 0; i < frameSize; ++i) // Hann window: fades the frame edges to avoid smearing
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (frameSize - 1));

    for (auto& f : sibilanceFilters)
        f.setHighpass (sampleRate, juce::jmin (5000.0, sampleRate * 0.4), 0.707);

    tubeVoice.prepare (sampleRate);
    setSettings (settings);
    reset();
}

void TalkboxEngine::reset()
{
    std::fill (history.begin(), history.end(), 0.0f);
    writePos = 0;
    hopCounter = 0;
    k.fill (0.0f);
    kStep.fill (0.0f);
    lattice.reset();
    tubeVoice.reset();
    prevVoice = 0.0f;
    voicePower = outPower = 0.0f;
    for (auto& f : sibilanceFilters) f.reset();
}

void TalkboxEngine::setSettings (const Settings& newSettings)
{
    settings = newSettings;
    settings.formantSemitones = juce::jlimit (-12.0f, 12.0f, settings.formantSemitones);
    settings.sibilance = juce::jlimit (0.0f, 1.0f, settings.sibilance);
    attackCoef  = timeToCoef (settings.attackMs,  sampleRate);
    releaseCoef = timeToCoef (settings.releaseMs, sampleRate);
    tubeVoice.setAmount (settings.tube);
}

void TalkboxEngine::analyse()
{
    // Formant shift: read the voice history with a stretched/squeezed spacing.
    // Spacing 2 (+12 st) packs twice as much time into the frame, so every frequency
    // in it appears twice as high: the mouth shape moves up an octave.
    const double spacing = std::exp2 (settings.formantSemitones / 12.0);
    const int size = (int) history.size();
    const int newest = writePos - 1 + size; // + size keeps the modulo positive

    for (int n = 0; n < frameSize; ++n)
    {
        const double back = (double) (frameSize - 1 - n) * spacing;
        const int i0 = (int) back;
        const float frac = (float) (back - i0);
        const float a = history[(size_t) ((newest - i0) % size)];
        const float b = history[(size_t) ((newest - i0 - 1) % size)];
        frame[(size_t) n] = (a + (b - a) * frac) * window[(size_t) n];
    }

    std::array<double, maxOrder + 1> R {};
    for (int lag = 0; lag <= order; ++lag)
    {
        double sum = 0.0;
        for (int n = lag; n < frameSize; ++n)
            sum += (double) frame[(size_t) n] * frame[(size_t) (n - lag)];
        R[(size_t) lag] = sum;
    }
    R[0] *= 1.0 + 1.0e-4; // tiny "noise floor" keeps the maths well-conditioned

    std::array<double, maxOrder> target {};
    levinson (R.data(), order, target.data());

    // Glide from the current filter to the new one over the next hop.
    // Every in-between filter is also stable, because each |k| stays below 1.
    for (int i = 0; i < order; ++i)
    {
        kTarget[(size_t) i] = (float) target[(size_t) i];
        kStep[(size_t) i] = (kTarget[(size_t) i] - k[(size_t) i]) / (float) hopSize;
    }
}

void TalkboxEngine::listen (const float* voice, int numSamples)
{
    const int size = (int) history.size();
    for (int i = 0; i < numSamples; ++i)
    {
        history[(size_t) writePos] = voice[i] - preEmphasis * prevVoice;
        prevVoice = voice[i];
        if (++writePos == size)
            writePos = 0;
    }
}

void TalkboxEngine::restart()
{
    lattice.reset();
    tubeVoice.reset();
    voicePower = outPower = 0.0f;
    for (auto& f : sibilanceFilters) f.reset();

    hopCounter = 0;
    analyse();
    k = kTarget; // start on the measured mouth shape instead of gliding in from flat
    kStep.fill (0.0f);
}

void TalkboxEngine::process (const float* voice, const float* carrier, float* out, int numSamples)
{
    const int size = (int) history.size();
    const float sib = settings.sibilance;

    for (int i = 0; i < numSamples; ++i)
    {
        const float v = voice[i];

        history[(size_t) writePos] = v - preEmphasis * prevVoice;
        prevVoice = v;
        if (++writePos == size)
            writePos = 0;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            analyse();
        }

        for (int j = 0; j < order; ++j)
            k[(size_t) j] += kStep[(size_t) j];

        // The carrier goes through the guitar-rig voicing first, then the "mouth".
        const float raw = lattice.process (tubeVoice.process (carrier[i]), k.data(), order);

        // Loudness matching: follow the power of the voice and of the raw filter output with
        // the same attack/release, then scale the output so the two match.
        const float vp = v * v, op = raw * raw;
        voicePower = vp + (vp > voicePower ? attackCoef : releaseCoef) * (voicePower - vp);
        outPower   = op + (op > outPower   ? attackCoef : releaseCoef) * (outPower - op);
        const float gain = juce::jmin (100.0f, std::sqrt (voicePower / (outPower + 1.0e-12f)));

        const float hiss = sibilanceFilters[1].process (sibilanceFilters[0].process (v));
        out[i] = raw * gain + hiss * sib;
    }

    // Safety net: if anything ever went non-finite, start clean rather than output garbage.
    if (! std::isfinite (outPower) || ! std::isfinite (lattice.state[0]))
        reset();
}

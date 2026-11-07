#pragma once

#include <array>
#include "Biquad.h"

// "Tube" voicing for the talkbox carrier, modelled loosely on a classic guitar talkbox rig:
// an overdriven guitar amp feeds a horn driver, which pushes sound up a plastic tube into
// the mouth. Applied to the carrier BEFORE the talkbox mouth filter, in this order:
//
//  1. Overdrive: asymmetric soft clipping, like a guitar pedal or amp. The asymmetry adds
//     even harmonics (warm), the clipping adds odd ones (grit). Clipping a chord stack also
//     creates the growly intermodulation of a distorted power chord.
//  2. Horn + tube band-limit: cuts deep lows and fizzy highs, which a horn driver and a
//     narrow tube can't carry.
//  3. Mid honk: a resonant bump around 1.2 kHz, the nasal "through a tube" colour.
//
// Amount 0 = bypassed (bit-exact). Level isn't compensated here: the talkbox matches its
// output loudness to the voice afterwards, so more Tube means more grit, not more volume.
class TubeVoice
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        lastAmount = -1.0f; // force a coefficient update
        setAmount (amount);
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : filters) f.reset();
    }

    void setAmount (float newAmount) noexcept
    {
        amount = juce::jlimit (0.0f, 1.0f, newAmount);
        if (std::abs (amount - lastAmount) < 1.0e-4f)
            return;
        lastAmount = amount;

        const double t = amount;
        const double nyquistSafe = sampleRate * 0.45;

        driveGain = 1.0f + 30.0f * std::pow (amount, 1.5f); // up to ~+30 dB into the clipper
        bias = 0.25f * amount;                              // asymmetry -> even harmonics
        biasOffset = std::tanh (bias);                      // removes the DC the bias creates

        const double lowCut  = 20.0 + 130.0 * t;                               // 20 -> 150 Hz
        const double highCut = juce::jmin (nyquistSafe, 20000.0 - 15500.0 * t); // 20 k -> 4.5 kHz
        filters[0].setHighpass (sampleRate, lowCut, 0.707);
        filters[1].setHighpass (sampleRate, lowCut, 0.707);  // two in series: 24 dB/oct, like a small horn
        filters[2].setLowpass  (sampleRate, highCut, 0.707);
        filters[3].setLowpass  (sampleRate, highCut, 0.707);
        filters[4].setPeak     (sampleRate, 1200.0, 1.2, 8.0 * t); // up to +8 dB honk
    }

    inline float process (float x) noexcept
    {
        if (amount <= 0.0f)
            return x;

        // Overdrive, then the rig's frequency response.
        float y = std::tanh (driveGain * x + bias) - biasOffset;
        for (auto& f : filters)
            y = f.process (y);
        return y;
    }

    float getAmount() const noexcept { return amount; }

private:
    double sampleRate = 48000.0;
    float amount = 0.0f, lastAmount = -1.0f;
    float driveGain = 1.0f, bias = 0.0f, biasOffset = 0.0f;
    std::array<Biquad, 5> filters {};
};

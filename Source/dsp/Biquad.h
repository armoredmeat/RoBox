#pragma once

#include <cmath>
#include <complex>
#include <juce_core/juce_core.h>

// A biquad is the basic 2nd-order digital filter (low-pass, high-pass, band-pass...).
// Coefficient formulas are from Robert Bristow-Johnson's "Audio EQ Cookbook".
// This one is tiny and allocation-free, so coefficients can be changed safely on the audio thread.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; // coefficients (a0 normalised to 1)
    float z1 = 0.0f, z2 = 0.0f;                                  // filter memory

    void reset() noexcept { z1 = z2 = 0.0f; }

    // Transposed Direct Form II: numerically well-behaved with floats.
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // Band-pass with 0 dB gain at the centre frequency.
    void setBandpass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        set (alpha / a0, 0.0, -alpha / a0, -2.0 * std::cos (w0) / a0, (1.0 - alpha) / a0);
    }

    void setHighpass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        set ((1.0 + cs) / 2.0 / a0, -(1.0 + cs) / a0, (1.0 + cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - alpha) / a0);
    }

    void setLowpass (double sampleRate, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        set ((1.0 - cs) / 2.0 / a0, (1.0 - cs) / a0, (1.0 - cs) / 2.0 / a0, -2.0 * cs / a0, (1.0 - alpha) / a0);
    }

    // Bell EQ: boosts (or cuts) gainDb around freq; q sets how wide.
    void setPeak (double sampleRate, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cs = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha / A;
        set ((1.0 + alpha * A) / a0, -2.0 * cs / a0, (1.0 - alpha * A) / a0, -2.0 * cs / a0, (1.0 - alpha / A) / a0);
    }

    // Shelves: boost (or cut) everything below (low shelf) or above (high shelf) freq.
    // q = 0.707 gives a smooth step; higher q adds a small bump next to the corner.
    void setLowShelf (double sampleRate, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cs = std::cos (w0);
        const double beta = 2.0 * std::sqrt (A) * std::sin (w0) / (2.0 * q);
        const double a0 = (A + 1.0) + (A - 1.0) * cs + beta;
        set (A * ((A + 1.0) - (A - 1.0) * cs + beta) / a0,
             2.0 * A * ((A - 1.0) - (A + 1.0) * cs) / a0,
             A * ((A + 1.0) - (A - 1.0) * cs - beta) / a0,
             -2.0 * ((A - 1.0) + (A + 1.0) * cs) / a0,
             ((A + 1.0) + (A - 1.0) * cs - beta) / a0);
    }

    void setHighShelf (double sampleRate, double freq, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cs = std::cos (w0);
        const double beta = 2.0 * std::sqrt (A) * std::sin (w0) / (2.0 * q);
        const double a0 = (A + 1.0) - (A - 1.0) * cs + beta;
        set (A * ((A + 1.0) + (A - 1.0) * cs + beta) / a0,
             -2.0 * A * ((A - 1.0) + (A + 1.0) * cs) / a0,
             A * ((A + 1.0) + (A - 1.0) * cs - beta) / a0,
             2.0 * ((A - 1.0) - (A + 1.0) * cs) / a0,
             ((A + 1.0) - (A - 1.0) * cs - beta) / a0);
    }

    // How much this filter boosts or cuts a sine at freq, in dB (used to draw the EQ curve).
    double magnitudeDb (double sampleRate, double freq) const noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const std::complex<double> z1inv = std::polar (1.0, -w), z2inv = z1inv * z1inv;
        const auto num = (double) b0 + (double) b1 * z1inv + (double) b2 * z2inv;
        const auto den = 1.0 + (double) a1 * z1inv + (double) a2 * z2inv;
        return 20.0 * std::log10 (std::abs (num) / std::abs (den) + 1.0e-12);
    }

private:
    void set (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = (float) nb0; b1 = (float) nb1; b2 = (float) nb2; a1 = (float) na1; a2 = (float) na2;
    }
};

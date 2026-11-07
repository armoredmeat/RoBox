#pragma once

// Test signals shared by several test files.
#include <juce_core/juce_core.h>
#include <vector>

namespace TestSignals
{
    struct Vowel { const char* name; double f1, f2; };

    // Rough first/second formants of three very different vowels.
    inline const std::vector<Vowel> vowels { { "ah", 700.0, 1200.0 }, { "ee", 300.0, 2300.0 }, { "oo", 300.0, 870.0 } };

    inline float rms (const std::vector<float>& x, size_t from = 0)
    {
        double sum = 0;
        for (size_t i = from; i < x.size(); ++i) sum += (double) x[i] * x[i];
        return (float) std::sqrt (sum / (double) juce::jmax<size_t> (1, x.size() - from));
    }

    // A synthetic vowel: a 120 Hz pulse train (like vocal cords) through two resonances
    // (like the mouth), scaled to 0.1 RMS = -20 dBFS. Much closer to real speech than
    // white noise, because the energy is bunched into a few formants.
    inline std::vector<float> vowel (int n, double rate, double f1 = 700.0, double f2 = 1200.0)
    {
        std::vector<float> s ((size_t) n, 0.0f);
        const int period = (int) (rate / 120.0);
        for (int i = 0; i < n; i += period) s[(size_t) i] = 1.0f;

        for (double f : { f1, f2 })
        {
            const double r = std::exp (-juce::MathConstants<double>::pi * 80.0 / rate);
            const double c = 2.0 * r * std::cos (juce::MathConstants<double>::twoPi * f / rate);
            double y1 = 0, y2 = 0;
            for (auto& v : s)
            {
                const double y = v + c * y1 - r * r * y2;
                y2 = y1; y1 = y;
                v = (float) y;
            }
        }
        const float scale = 0.1f / rms (s);
        for (auto& v : s) v *= scale;
        return s;
    }
}

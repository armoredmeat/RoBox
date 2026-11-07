#include "LiveDisplay.h"

namespace
{
    constexpr float headerHeight = 18.0f, footerHeight = 16.0f, padding = 10.0f;

    // 0..1 position of a frequency on the log axis.
    float freqToX (float hz, float minHz, float maxHz) { return std::log (hz / minHz) / std::log (maxHz / minHz); }
}

juce::Rectangle<float> LiveDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (padding).withTrimmedTop (headerHeight).withTrimmedBottom (footerHeight);
}

void LiveDisplay::update (float formantSemitones)
{
    formant = formantSemitones;
    talkbox = source.talkboxMode.load (std::memory_order_relaxed);

    if (talkbox)
    {
        computeVowelCurve();
    }
    else
    {
        numBands = juce::jlimit (2, VocoderEngine::maxBands, source.numBands.load (std::memory_order_relaxed));
        for (int b = 0; b < numBands; ++b)
        {
            // Band envelope -> 0..1 over a 40 dB window. Bars jump up instantly and fall smoothly.
            const float db = juce::Decibels::gainToDecibels (source.bandLevels[(size_t) b].load (std::memory_order_relaxed), -100.0f);
            const float target = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 40.0f);
            bars[(size_t) b] = juce::jmax (target, bars[(size_t) b] * 0.85f);
        }
    }
    repaint();
}

void LiveDisplay::computeVowelCurve()
{
    // Turn the talkbox's reflection coefficients back into a normal filter polynomial A(z)
    // ("step-up" recursion), then evaluate 1/|A| along the frequency axis.
    const int order = juce::jlimit (0, TalkboxEngine::maxOrder, source.lpcOrder.load (std::memory_order_relaxed));
    std::array<double, TalkboxEngine::maxOrder + 1> a {}, prev {};
    a[0] = 1.0;
    for (int i = 1; i <= order; ++i)
    {
        const double k = source.lpc[(size_t) i - 1].load (std::memory_order_relaxed);
        prev = a;
        for (int j = 1; j < i; ++j)
            a[(size_t) j] = prev[(size_t) j] + k * prev[(size_t) (i - j)];
        a[(size_t) i] = k;
    }

    const double sampleRate = source.sampleRate.load (std::memory_order_relaxed);
    std::array<float, curvePoints> db {};
    float peak = -1000.0f;
    for (int p = 0; p < curvePoints; ++p)
    {
        const double hz = minHz * std::pow (maxHz / minHz, (double) p / (curvePoints - 1));
        const double w = juce::MathConstants<double>::twoPi * hz / sampleRate;
        double re = 0.0, im = 0.0;
        for (int j = 0; j <= order; ++j)
        {
            re += a[(size_t) j] * std::cos (w * j);
            im -= a[(size_t) j] * std::sin (w * j);
        }
        db[(size_t) p] = (float) (-10.0 * std::log10 (re * re + im * im + 1.0e-12));
        peak = juce::jmax (peak, db[(size_t) p]);
    }

    // Shape relative to its own peak over a 45 dB window, scaled by how loud the voice is,
    // so the curve sinks when you stop talking.
    const float voiceDb = juce::Decibels::gainToDecibels (source.voiceRms.load (std::memory_order_relaxed), -100.0f);
    const float loudness = juce::jlimit (0.0f, 1.0f, (voiceDb + 60.0f) / 40.0f);
    for (int p = 0; p < curvePoints; ++p)
    {
        const float target = juce::jlimit (0.0f, 1.0f, (db[(size_t) p] - peak + 45.0f) / 45.0f) * loudness;
        curve[(size_t) p] += (target - curve[(size_t) p]) * 0.4f; // smooth between frames
    }
}

void LiveDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    const auto inner = bounds.reduced (padding);
    g.setFont (Theme::font (11.0f));
    g.setColour (Theme::subtext);
    const auto header = inner.withHeight (headerHeight);
    g.drawText (talkbox ? juce::String ("Vowel shape") : juce::String (numBands) + " bands", header, juce::Justification::topLeft);
    g.drawText ("formant " + (std::abs (formant) < 0.05f ? juce::String ("0") : (formant > 0 ? "+" : "") + juce::String (formant, 1)) + " st",
                header, juce::Justification::topRight);

    const auto plot = plotArea();

    // Frequency labels under the plot.
    const auto footer = juce::Rectangle<float> (plot.getX(), plot.getBottom() + 2.0f, plot.getWidth(), footerHeight);
    for (auto [hz, text, just] : { std::tuple { 80.0f, "80 Hz", juce::Justification::left },
                                   std::tuple { 1000.0f, "1 kHz", juce::Justification::centred },
                                   std::tuple { 12000.0f, "12 kHz", juce::Justification::right } })
    {
        const float x = plot.getX() + freqToX (hz, minHz, maxHz) * plot.getWidth();
        auto box = footer.withWidth (60.0f);
        if (just == juce::Justification::left)        box.setX (plot.getX());
        else if (just == juce::Justification::right)  box.setX (plot.getRight() - box.getWidth());
        else                                          box.setCentre (x, footer.getCentreY());
        g.drawText (text, box, just);
    }

    g.setColour (Theme::carrier);
    if (talkbox)
    {
        juce::Path line, fill;
        for (int p = 0; p < curvePoints; ++p)
        {
            const float x = plot.getX() + plot.getWidth() * (float) p / (curvePoints - 1);
            const float y = plot.getBottom() - curve[(size_t) p] * plot.getHeight();
            if (p == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
            else        line.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setColour (Theme::carrier.withAlpha (0.25f));
        g.fillPath (fill);
        g.setColour (Theme::carrier);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        const float gap = 3.0f;
        const float w = (plot.getWidth() - gap * (float) (numBands - 1)) / (float) numBands;
        for (int b = 0; b < numBands; ++b)
        {
            const float h = juce::jmax (2.0f, bars[(size_t) b] * plot.getHeight());
            const auto bar = juce::Rectangle<float> (plot.getX() + (float) b * (w + gap), plot.getBottom() - h, w, h);
            juce::Path p;
            p.addRoundedRectangle (bar.getX(), bar.getY(), bar.getWidth(), bar.getHeight(), 2.0f, 2.0f, true, true, false, false);
            g.fillPath (p);
        }
    }
}

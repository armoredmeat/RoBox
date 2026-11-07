#pragma once

#include "Theme.h"
#include "../PluginProcessor.h"

// The centre display: what the voice is doing right now.
//  - Vocoder mode: one bar per band, showing how loud the voice is in that band.
//  - Talkbox mode: the "vowel shape" curve, i.e. the frequency response of the mouth
//    filter the talkbox has estimated from the voice. Peaks are the formants.
// The horizontal axis is frequency, 80 Hz to 12 kHz, spaced like a piano (log scale).
class LiveDisplay final : public juce::Component
{
public:
    explicit LiveDisplay (RoBoxAudioProcessor::DisplayData& data) : source (data) {}

    // Called by the editor's timer (~30 times a second) on the message thread.
    void update (float formantSemitones);

    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<float> plotArea() const;
    void computeVowelCurve();

    static constexpr float minHz = 80.0f, maxHz = 12000.0f;
    static constexpr int curvePoints = 96;

    RoBoxAudioProcessor::DisplayData& source;
    bool talkbox = false;
    int numBands = 16;
    float formant = 0.0f;
    std::array<float, VocoderEngine::maxBands> bars {};
    std::array<float, curvePoints> curve {};
};

// Stereo output level meter (peak, with a smooth fall).
class OutputMeter final : public juce::Component
{
public:
    explicit OutputMeter (RoBoxAudioProcessor::DisplayData& data) : source (data) {}

    void update()
    {
        auto toLevel = [] (float peak) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 48.0f) / 48.0f); };
        const float l = toLevel (source.outPeakLeft.exchange (0.0f));
        const float r = toLevel (source.outPeakRight.exchange (0.0f));
        left  = juce::jmax (l, left * 0.88f);
        right = juce::jmax (r, right * 0.88f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();
        const float w = (area.getWidth() - 3.0f) * 0.5f;
        int i = 0;
        for (float level : { left, right })
        {
            const auto bar = juce::Rectangle<float> (area.getX() + (float) i++ * (w + 3.0f), area.getY(), w, area.getHeight());
            g.setColour (Theme::panel);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (level > 0.97f ? Theme::danger : Theme::output); // red when near full scale
            g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * level), 2.0f);
        }
    }

private:
    RoBoxAudioProcessor::DisplayData& source;
    float left = 0.0f, right = 0.0f;
};

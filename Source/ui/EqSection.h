#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Knob.h"
#include "../dsp/Equalizer.h"

// The 3-band EQ panel: a small response graph, Low Shelf / Mid / High Shelf buttons that pick
// which band the Freq, Q and Level knobs control, and an ON/OFF button.
// Clicking the Low or High button again (or right-clicking it) switches that band between
// Shelf and Cut; the button's label shows which ("Low Cut"), and Level greys out for a cut.
// You can also drag a band's dot on the graph (left/right = frequency, up/down = level),
// use the mouse wheel over the graph to change Q, and double-click a dot to flatten that band.
class EqSection final : public juce::Component
{
public:
    EqSection (juce::AudioProcessorValueTreeState&, const std::atomic<float>& sampleRate);

    void refresh();   // call regularly (the editor's timer): redraws the graph when a value changed
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    static constexpr int preferredWidth = 328, preferredHeight = 100;

private:
    // A TextButton that tells us which mouse button clicked it (right-click opens a menu).
    struct BandButton final : juce::TextButton
    {
        std::function<void (const juce::ModifierKeys&)> onClickWithModifiers;
        void clicked (const juce::ModifierKeys& mods) override { if (onClickWithModifiers) onClickWithModifiers (mods); }
    };

    class Graph final : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Graph (EqSection& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    private:
        juce::Rectangle<float> plot() const;
        float freqToX (float hz) const;
        float xToFreq (float x) const;
        float dbToY (float db) const;
        float yToDb (float y) const;
        juce::Point<float> nodePosition (int band) const;
        int bandAt (juce::Point<float>) const;

        EqSection& owner;
        int dragBand = -1;
        juce::Point<float> dragPoint, lastMouse;
        bool dragLevel = false;
    };

    void selectBand (int band);
    void bandButtonClicked (int band, const juce::ModifierKeys&);
    void setCut (int band, bool cut);
    void updateBandLabels();
    juce::RangedAudioParameter& param (const char* id) const;
    void setParam (const char* id, float value);

    juce::AudioProcessorValueTreeState& apvts;
    const std::atomic<float>& sampleRate;
    Equalizer::Settings current;
    int selected = Equalizer::mid;
    bool firstRefresh = true;

    std::array<BandButton, Equalizer::numBands> bandButtons;
    juce::TextButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    Graph graph { *this };
    juce::OwnedArray<Knob> freqKnobs, qKnobs, levelKnobs; // one of each per band; only the selected band's are shown

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqSection)
};

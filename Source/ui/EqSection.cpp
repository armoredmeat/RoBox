#include "EqSection.h"
#include "../Parameters.h"
#include "Help.h"

namespace
{
    constexpr float minHz = 20.0f, maxHz = 20000.0f;
    constexpr float rangeDb = 15.0f; // graph shows -15 .. +15 dB, the Level knob's full range
    const juce::Identifier selectedBandProperty { "eqSelectedBand" }; // UI-only, saved with the project

    juce::String formatHz (double v)
    {
        if (v < 1000.0) return juce::String (juce::roundToInt (v)) + " Hz";
        return juce::String (v / 1000.0, v < 10000.0 ? 2 : 1) + " kHz";
    }

    juce::String formatQ (double v) { return juce::String (v, 2); }
}

EqSection::EqSection (juce::AudioProcessorValueTreeState& state, const std::atomic<float>& sr)
    : apvts (state), sampleRate (sr)
{
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        auto& button = bandButtons[(size_t) b];
        button.setConnectedEdges ((b > 0 ? juce::Button::ConnectedOnLeft : 0)
                                  | (b < Equalizer::numBands - 1 ? juce::Button::ConnectedOnRight : 0));
        button.onClickWithModifiers = [this, b] (const juce::ModifierKeys& mods) { bandButtonClicked (b, mods); };
        Help::set (button, b == Equalizer::mid
            ? juce::String ("Mid band: a bell-shaped boost or cut around one frequency. Click to edit it with the knobs.")
            : juce::String (b == Equalizer::lowShelf ? "Low band" : "High band")
              + ": click to edit it. Click again, or right-click, to switch between Shelf (boost or cut) and Cut (filter out).");
        addAndMakeVisible (button);

        freqKnobs.add  (new Knob (apvts, ParamID::eqFreq[b], "Freq",  Theme::eq, formatHz));
        qKnobs.add     (new Knob (apvts, ParamID::eqQ[b],    "Q",     Theme::eq, formatQ));
        levelKnobs.add (new Knob (apvts, ParamID::eqGain[b], "Level", Theme::eq, Format::decibels, Knob::Arc::fromCentre));
        for (auto* k : { freqKnobs.getLast(), qKnobs.getLast(), levelKnobs.getLast() })
            addChildComponent (k);
        Help::set (*freqKnobs.getLast(),  b == Equalizer::mid ? "Freq: the centre of the bell, 100 Hz to 10 kHz."
                                                             : "Freq: where the shelf or cut starts, anywhere from 20 Hz to 20 kHz.");
        Help::set (*qKnobs.getLast(),     b == Equalizer::mid ? "Q: how narrow the bell is. Low is broad and gentle, high is narrow and focused."
                                                             : "Q: the shape of the corner. 0.71 is smooth; higher adds a bump at the corner.");
        Help::set (*levelKnobs.getLast(), "Level: how much this band boosts (+) or cuts (-). Not used when the band is set to Cut.");
    }

    onButton.setClickingTogglesState (true);
    Help::set (onButton, "EQ on/off: the EQ shapes only the effect, not your dry voice.");
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamID::eqOn, onButton);
    addAndMakeVisible (onButton);

    Help::set (graph, "EQ curve: drag a dot to set frequency and level (Shift for fine), scroll to change Q, double-click a dot to flatten it.");
    addAndMakeVisible (graph);

    lookAndFeelChanged();
    selectBand (juce::jlimit (0, Equalizer::numBands - 1, (int) apvts.state.getProperty (selectedBandProperty, (int) Equalizer::mid)));
    refresh();
}

void EqSection::lookAndFeelChanged() // also called when the theme changes
{
    for (auto* b : std::initializer_list<juce::Button*> { &bandButtons[0], &bandButtons[1], &bandButtons[2], &onButton })
    {
        b->setColour (juce::TextButton::buttonOnColourId, Theme::eq);
        b->setColour (juce::TextButton::textColourOnId, Theme::eqInk);
    }
    graph.repaint();
}

juce::RangedAudioParameter& EqSection::param (const char* id) const
{
    auto* p = apvts.getParameter (id);
    jassert (p != nullptr);
    return *p;
}

void EqSection::setParam (const char* id, float value)
{
    auto& p = param (id);
    p.setValueNotifyingHost (p.convertTo0to1 (p.getNormalisableRange().snapToLegalValue (value)));
}

void EqSection::selectBand (int band)
{
    selected = band;
    apvts.state.setProperty (selectedBandProperty, band, nullptr);
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        bandButtons[(size_t) b].setToggleState (b == band, juce::dontSendNotification);
        freqKnobs[b]->setVisible (b == band);
        qKnobs[b]->setVisible (b == band);
        levelKnobs[b]->setVisible (b == band);
    }
    graph.repaint();
}

void EqSection::bandButtonClicked (int band, const juce::ModifierKeys& mods)
{
    const bool hasType = ParamID::eqType[band] != nullptr;

    if (mods.isPopupMenu() && hasType)
    {
        selectBand (band);
        const bool cut = current.bands[(size_t) band].cut;
        juce::PopupMenu menu;
        menu.addItem (band == Equalizer::lowShelf ? "Low Shelf" : "High Shelf", true, ! cut, [this, band] { setCut (band, false); });
        menu.addItem (band == Equalizer::lowShelf ? "Low Cut (12 dB/oct)" : "High Cut (12 dB/oct)", true, cut, [this, band] { setCut (band, true); });
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (bandButtons[(size_t) band]));
        return;
    }

    // Clicking the band that's already selected flips Shelf <-> Cut; otherwise just select it.
    if (band == selected && hasType)
        setCut (band, ! current.bands[(size_t) band].cut);
    else
        selectBand (band);
}

void EqSection::setCut (int band, bool cut)
{
    auto& type = param (ParamID::eqType[band]);
    type.beginChangeGesture();
    type.setValueNotifyingHost (type.convertTo0to1 (cut ? 1.0f : 0.0f));
    type.endChangeGesture();
    refresh();
}

void EqSection::updateBandLabels()
{
    bandButtons[Equalizer::lowShelf].setButtonText (current.bands[Equalizer::lowShelf].cut ? "Low Cut" : "Low Shelf");
    bandButtons[Equalizer::mid].setButtonText ("Mid");
    bandButtons[Equalizer::highShelf].setButtonText (current.bands[Equalizer::highShelf].cut ? "High Cut" : "High Shelf");
}

void EqSection::refresh()
{
    Equalizer::Settings s;
    s.enabled = apvts.getRawParameterValue (ParamID::eqOn)->load() > 0.5f;
    for (size_t b = 0; b < Equalizer::numBands; ++b)
        s.bands[b] = { apvts.getRawParameterValue (ParamID::eqFreq[b])->load(),
                       apvts.getRawParameterValue (ParamID::eqQ[b])->load(),
                       apvts.getRawParameterValue (ParamID::eqGain[b])->load(),
                       ParamID::eqType[b] != nullptr && apvts.getRawParameterValue (ParamID::eqType[b])->load() > 0.5f };

    bool changed = s.enabled != current.enabled;
    for (size_t b = 0; b < Equalizer::numBands; ++b)
        changed = changed || s.bands[b].freq != current.bands[b].freq || s.bands[b].q != current.bands[b].q
                          || s.bands[b].gainDb != current.bands[b].gainDb || s.bands[b].cut != current.bands[b].cut;

    if (changed || firstRefresh)
    {
        firstRefresh = false;
        current = s;
        for (int b = 0; b < Equalizer::numBands; ++b)
        {
            freqKnobs[b]->setDimmed (! s.enabled);
            qKnobs[b]->setDimmed (! s.enabled);
            levelKnobs[b]->setDimmed (! s.enabled || s.bands[(size_t) b].cut); // a cut has no level
        }
        updateBandLabels();
        onButton.setButtonText (s.enabled ? "ON" : "OFF");
        graph.repaint();
    }
}

void EqSection::paint (juce::Graphics& g)
{
    g.setColour (Theme::eq);
    g.setFont (Theme::font (12.5f, true));
    g.drawText ("EQ", 0, 2, 30, 16, juce::Justification::centredLeft);
}

void EqSection::resized()
{
    const int buttonsX = 34, onW = 40;
    const int buttonsW = getWidth() - buttonsX - onW - 10;
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        const int x0 = buttonsX + buttonsW * b / Equalizer::numBands;
        const int x1 = buttonsX + buttonsW * (b + 1) / Equalizer::numBands;
        bandButtons[(size_t) b].setBounds (x0, 0, x1 - x0 + (b < Equalizer::numBands - 1 ? 1 : 0), 20);
    }
    onButton.setBounds (getWidth() - onW, 0, onW, 20);

    constexpr int knobW = 58, top = 24;
    const int knobsX = getWidth() - 3 * knobW;
    graph.setBounds (0, top, knobsX - 8, Knob::preferredHeight - 3);
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        freqKnobs[b]->setBounds  (knobsX,             top, knobW, Knob::preferredHeight);
        qKnobs[b]->setBounds     (knobsX + knobW,     top, knobW, Knob::preferredHeight);
        levelKnobs[b]->setBounds (knobsX + 2 * knobW, top, knobW, Knob::preferredHeight);
    }
}

// ---- Graph --------------------------------------------------------------------------------------

juce::Rectangle<float> EqSection::Graph::plot() const { return getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (8.0f); }

float EqSection::Graph::freqToX (float hz) const
{
    const auto p = plot();
    return p.getX() + p.getWidth() * std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
}

float EqSection::Graph::xToFreq (float x) const
{
    const auto p = plot();
    return minHz * std::pow (maxHz / minHz, juce::jlimit (0.0f, 1.0f, (x - p.getX()) / p.getWidth()));
}

float EqSection::Graph::dbToY (float db) const
{
    const auto p = plot();
    return p.getCentreY() - juce::jlimit (-rangeDb, rangeDb, db) / rangeDb * p.getHeight() * 0.5f;
}

float EqSection::Graph::yToDb (float y) const
{
    const auto p = plot();
    return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * rangeDb;
}

juce::Point<float> EqSection::Graph::nodePosition (int band) const
{
    const auto& b = owner.current.bands[(size_t) band];
    return { freqToX (b.freq), dbToY (b.cut ? 0.0f : b.gainDb) }; // a cut's dot sits on the 0 dB line at its cutoff
}

int EqSection::Graph::bandAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDistance = 12.0f; // how close you need to click to grab a dot
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        const float d = nodePosition (b).getDistanceFrom (pos);
        if (d < bestDistance) { bestDistance = d; best = b; }
    }
    return best;
}

void EqSection::Graph::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto p = plot();
    const bool on = owner.current.enabled;
    const auto accent = on ? Theme::eq : Theme::dimmed;

    // Grid: 100 Hz, 1 kHz, 10 kHz, and the 0 dB line.
    g.setFont (Theme::font (9.0f));
    for (auto [hz, label] : { std::pair { 100.0f, "100" }, std::pair { 1000.0f, "1k" }, std::pair { 10000.0f, "10k" } })
    {
        const float x = freqToX (hz);
        g.setColour (Theme::line);
        g.drawVerticalLine (juce::roundToInt (x), p.getY(), p.getBottom());
        g.setColour (Theme::dimmed);
        g.drawText (label, juce::Rectangle<float> (x - 15.0f, p.getBottom() + 1.0f, 30.0f, 10.0f), juce::Justification::centred);
    }
    g.setColour (Theme::line);
    g.drawHorizontalLine (juce::roundToInt (dbToY (0.0f)), p.getX(), p.getRight());

    // The response curve, filled toward the 0 dB line.
    const double sr = juce::jmax (8000.0, (double) owner.sampleRate.load (std::memory_order_relaxed));
    auto settings = owner.current;
    settings.enabled = true; // when off, still show the (greyed) curve you'd get
    juce::Path line, fill;
    const float zeroY = dbToY (0.0f);
    constexpr int points = 96;
    for (int i = 0; i < points; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (points - 1);
        const float y = dbToY ((float) Equalizer::responseDb (settings, xToFreq (x), sr));
        if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, zeroY); }
        else        line.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (p.getRight(), zeroY);
    fill.closeSubPath();

    g.saveState();
    g.reduceClipRegion (p.expanded (0.0f, 2.0f).toNearestInt());
    g.setColour (accent.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (line, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();

    // One dot per band; the band the knobs are controlling is filled in.
    for (int b = 0; b < Equalizer::numBands; ++b)
    {
        const auto c = nodePosition (b);
        const bool isSelected = b == owner.selected;
        const float r = isSelected ? 4.5f : 3.5f;
        const auto dot = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
        g.setColour (isSelected ? accent : Theme::panel);
        g.fillEllipse (dot);
        g.setColour (accent);
        g.drawEllipse (dot, 1.5f);
    }
}

void EqSection::Graph::mouseDown (const juce::MouseEvent& e)
{
    dragBand = bandAt (e.position);
    if (dragBand < 0)
        return;
    owner.selectBand (dragBand);
    dragPoint = nodePosition (dragBand); // move the dot from where it is, not from where you clicked
    lastMouse = e.position;
    owner.param (ParamID::eqFreq[dragBand]).beginChangeGesture();
    dragLevel = ! owner.current.bands[(size_t) dragBand].cut; // a cut only moves left/right
    if (dragLevel)
        owner.param (ParamID::eqGain[dragBand]).beginChangeGesture();
}

void EqSection::Graph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;
    // Relative movement, 10x finer with Shift (same as the knobs).
    const float scale = e.mods.isShiftDown() ? 0.1f : 1.0f;
    const auto p = plot();
    dragPoint += (e.position - lastMouse) * scale;
    dragPoint = { juce::jlimit (p.getX(), p.getRight(), dragPoint.x), juce::jlimit (p.getY(), p.getBottom(), dragPoint.y) };
    lastMouse = e.position;
    owner.setParam (ParamID::eqFreq[dragBand], xToFreq (dragPoint.x));
    if (dragLevel)
        owner.setParam (ParamID::eqGain[dragBand], yToDb (dragPoint.y));
    owner.refresh();
}

void EqSection::Graph::mouseUp (const juce::MouseEvent&)
{
    if (dragBand < 0)
        return;
    owner.param (ParamID::eqFreq[dragBand]).endChangeGesture();
    if (dragLevel)
        owner.param (ParamID::eqGain[dragBand]).endChangeGesture();
    dragBand = -1;
}

void EqSection::Graph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int band = bandAt (e.position);
    if (band < 0 || owner.current.bands[(size_t) band].cut)
        return;
    auto& gain = owner.param (ParamID::eqGain[band]);
    gain.beginChangeGesture();
    gain.setValueNotifyingHost (gain.getDefaultValue());
    gain.endChangeGesture();
    owner.refresh();
}

void EqSection::Graph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Scroll changes the selected band's Q, in even steps on the knob's own scale (10x finer with Shift).
    // Windows turns Shift + wheel into a sideways scroll, so use whichever direction moved.
    const float delta = std::abs (wheel.deltaY) >= std::abs (wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
    auto& q = owner.param (ParamID::eqQ[owner.selected]);
    q.beginChangeGesture();
    q.setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, q.getValue() + delta * (e.mods.isShiftDown() ? 0.015f : 0.15f)));
    q.endChangeGesture();
    owner.refresh();
}

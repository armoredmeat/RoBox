#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    constexpr int designWidthMinusOutput = MainPanel::designWidth - 16 - 220; // left edge of the Output section
    // Layout grid (design pixels). See the C1 mockup.
    constexpr int pad = 16;
    constexpr int topBarY = 14, topBarH = 28;
    constexpr int mainY = 58, mainH = 304;
    constexpr int carrierX = pad, carrierW = 220;
    constexpr int voiceW = 176, voiceX = MainPanel::designWidth - pad - voiceW;
    constexpr int displayX = carrierX + carrierW + 16, displayW = voiceX - 16 - displayX;
    constexpr int dividerY = mainY + mainH + 24; // leaves ~13 px under the Detune/Noise labels
    constexpr int bottomY = dividerY + 10;
    constexpr int knobCellW = 72;
    constexpr int statusDividerY = bottomY + 110;             // a line under EFX / EQ / Output...
    constexpr int statusY = statusDividerY + 5, statusH = 16; // ...and one line of help text under it
    static_assert (statusY + statusH + 8 <= MainPanel::designHeight);
    constexpr int eqX = designWidthMinusOutput - 16 - EqSection::preferredWidth; // EQ sits between EFX and Output

    const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const juce::StringArray chordWords { "", "octave", "fifth", "oct + 5th", "minor", "major", "power", "sus4" };
    const juce::StringArray chordMenu { "No chord", "Octave", "Fifth", "Oct + 5th", "Minor", "Major", "Power", "Sus4" };

    juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        auto* p = s.getParameter (id);
        jassert (p != nullptr);
        return *p;
    }

    void drawHeading (juce::Graphics& g, const juce::String& text, juce::Colour colour, int x, int y, int w = 120)
    {
        g.setColour (colour);
        g.setFont (Theme::font (12.5f, true));
        g.drawText (text, x, y, w, 16, juce::Justification::centredLeft);
    }
}

MainPanel::MainPanel (RoBoxAudioProcessor& p)
    : processor (p),
      apvts (p.apvts),
      modeSelector (param (apvts, ParamID::mode), { "Vocoder", "Talkbox" }, Theme::voice, Theme::voiceInk, true),
      presetBar (p.presets),
      waveformSelector (param (apvts, ParamID::waveform), { "Saw", "Sqr", "Pulse", "Super" }, Theme::carrier, Theme::carrierInk, false),
      keyboard (param (apvts, ParamID::note)),
      octaveSelector (param (apvts, ParamID::octave), { "0", "1", "2", "3", "4", "5", "6" }, Theme::carrier, Theme::carrierInk, false, 2),
      detune     (apvts, ParamID::detune,     "Detune",      Theme::carrier, Format::percent),
      pulseWidth (apvts, ParamID::pulseWidth, "Width",       Theme::carrier, Format::percent),
      noise      (apvts, ParamID::noise,      "Noise",       Theme::carrier, Format::percent),
      liveDisplay (p.display),
      bands      (apvts, ParamID::bands,      "Bands",       Theme::voice, [] (double v) { return ParamChoices::bands[juce::jlimit (0, ParamChoices::bands.size() - 1, (int) std::lround (v))]; }),
      formant    (apvts, ParamID::formant,    "Formant",     Theme::voice, Format::semitones, Knob::Arc::fromCentre),
      attack     (apvts, ParamID::attack,     "Attack",      Theme::voice, Format::ms),
      release    (apvts, ParamID::release,    "Release",     Theme::voice, Format::ms),
      sibilance  (apvts, ParamID::sibilance,  "Sibilance",   Theme::voice, Format::percent),
      tube       (apvts, ParamID::tube,       "Tube",        Theme::voice, Format::percent),
      drive      (apvts, ParamID::drive,      "Drive",       Theme::efx, Format::percent),
      bitCrush   (apvts, ParamID::crushBits,  "Bit Crush",   Theme::efx, Format::bits, Knob::Arc::fromEnd),
      sampleRate (apvts, ParamID::crushRate,  "Sample Rate", Theme::efx, Format::rate, Knob::Arc::fromEnd),
      mix        (apvts, ParamID::mix,        "Mix",         Theme::output, Format::percent),
      gain       (apvts, ParamID::outputGain, "Gain",        Theme::output, Format::decibels),
      meter (p.display),
      eq (p.apvts, p.display.sampleRate)
{
    for (auto* c : std::initializer_list<juce::Component*> {
             &titleArea, &modeSelector, &presetBar, &themeButton, &waveformSelector, &midiButton, &soloButton, &keyboard, &octaveSelector,
             &keyBox, &chordBox, &detune, &pulseWidth, &noise, &liveDisplay,
             &bands, &formant, &attack, &release, &sibilance, &tube,
             &drive, &bitCrush, &sampleRate, &mix, &gain, &meter, &eq })
        addAndMakeVisible (c);

    for (auto* b : { &midiButton, &soloButton })
        b->setClickingTogglesState (true);
    lookAndFeelChanged();
    midiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamID::midiFollow, midiButton);
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, ParamID::carrierSolo, soloButton);

    // Combo box item IDs must be 1..N in parameter order for the attachments.
    for (int i = 0; i < noteNames.size(); ++i)  keyBox.addItem ("Key " + noteNames[i], i + 1);
    for (int i = 0; i < chordMenu.size(); ++i)  chordBox.addItem (chordMenu[i], i + 1);
    keyAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamID::note, keyBox);
    chordAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, ParamID::chord, chordBox);

    setHelpTexts();
    addMouseListener (this, true); // hear mouse movement over every child control too

    updateModeDependentControls();
    timerCallback();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    removeMouseListener (this);
    stopTimer();
}

void MainPanel::setHelpTexts()
{
    // "Name: what it does". The part before the colon is shown in bold.
    Help::set (titleArea,        "RoBox " ROBOX_VERSION ": a vocoder and talkbox with a built-in synth, for electro and bass music.");
    Help::set (modeSelector,     "Mode: Vocoder is the classic robot voice. Talkbox shapes the synth with the vowel shape of your mouth, like a real talkbox.");
    Help::set (waveformSelector, "Waveform: the synth sound your voice shapes. Saw is bright, Square is hollow, Pulse is thin and nasal, Super is thick and wide.");
    Help::set (midiButton,       "MIDI Follow: when on, MIDI notes sent to this track set the pitch. The light shows while a note is held.");
    Help::set (soloButton,       "Carrier Solo: hear only the synth, to check its pitch and tone. Turn it off to hear the effect again.");
    Help::set (keyboard,         "Key: click a key to set the root note of the synth.");
    Help::set (octaveSelector,   "Octave: how low or high the synth plays. 1 to 2 suits deep bass voices, 3 to 4 a normal voice.");
    Help::set (keyBox,           "Key: the root note of the synth. Same as clicking the keyboard.");
    Help::set (chordBox,         "Chord: adds notes above the root (an octave, a fifth, a minor chord...) for a fuller voice.");
    Help::set (detune,           "Detune: spreads the 7 voices of the Super waveform apart for a wider, thicker sound. Only used by Super.");
    Help::set (pulseWidth,       "Width: how narrow the pulse wave is. 50% is a square wave; lower sounds thinner and more nasal.");
    Help::set (noise,            "Noise: blends hiss into the synth so consonants like s, t and f come through clearly.");
    Help::set (liveDisplay,      "Display: the band levels of the vocoder, or the vowel shape of the talkbox, moving with your voice.");
    Help::set (bands,            "Bands: how many frequency bands the vocoder uses. Fewer is more robotic, more is clearer. Vocoder only.");
    Help::set (formant,          "Formant: makes the voice sound smaller (up) or bigger (down) without changing the pitch.");
    Help::set (attack,           "Attack: how fast the effect reacts when a sound starts. Short is snappy, long is softer.");
    Help::set (release,          "Release: how long the effect hangs on after a sound stops. Short is choppy, long is smooth.");
    Help::set (sibilance,        "Sibilance: lets the real s, t and sh sounds of your voice through, so words stay clear.");
    Help::set (tube,             "Tube: the warm, gritty, mid-range sound of the driver and tube of a real talkbox. Talkbox only.");
    Help::set (drive,            "Drive: saturation on the effect, from warm to fuzzy. Loudness stays about the same.");
    Help::set (bitCrush,         "Bit Crush: fewer bits for a crunchy, digital lo-fi sound. 16 bit is off.");
    Help::set (sampleRate,       "Sample Rate: lowers the sample rate for a metallic, old-sampler sound. Off is full quality.");
    Help::set (mix,              "Mix: the balance between your dry voice (0%) and the effect (100%).");
    Help::set (gain,             "Gain: the overall output volume.");
    Help::set (meter,            "Output meter: turns red when the output is close to clipping. Turn Gain down if it does.");
}

void MainPanel::mouseEnter (const juce::MouseEvent& e) { showHelpFor (e.eventComponent); }
void MainPanel::mouseMove (const juce::MouseEvent& e)  { showHelpFor (e.eventComponent); }
void MainPanel::mouseExit (const juce::MouseEvent&)    { showHelpFor (nullptr); } // the next mouseEnter fills it in again

void MainPanel::showHelpFor (const juce::Component* c)
{
    const auto text = Help::find (c);
    if (text != statusText)
    {
        statusText = text;
        repaint (pad, statusY, designWidth - 2 * pad, statusH);
    }
}

void MainPanel::lookAndFeelChanged() // also called when the theme changes
{
    for (auto* b : { &midiButton, &soloButton })
    {
        b->setColour (juce::TextButton::buttonOnColourId, Theme::carrier);
        b->setColour (juce::TextButton::textColourOnId, Theme::carrierInk);
    }
}

void MainPanel::recolour()
{
    seenThemeVersion = Theme::version();
    if (auto* lf = dynamic_cast<RoBoxLookAndFeel*> (&getLookAndFeel()))
        lf->applyTheme();

    // Tell every control (and the window behind the panel) to pick up the new colours.
    auto* top = getParentComponent() != nullptr ? getParentComponent() : this;
    top->sendLookAndFeelChange();
    top->repaint();
}

void MainPanel::updateModeDependentControls()
{
    const bool talkboxMode = modeSelector.getSelectedIndex() == 1;
    bands.setDimmed (talkboxMode);   // band count only matters to the vocoder
    tube.setDimmed (! talkboxMode);  // Tube only matters to the talkbox

    // One knob slot under the keyboard: Width for Pulse, otherwise Detune (lit for Supersaw only).
    const int wave = waveformSelector.getSelectedIndex();
    pulseWidth.setVisible (wave == 2);
    detune.setVisible (wave != 2);
    detune.setDimmed (wave != 3);
}

void MainPanel::timerCallback()
{
    if (Theme::version() != seenThemeVersion) // changed here or in another RoBox window
        recolour();

    updateModeDependentControls();

    // Readout + keyboard: the note actually playing (follows MIDI when MIDI Follow has a held note),
    // otherwise the Key/Octave you've set.
    midiNoteHeld = processor.midiNoteActive.load();
    const int dialNote = (int) apvts.getRawParameterValue (ParamID::note)->load();
    const int dialOctave = (int) apvts.getRawParameterValue (ParamID::octave)->load();
    const int midiNote = midiNoteHeld ? processor.currentCarrierNote.load() : (dialOctave + 1) * 12 + dialNote;

    keyboard.setShownKey (midiNote % 12);
    readoutNote  = noteNames[midiNote % 12] + juce::String (midiNote / 12 - 1);
    readoutChord = chordWords[juce::jlimit (0, chordWords.size() - 1, (int) apvts.getRawParameterValue (ParamID::chord)->load())];
    const float hz = 440.0f * std::exp2 ((float) (midiNote - 69) / 12.0f);
    readoutHz = juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz";

    liveDisplay.update (apvts.getRawParameterValue (ParamID::formant)->load());
    meter.update();
    eq.refresh();
    presetBar.refresh();
    repaint (carrierX, mainY, carrierW, 110); // readout + MIDI light
}

void MainPanel::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    // Top bar: name on the left (the mode toggle and preset bar are components).
    g.setColour (Theme::text);
    g.setFont (Theme::font (18.0f, true));
    g.drawText ("RoBox", pad, topBarY, 90, topBarH, juce::Justification::centredLeft);

    // Section headings.
    drawHeading (g, "Carrier", Theme::carrier, carrierX, mainY);
    drawHeading (g, "Voice", Theme::voice, voiceX, mainY);
    drawHeading (g, "EFX", Theme::efx, pad, bottomY);
    drawHeading (g, "Output", Theme::output, designWidth - pad - 220 + 16, bottomY);

    // MIDI light: lit while a held MIDI note is setting the pitch.
    const auto led = juce::Rectangle<float> ((float) (carrierX + carrierW - 104), (float) mainY + 4.0f, 8.0f, 8.0f);
    g.setColour (midiNoteHeld ? Theme::carrier : Theme::line);
    g.fillEllipse (led);

    // Key readout: e.g. "F2 minor" with the frequency on the right.
    const int readoutY = mainY + 52;
    g.setColour (Theme::text);
    g.setFont (Theme::font (20.0f, true));
    g.drawText (readoutNote + (readoutChord.isEmpty() ? "" : " " + readoutChord), carrierX, readoutY, carrierW - 70, 26, juce::Justification::centredLeft);
    g.setColour (Theme::subtext);
    g.setFont (Theme::font (11.0f));
    g.drawText (readoutHz, carrierX + carrierW - 70, readoutY, 70, 26, juce::Justification::centredRight);

    g.drawText ("Octave", carrierX, mainY + 162, 80, 14, juce::Justification::centredLeft);

    // Dividers.
    g.setColour (Theme::line);
    g.drawHorizontalLine (dividerY, (float) pad, (float) (designWidth - pad));
    g.drawVerticalLine (eqX - 16, (float) bottomY, (float) (bottomY + 104));
    g.drawVerticalLine (designWidth - pad - 220, (float) bottomY, (float) (bottomY + 104));
    g.drawHorizontalLine (statusDividerY, (float) pad, (float) (designWidth - pad));

    // Status bar: help for whatever the mouse is over. "Name: description", with the name in bold.
    const auto status = juce::Rectangle<int> (pad, statusY, designWidth - 2 * pad, statusH);
    if (statusText.isEmpty())
    {
        g.setColour (Theme::dimmed);
        g.setFont (Theme::font (11.5f));
        g.drawText ("RoBox " ROBOX_VERSION ".  Point at any control to see what it does. Hold Shift while turning a knob for fine control.", status, juce::Justification::centredLeft);
    }
    else
    {
        const int colon = statusText.indexOf (": ");
        const auto name = colon > 0 ? statusText.substring (0, colon + 1) : juce::String();
        const auto description = colon > 0 ? statusText.substring (colon + 2) : statusText;
        const auto bold = Theme::font (11.5f, true);
        const int nameW = name.isEmpty() ? 0 : juce::roundToInt (juce::GlyphArrangement::getStringWidth (bold, name)) + 6;
        g.setColour (Theme::text);
        g.setFont (bold);
        g.drawText (name, status.withWidth (nameW), juce::Justification::centredLeft);
        g.setColour (Theme::subtext);
        g.setFont (Theme::font (11.5f));
        g.drawText (description, status.withTrimmedLeft (nameW), juce::Justification::centredLeft, true);
    }
}

void MainPanel::resized()
{
    // Top bar
    titleArea.setBounds (pad, topBarY, 80, topBarH);
    modeSelector.setBounds (pad + 90, topBarY + 2, 180, topBarH - 4);
    themeButton.setBounds (modeSelector.getRight() + 10, topBarY + 2, topBarH - 4, topBarH - 4);
    presetBar.setBounds (designWidth - pad - 304, topBarY + 2, 304, topBarH - 4);

    // Carrier column
    midiButton.setBounds (carrierX + carrierW - 92, mainY - 2, 44, 20);
    soloButton.setBounds (carrierX + carrierW - 44, mainY - 2, 44, 20);
    waveformSelector.setBounds (carrierX, mainY + 24, carrierW, 22);
    keyboard.setBounds (carrierX, mainY + 82, carrierW, 76);
    octaveSelector.setBounds (carrierX, mainY + 178, carrierW, 22);
    keyBox.setBounds (carrierX, mainY + 208, carrierW / 2 - 3, 24);
    chordBox.setBounds (carrierX + carrierW / 2 + 3, mainY + 208, carrierW / 2 - 3, 24);
    const int carrierKnobY = mainY + 240;
    const int slotW = carrierW / 2;
    detune.setBounds (carrierX, carrierKnobY, slotW, Knob::preferredHeight);
    pulseWidth.setBounds (detune.getBounds());
    noise.setBounds (carrierX + slotW, carrierKnobY, slotW, Knob::preferredHeight);

    // Centre display
    liveDisplay.setBounds (displayX, mainY, displayW, mainH);

    // Voice column: 2 x 3 grid
    const int cellW = voiceW / 2, firstRowY = mainY + 24, rowH = Knob::preferredHeight + 18;
    Knob* voiceKnobs[] = { &bands, &formant, &attack, &release, &sibilance, &tube };
    for (int i = 0; i < 6; ++i)
        voiceKnobs[i]->setBounds (voiceX + (i % 2) * cellW, firstRowY + (i / 2) * rowH, cellW, Knob::preferredHeight);

    // Bottom row: EFX on the left, Output on the right
    const int knobY = bottomY + 22;
    Knob* efxKnobs[] = { &drive, &bitCrush, &sampleRate };
    for (int i = 0; i < 3; ++i)
        efxKnobs[i]->setBounds (pad + i * (knobCellW + 8), knobY, knobCellW, Knob::preferredHeight);

    const int outX = designWidth - pad - 220 + 16;
    mix.setBounds (outX, knobY, knobCellW, Knob::preferredHeight);
    gain.setBounds (outX + knobCellW + 8, knobY, knobCellW, Knob::preferredHeight);
    meter.setBounds (outX + 2 * (knobCellW + 8) + 4, knobY, 16, Knob::preferredHeight - 14);

    eq.setBounds (eqX, bottomY - 2, EqSection::preferredWidth, EqSection::preferredHeight);
}

// ---------------------------------------------------------------------------------------------

RoBoxAudioProcessorEditor::RoBoxAudioProcessorEditor (RoBoxAudioProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);

    // Resizable, keeping the design's proportions, from 75 % to 200 % size.
    setResizable (true, true);
    constexpr double w = MainPanel::designWidth, h = MainPanel::designHeight;
    setResizeLimits ((int) (w * 0.75), (int) (h * 0.75), (int) (w * 2.0), (int) (h * 2.0));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (w / h);
    setSize ((int) w, (int) h);
}

RoBoxAudioProcessorEditor::~RoBoxAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void RoBoxAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);
}

void RoBoxAudioProcessorEditor::resized()
{
    // Lay the panel out at design size, then scale it to fill the window.
    panel.setBounds (0, 0, MainPanel::designWidth, MainPanel::designHeight);
    const float scale = (float) getWidth() / (float) MainPanel::designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}

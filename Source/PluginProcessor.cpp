#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

RoBoxAudioProcessor::RoBoxAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RoBoxState", createParameterLayout())
{
    outputGainParam  = apvts.getRawParameterValue (ParamID::outputGain);
    waveformParam    = apvts.getRawParameterValue (ParamID::waveform);
    pulseWidthParam  = apvts.getRawParameterValue (ParamID::pulseWidth);
    detuneParam      = apvts.getRawParameterValue (ParamID::detune);
    noteParam        = apvts.getRawParameterValue (ParamID::note);
    octaveParam      = apvts.getRawParameterValue (ParamID::octave);
    chordParam       = apvts.getRawParameterValue (ParamID::chord);
    noiseParam       = apvts.getRawParameterValue (ParamID::noise);
    carrierSoloParam = apvts.getRawParameterValue (ParamID::carrierSolo);
    modeParam        = apvts.getRawParameterValue (ParamID::mode);
    bandsParam       = apvts.getRawParameterValue (ParamID::bands);
    formantParam     = apvts.getRawParameterValue (ParamID::formant);
    attackParam      = apvts.getRawParameterValue (ParamID::attack);
    releaseParam     = apvts.getRawParameterValue (ParamID::release);
    sibilanceParam   = apvts.getRawParameterValue (ParamID::sibilance);
    driveParam       = apvts.getRawParameterValue (ParamID::drive);
    crushBitsParam   = apvts.getRawParameterValue (ParamID::crushBits);
    crushRateParam   = apvts.getRawParameterValue (ParamID::crushRate);
    mixParam         = apvts.getRawParameterValue (ParamID::mix);
    midiFollowParam  = apvts.getRawParameterValue (ParamID::midiFollow);
    tubeParam        = apvts.getRawParameterValue (ParamID::tube);
    eqOnParam        = apvts.getRawParameterValue (ParamID::eqOn);
    for (size_t b = 0; b < Equalizer::numBands; ++b)
    {
        eqFreqParams[b] = apvts.getRawParameterValue (ParamID::eqFreq[b]);
        eqQParams[b]    = apvts.getRawParameterValue (ParamID::eqQ[b]);
        eqGainParams[b] = apvts.getRawParameterValue (ParamID::eqGain[b]);
        if (ParamID::eqType[b] != nullptr)
            eqTypeParams[b] = apvts.getRawParameterValue (ParamID::eqType[b]);
    }
}

void RoBoxAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    display.sampleRate.store ((float) sampleRate);
    const juce::dsp::ProcessSpec spec { sampleRate,
                                        static_cast<juce::uint32> (samplesPerBlock),
                                        static_cast<juce::uint32> (getTotalNumOutputChannels()) };
    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02); // smooths knob moves so they don't click
    outputGain.setGainDecibels (outputGainParam->load());

    carrier.prepare (sampleRate);
    vocoder.prepare (sampleRate);
    talkbox.prepare (sampleRate); // allocates its analysis buffers here, not on the audio thread
    wasTalkboxMode = (int) modeParam->load() == 1;
    modeFadeLength = juce::jmax (1, juce::roundToInt (sampleRate * modeFadeSeconds));
    modeFadeRemaining = 0;
    inputHighpass.setHighpass (sampleRate, 70.0, 0.707); // removes mic rumble before analysis
    inputHighpass.reset();
    character.prepare (sampleRate);
    equalizer.setSettings (readEqSettings());
    equalizer.prepare (sampleRate);
    heldNotes.clear();

    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::sin3dB); // equal-power: no volume dip at 50 %
    mixer.setWetMixProportion (mixParam->load() / 100.0f);
    mixer.reset();

    // Allocate here, never in processBlock.
    scratch.setSize (numScratchChannels, juce::jmax (1, samplesPerBlock));
}

bool RoBoxAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RoBoxAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals; // stops tiny numbers from eating CPU

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Read the carrier controls once per block. The pitch is set per segment below,
    // because MIDI notes can arrive anywhere inside the block.
    CarrierOsc::Settings cs;
    cs.waveform   = (int) waveformParam->load();
    cs.pulseWidth = pulseWidthParam->load() / 100.0f;
    cs.detune     = detuneParam->load() / 100.0f;
    cs.chord      = (int) chordParam->load();
    cs.noise      = noiseParam->load() / 100.0f;

    const int dialNote = CarrierOsc::noteToMidi ((int) noteParam->load(), (int) octaveParam->load());
    const bool followMidi = midiFollowParam->load() > 0.5f;

    // Held keys always get tracked, so switching MIDI Follow on while holding a key works at once.
    auto applyPitch = [&]
    {
        const int note = followMidi ? heldNotes.currentNoteOr (dialNote) : dialNote;
        cs.midiNote = (float) note;
        carrier.setSettings (cs);
        currentCarrierNote.store (note);
        midiNoteActive.store (followMidi && ! heldNotes.isEmpty());
    };

    auto handleMidi = [this] (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
            heldNotes.noteOn (m.getNoteNumber());
        else if (m.isNoteOff())
            heldNotes.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            heldNotes.clear();
    };

    VocoderEngine::Settings vs;
    vs.numBands         = ParamChoices::bands[(int) bandsParam->load()].getIntValue();
    vs.formantSemitones = formantParam->load();
    vs.attackMs         = attackParam->load();
    vs.releaseMs        = releaseParam->load();
    vs.sibilance        = sibilanceParam->load() / 100.0f;
    vocoder.setSettings (vs);

    TalkboxEngine::Settings ts;
    ts.formantSemitones = vs.formantSemitones;
    ts.attackMs         = vs.attackMs;
    ts.releaseMs        = vs.releaseMs;
    ts.sibilance        = vs.sibilance;
    ts.tube             = tubeParam->load() / 100.0f;
    talkbox.setSettings (ts);

    // Normally only the active engine runs. On a switch, clear the one taking over so it starts
    // fresh (instead of releasing old sound from the last time it was used), and keep the old
    // one running for 30 ms while it fades out, so the switch doesn't click.
    const bool talkboxMode = (int) modeParam->load() == 1;
    if (talkboxMode != wasTalkboxMode)
    {
        if (talkboxMode) talkbox.restart(); // it has kept listening, so it starts on the right vowel
        else             vocoder.reset();
        wasTalkboxMode = talkboxMode;
        modeFadeRemaining = modeFadeLength;
    }

    Character::Settings chs;
    chs.drive  = driveParam->load() / 100.0f;
    chs.bits   = crushBitsParam->load();
    chs.rateHz = crushRateParam->load();
    character.setSettings (chs);

    equalizer.setSettings (readEqSettings());

    mixer.setWetMixProportion (mixParam->load() / 100.0f);

    const bool solo = carrierSoloParam->load() > 0.5f;
    const int numSamples  = buffer.getNumSamples();
    const int numInputs   = getTotalNumInputChannels();
    const int numChannels = getTotalNumOutputChannels();
    const int chunkSize   = scratch.getNumSamples();
    constexpr float soloLevel = 0.25f; // about -12 dBFS, so soloing isn't a jump scare

    auto* carrierData = scratch.getWritePointer (carrierCh);
    auto* voiceData   = scratch.getWritePointer (voiceCh);
    auto* wetData     = scratch.getWritePointer (wetCh);
    auto* fadeData    = scratch.getWritePointer (fadeCh);
    double voiceSumSquares = 0.0;

    // Some hosts send blocks larger than promised, so work in chunks that fit the scratch buffer.
    for (int start = 0; chunkSize > 0 && start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);

        // 1. Voice: mix the input channels down to mono and remove rumble.
        juce::FloatVectorOperations::clear (voiceData, n);
        for (int ch = 0; ch < numInputs; ++ch)
            juce::FloatVectorOperations::addWithMultiply (voiceData, buffer.getReadPointer (ch, start), 1.0f / (float) numInputs, n);
        for (int i = 0; i < n; ++i)
        {
            voiceData[i] = inputHighpass.process (voiceData[i]);
            voiceSumSquares += (double) voiceData[i] * voiceData[i]; // for the live display
        }

        // 2. Carrier synth. It always runs so the vocoder keeps its state when you toggle Solo.
        //    Render it in segments split at each MIDI event, so a note starts on the exact sample
        //    it was played rather than at the start of the next block.
        {
            auto event = midi.findNextSamplePosition (start);
            int pos = 0;
            while (pos < n)
            {
                while (event != midi.cend() && (*event).samplePosition <= start + pos)
                {
                    handleMidi ((*event).getMessage());
                    ++event;
                }

                const int segmentEnd = event != midi.cend() ? juce::jmin (n, (*event).samplePosition - start) : n;
                applyPitch();
                carrier.process (carrierData + pos, segmentEnd - pos);
                pos = segmentEnd;
            }
        }

        // 3. The voice shapes the carrier: vocoder or talkbox.
        if (talkboxMode)
            talkbox.process (voiceData, carrierData, wetData, n);
        else
            vocoder.process (voiceData, carrierData, wetData, n);

        if (! talkboxMode && modeFadeRemaining == 0)
            talkbox.listen (voiceData, n);

        if (modeFadeRemaining > 0)
        {
            // The engine we just left, fading out (equal-power, so the level doesn't dip midway).
            if (talkboxMode)
                vocoder.process (voiceData, carrierData, fadeData, n);
            else
                talkbox.process (voiceData, carrierData, fadeData, n);

            for (int i = 0; i < n && modeFadeRemaining > 0; ++i, --modeFadeRemaining)
            {
                const float t = (float) modeFadeRemaining / (float) modeFadeLength; // 1 -> 0
                const float angle = t * juce::MathConstants<float>::halfPi;
                wetData[i] = wetData[i] * std::cos (angle) + fadeData[i] * std::sin (angle);
            }
        }

        // 4. Character: drive, then bit crush, on the wet signal only.
        character.process (wetData, n);

        // 5. EQ: shapes the finished robot voice (the dry voice isn't touched).
        equalizer.process (wetData, n);

        // 6. Mix: keep a copy of the original (dry) audio, write the wet signal over it,
        //    then blend the two. The dry side keeps its stereo image; the wet side is mono.
        auto chunk = juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) start, (size_t) n);
        mixer.pushDrySamples (chunk);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (solo)
                juce::FloatVectorOperations::multiply (buffer.getWritePointer (ch, start), carrierData, soloLevel, n);
            else
                juce::FloatVectorOperations::copy (buffer.getWritePointer (ch, start), wetData, n);
        }

        if (! solo) // Solo is a tuning aid: you hear only the carrier, whatever Mix is set to.
            mixer.mixWetSamples (chunk);
    }

    // A badly behaved host can send events stamped past the end of the block; don't lose their note-offs.
    for (auto event = midi.findNextSamplePosition (numSamples); event != midi.cend(); ++event)
        handleMidi ((*event).getMessage());
    midi.clear(); // RoBox is an audio effect: don't pass MIDI on

    outputGain.setGainDecibels (outputGainParam->load());
    juce::dsp::AudioBlock<float> block (buffer);
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    publishDisplayData (talkboxMode, voiceSumSquares, numSamples, buffer);
}

Equalizer::Settings RoBoxAudioProcessor::readEqSettings() const
{
    Equalizer::Settings s;
    s.enabled = eqOnParam->load() > 0.5f;
    for (size_t b = 0; b < Equalizer::numBands; ++b)
        s.bands[b] = { eqFreqParams[b]->load(), eqQParams[b]->load(), eqGainParams[b]->load(),
                       eqTypeParams[b] != nullptr && eqTypeParams[b]->load() > 0.5f };
    return s;
}

void RoBoxAudioProcessor::publishDisplayData (bool talkboxMode, double voiceSumSquares, int numSamples,
                                              const juce::AudioBuffer<float>& buffer)
{
    // Plain atomic stores: cheap, never block, and the UI reads them ~30 times a second.
    // A UI frame might mix two blocks' values, which is harmless for a display.
    constexpr auto relaxed = std::memory_order_relaxed;

    display.talkboxMode.store (talkboxMode, relaxed);
    display.voiceRms.store ((float) std::sqrt (voiceSumSquares / juce::jmax (1, numSamples)), relaxed);

    if (talkboxMode)
    {
        const auto& k = talkbox.getReflectionCoefficients();
        for (size_t i = 0; i < k.size(); ++i)
            display.lpc[i].store (k[i], relaxed);
        display.lpcOrder.store (talkbox.getOrder(), relaxed);
    }
    else
    {
        const int bands = vocoder.getNumBands();
        for (int b = 0; b < bands; ++b)
            display.bandLevels[(size_t) b].store (vocoder.getBandLevel (b), relaxed);
        display.numBands.store (bands, relaxed);
    }

    // Peak since the UI last looked (the UI resets these to 0 when it reads them).
    auto keepMax = [] (std::atomic<float>& a, float v) { if (v > a.load (relaxed)) a.store (v, relaxed); };
    const int chans = buffer.getNumChannels();
    keepMax (display.outPeakLeft,  chans > 0 ? buffer.getMagnitude (0, 0, numSamples) : 0.0f);
    keepMax (display.outPeakRight, chans > 1 ? buffer.getMagnitude (1, 0, numSamples) : display.outPeakLeft.load (relaxed));
}

juce::AudioProcessorEditor* RoBoxAudioProcessor::createEditor()
{
    return new RoBoxAudioProcessorEditor (*this);
}

void RoBoxAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RoBoxAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.stateRestored();
        }
}

// JUCE calls this to create the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoBoxAudioProcessor();
}

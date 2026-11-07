#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Every RoBox parameter lives here. The string IDs are saved inside DAW projects
// and presets, so never rename one after release; add new ones instead.
namespace ParamID
{
    // Mode
    inline constexpr const char* mode        = "mode";
    // Carrier
    inline constexpr const char* waveform    = "waveform";
    inline constexpr const char* pulseWidth  = "pulseWidth";
    inline constexpr const char* detune      = "detune";
    inline constexpr const char* note        = "note";
    inline constexpr const char* octave      = "octave";
    inline constexpr const char* chord       = "chord";
    inline constexpr const char* noise       = "noise";
    inline constexpr const char* midiFollow  = "midiFollow";
    inline constexpr const char* carrierSolo = "carrierSolo"; // hear only the carrier (tuning aid)
    // Voice
    inline constexpr const char* bands       = "bands";
    inline constexpr const char* formant     = "formant";
    inline constexpr const char* attack      = "attack";
    inline constexpr const char* release     = "release";
    inline constexpr const char* sibilance   = "sibilance";
    inline constexpr const char* tube        = "tube";        // Talkbox only
    // Character
    inline constexpr const char* drive       = "drive";
    inline constexpr const char* crushBits   = "crushBits";
    inline constexpr const char* crushRate   = "crushRate";
    // EQ (on the wet signal, after EFX)
    inline constexpr const char* eqOn        = "eqOn";
    inline constexpr const char* eqLowType   = "eqLowType";   // Shelf or Cut
    inline constexpr const char* eqHighType  = "eqHighType";  // Shelf or Cut
    inline constexpr const char* eqLowFreq   = "eqLowFreq";
    inline constexpr const char* eqLowQ      = "eqLowQ";
    inline constexpr const char* eqLowGain   = "eqLowGain";
    inline constexpr const char* eqMidFreq   = "eqMidFreq";
    inline constexpr const char* eqMidQ      = "eqMidQ";
    inline constexpr const char* eqMidGain   = "eqMidGain";
    inline constexpr const char* eqHighFreq  = "eqHighFreq";
    inline constexpr const char* eqHighQ     = "eqHighQ";
    inline constexpr const char* eqHighGain  = "eqHighGain";

    // The three EQ bands' IDs in order (Low Shelf, Mid, High Shelf), for loops.
    inline constexpr const char* eqFreq[] { eqLowFreq, eqMidFreq, eqHighFreq };
    inline constexpr const char* eqQ[]    { eqLowQ,    eqMidQ,    eqHighQ };
    inline constexpr const char* eqGain[] { eqLowGain, eqMidGain, eqHighGain };
    inline constexpr const char* eqType[] { eqLowType, nullptr,   eqHighType }; // the Mid band is always a bell
    // Output
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* outputGain  = "outputGain";
}

namespace ParamChoices
{
    inline const juce::StringArray modes     { "Vocoder", "Talkbox" };
    inline const juce::StringArray waveforms { "Saw", "Square", "Pulse", "Supersaw" };
    inline const juce::StringArray notes     { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray chords    { "Off", "Octave", "Fifth", "Oct+5th", "Minor", "Major", "Power", "Sus4" };
    inline const juce::StringArray bands     { "8", "12", "16", "20", "24", "32" };
    inline const juce::StringArray eqTypes   { "Shelf", "Cut" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (const char* id, const char* name, float defaultValue)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultValue,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };

    auto ms = [] (const char* id, const char* name, float lo, float hi, float defaultValue)
    {
        NormalisableRange<float> range (lo, hi, 0.01f);
        range.setSkewForCentre (std::sqrt (lo * hi)); // feels natural for time values
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue,
                                                      AudioParameterFloatAttributes().withLabel ("ms"));
    };

    // Mode
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode", ParamChoices::modes, 0));

    // Carrier
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::waveform, 1 }, "Waveform", ParamChoices::waveforms, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::pulseWidth, 1 }, "Pulse Width",
                                                       NormalisableRange<float> (5.0f, 95.0f, 0.1f), 50.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (percent (ParamID::detune, "Detune", 30.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::note, 1 }, "Note", ParamChoices::notes, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamID::octave, 1 }, "Octave", 0, 6, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::chord, 1 }, "Chord", ParamChoices::chords, 0));
    layout.add (percent (ParamID::noise, "Noise", 10.0f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::midiFollow, 1 }, "MIDI Follow", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::carrierSolo, 1 }, "Carrier Solo", false));

    // Voice
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bands, 1 }, "Bands", ParamChoices::bands, 2));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::formant, 1 }, "Formant Shift",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("st")));
    layout.add (ms (ParamID::attack, "Attack", 0.5f, 100.0f, 5.0f));
    layout.add (ms (ParamID::release, "Release", 5.0f, 500.0f, 60.0f));
    layout.add (percent (ParamID::sibilance, "Sibilance", 30.0f));
    layout.add (percent (ParamID::tube, "Tube", 60.0f)); // guitar talkbox voicing; 0 = clean LPC talkbox

    // Character
    layout.add (percent (ParamID::drive, "Drive", 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::crushBits, 1 }, "Bit Crush",
                                                       NormalisableRange<float> (4.0f, 16.0f, 0.01f), 16.0f,
                                                       AudioParameterFloatAttributes().withLabel ("bits")));
    {
        NormalisableRange<float> rateRange (4000.0f, 48000.0f, 1.0f);
        rateRange.setSkewForCentre (12000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::crushRate, 1 }, "Sample Rate", rateRange, 48000.0f,
                                                           AudioParameterFloatAttributes().withLabel ("Hz")));
    }

    // EQ
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::eqOn, 1 }, "EQ On", true));
    {
        auto hz = [] (const char* id, const char* name, float lo, float hi, float defaultValue)
        {
            NormalisableRange<float> range (lo, hi, 0.1f);
            range.setSkewForCentre (std::sqrt (lo * hi)); // even spacing per octave
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue,
                                                          AudioParameterFloatAttributes().withLabel ("Hz"));
        };
        auto q = [] (const char* id, const char* name, float hi, float defaultValue)
        {
            NormalisableRange<float> range (0.3f, hi, 0.01f);
            range.setSkewForCentre (std::sqrt (0.3f * hi));
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue);
        };
        auto db = [] (const char* id, const char* name)
        {
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                          NormalisableRange<float> (-15.0f, 15.0f, 0.01f), 0.0f,
                                                          AudioParameterFloatAttributes().withLabel ("dB"));
        };
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::eqLowType, 1 }, "EQ Low Type", ParamChoices::eqTypes, 0));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::eqHighType, 1 }, "EQ High Type", ParamChoices::eqTypes, 0));
        layout.add (hz (ParamID::eqLowFreq, "EQ Low Freq", 20.0f, 20000.0f, 120.0f));   // full range, so a Low Cut can go anywhere
        layout.add (q  (ParamID::eqLowQ, "EQ Low Q", 2.0f, 0.71f));
        layout.add (db (ParamID::eqLowGain, "EQ Low Level"));
        layout.add (hz (ParamID::eqMidFreq, "EQ Mid Freq", 100.0f, 10000.0f, 1000.0f));
        layout.add (q  (ParamID::eqMidQ, "EQ Mid Q", 10.0f, 1.0f));
        layout.add (db (ParamID::eqMidGain, "EQ Mid Level"));
        layout.add (hz (ParamID::eqHighFreq, "EQ High Freq", 20.0f, 20000.0f, 8000.0f)); // full range, so a High Cut can go anywhere
        layout.add (q  (ParamID::eqHighQ, "EQ High Q", 2.0f, 0.71f));
        layout.add (db (ParamID::eqHighGain, "EQ High Level"));
    }

    // Output
    layout.add (percent (ParamID::mix, "Mix", 100.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::outputGain, 1 }, "Output Gain",
                                                       NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

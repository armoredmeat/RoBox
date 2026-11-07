#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/CarrierOsc.h"
#include "dsp/VocoderEngine.h"
#include "dsp/TalkboxEngine.h"
#include "dsp/Character.h"
#include "dsp/Equalizer.h"
#include "dsp/NoteStack.h"
#include "presets/PresetManager.h"

// The "brain" of the plugin: the DAW calls processBlock() on the audio thread
// many times a second. Rule of thumb for that function: no memory allocation,
// no locks, no file access, nothing that can wait. Just maths on the buffer.
class RoBoxAudioProcessor final : public juce::AudioProcessor
{
public:
    RoBoxAudioProcessor();
    ~RoBoxAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { *this, apvts }; // after apvts: it reads the parameters

    // Read by the UI (milestone 7) to show the playing note and the MIDI LED. Written on the audio thread.
    std::atomic<int>  currentCarrierNote { 36 };
    std::atomic<bool> midiNoteActive { false };

    // Live data for the display and meter. Written by the audio thread, read by the UI.
    struct DisplayData
    {
        std::array<std::atomic<float>, VocoderEngine::maxBands> bandLevels {};
        std::atomic<int> numBands { 16 };
        std::array<std::atomic<float>, TalkboxEngine::maxOrder> lpc {}; // talkbox mouth-filter shape
        std::atomic<int> lpcOrder { 0 };
        std::atomic<float> voiceRms { 0.0f };
        std::atomic<float> outPeakLeft { 0.0f }, outPeakRight { 0.0f };
        std::atomic<float> sampleRate { 48000.0f };
        std::atomic<bool> talkboxMode { false };
    };
    DisplayData display;

private:
    Equalizer::Settings readEqSettings() const;
    void publishDisplayData (bool talkboxMode, double voiceSumSquares, int numSamples, const juce::AudioBuffer<float>&);

    // Cached pointers to parameter values: reading these is lock-free and safe on the audio thread.
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* waveformParam   = nullptr;
    std::atomic<float>* pulseWidthParam = nullptr;
    std::atomic<float>* detuneParam     = nullptr;
    std::atomic<float>* noteParam       = nullptr;
    std::atomic<float>* octaveParam     = nullptr;
    std::atomic<float>* chordParam      = nullptr;
    std::atomic<float>* noiseParam      = nullptr;
    std::atomic<float>* carrierSoloParam = nullptr;
    std::atomic<float>* modeParam       = nullptr;
    std::atomic<float>* bandsParam      = nullptr;
    std::atomic<float>* formantParam    = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* releaseParam    = nullptr;
    std::atomic<float>* sibilanceParam  = nullptr;
    std::atomic<float>* driveParam      = nullptr;
    std::atomic<float>* crushBitsParam  = nullptr;
    std::atomic<float>* crushRateParam  = nullptr;
    std::atomic<float>* mixParam        = nullptr;
    std::atomic<float>* midiFollowParam = nullptr;
    std::atomic<float>* tubeParam       = nullptr;
    std::atomic<float>* eqOnParam       = nullptr;
    std::array<std::atomic<float>*, Equalizer::numBands> eqFreqParams {}, eqQParams {}, eqGainParams {}, eqTypeParams {};

    NoteStack heldNotes; // MIDI keys currently down, for MIDI Follow

    CarrierOsc carrier;
    VocoderEngine vocoder;
    TalkboxEngine talkbox;
    bool wasTalkboxMode = false;

    // Switching Vocoder <-> Talkbox crossfades: for a moment both engines run and the old one fades out.
    static constexpr double modeFadeSeconds = 0.03;
    int modeFadeLength = 1, modeFadeRemaining = 0;
    Character character;
    Equalizer equalizer;
    Biquad inputHighpass;

    // Blends the original audio with the effect. Both engines add zero latency,
    // so the dry side needs no delay to stay lined up.
    juce::dsp::DryWetMixer<float> mixer;

    // Mono working buffers, sized in prepareToPlay.
    enum ScratchChannel { carrierCh = 0, voiceCh, wetCh, fadeCh, numScratchChannels };
    juce::AudioBuffer<float> scratch;

    juce::dsp::Gain<float> outputGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoBoxAudioProcessor)
};

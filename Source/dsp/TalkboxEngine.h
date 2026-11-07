#pragma once

#include <array>
#include <vector>
#include "Biquad.h"
#include "TubeVoice.h"

// Talkbox-style effect using Linear Predictive Coding (LPC).
//
// A real talkbox pipes a synth into your mouth: your mouth shape filters the synth.
// LPC does the same in software:
//  1. Every ~2.5 ms, look at the last ~30 ms of voice and estimate the "mouth filter"
//     (the vocal tract's resonances, called formants) that best explains it.
//     Maths: autocorrelation -> Levinson-Durbin recursion -> reflection coefficients.
//  2. Run the carrier synth through that filter. Its tone now has your vowels.
//  3. Match the output loudness to the voice (Attack/Release set how fast).
//
// Compared with the vocoder it captures vowel shapes far more precisely, so it sounds
// smoother and more "mouthy", like the classic talkbox funk/electro sound.
//
// It only looks at past audio, so it adds no latency.
// The filter is a lattice filter: it stays stable as long as every coefficient is
// between -1 and 1, which also makes it safe to glide smoothly between filter shapes.
class TalkboxEngine
{
public:
    static constexpr int maxOrder = 48;

    struct Settings
    {
        float formantSemitones = 0.0f; // -12 .. +12
        float attackMs  = 5.0f;
        float releaseMs = 60.0f;
        float sibilance = 0.3f;        // 0 .. 1
        float tube      = 0.0f;        // 0 .. 1, guitar-rig voicing on the carrier (see TubeVoice)
    };

    // All-pole synthesis filter in lattice form, driven by reflection coefficients k.
    struct Lattice
    {
        std::array<float, maxOrder> state {};
        void reset() noexcept { state.fill (0.0f); }

        inline float process (float x, const float* k, int order) noexcept
        {
            float f = x;
            for (int i = order; i >= 1; --i)
            {
                f -= k[i - 1] * state[(size_t) i - 1];
                if (i < order)
                    state[(size_t) i] = k[i - 1] * f + state[(size_t) i - 1];
            }
            state[0] = f;
            return f;
        }
    };

    // Levinson-Durbin: autocorrelation R[0..order] -> reflection coefficients k[0..order-1].
    // Returns false if the input was degenerate (silence); k is then all zeros.
    static bool levinson (const double* R, int order, double* k) noexcept;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const Settings& newSettings);
    void process (const float* voice, const float* carrier, float* out, int numSamples); // mono

    // While the vocoder is in use: just keep recording the voice (very cheap), so that when you
    // switch to Talkbox it already has a full analysis window and starts on the right mouth shape.
    void listen (const float* voice, int numSamples);

    // Switching to Talkbox: clear the sound-making state but keep the recorded voice, then
    // analyse it straight away so the first output already has the right vowel.
    void restart();

    int getOrder() const noexcept     { return order; }
    const std::array<float, maxOrder>& getReflectionCoefficients() const noexcept { return k; } // for the display
    int getFrameSize() const noexcept { return frameSize; }
    int getHopSize() const noexcept   { return hopSize; }

private:
    void analyse();

    double sampleRate = 48000.0;
    Settings settings;
    int order = 24, frameSize = 1440, hopSize = 120, hopCounter = 0;

    // Recent voice (pre-emphasised) for analysis. Long enough for a +12 st formant shift,
    // which reads twice as far back.
    std::vector<float> history;
    int writePos = 0;
    std::vector<float> window, frame;

    std::array<float, maxOrder> k {}, kStep {}, kTarget {};
    Lattice lattice;
    TubeVoice tubeVoice;

    float prevVoice = 0.0f;              // for pre-emphasis
    float voicePower = 0.0f, outPower = 0.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f;

    std::array<Biquad, 2> sibilanceFilters {};
};

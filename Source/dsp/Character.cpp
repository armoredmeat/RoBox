#include "Character.h"

void Character::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    driveGain.reset (sampleRate, 0.02);
    reset();
}

void Character::reset()
{
    holdPhase = 1.0f;
    heldValue = 0.0f;
    driveGain.setCurrentAndTargetValue (1.0f + 19.0f * settings.drive * settings.drive);
}

void Character::setSettings (const Settings& newSettings)
{
    settings = newSettings;
    settings.drive = juce::jlimit (0.0f, 1.0f, settings.drive);
    settings.bits  = juce::jlimit (4.0f, 16.0f, settings.bits);

    // Squared so the first half of the knob is subtle: 1x (clean) up to 20x (+26 dB into tanh).
    driveGain.setTargetValue (1.0f + 19.0f * settings.drive * settings.drive);
}

void Character::process (float* data, int numSamples)
{
    const bool driveOn = settings.drive > 0.0f || driveGain.isSmoothing();
    const bool bitsOn  = settings.bits < bitsOffThreshold;
    const bool rateOn  = settings.rateHz < rateOffThreshold && settings.rateHz < (float) sampleRate;

    const float steps    = std::exp2 (settings.bits - 1.0f); // levels per polarity
    const float holdStep = (float) (settings.rateHz / sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        float x = data[i];

        if (driveOn)
        {
            const float g = driveGain.getNextValue();
            // Saturation squashes loud parts and lifts quiet ones (that's the sound of drive).
            // Dividing by g^0.75 keeps the loudness the same at RoBox's working level
            // (about -20 dBFS RMS out of the vocoder) at every Drive setting (see CharacterTests).
            x = std::tanh (g * x) / std::pow (g, 0.75f);
        }

        if (bitsOn)
            x = std::round (x * steps) / steps;

        if (rateOn)
        {
            holdPhase += holdStep;
            if (holdPhase >= 1.0f)
            {
                holdPhase -= 1.0f;
                heldValue = x;
            }
            x = heldValue;
        }

        data[i] = x;
    }
}

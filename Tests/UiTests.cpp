// UI behaviour tests: simulated mouse drags on a real Knob, connected to the real plugin.
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Parameters.h"
#include "ui/Knob.h"

namespace
{
    juce::MouseEvent mouse (juce::Component& c, juce::Point<float> pos, bool shift)
    {
        const auto mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0));
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods,
                                 juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                 juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                                 juce::MouseInputSource::defaultTiltY, &c, &c, now, pos, now, 1, false);
    }

    // Drags upward in 'steps' moves of 'pixels' each; shiftDuring decides, per move, whether Shift is held.
    void drag (FineSlider& s, int steps, float pixels, std::function<bool (int)> shiftDuring)
    {
        juce::Point<float> pos { 20.0f, 100.0f };
        s.mouseDown (mouse (s, pos, shiftDuring (0)));
        for (int i = 0; i < steps; ++i)
        {
            pos.y -= pixels;
            s.mouseDrag (mouse (s, pos, shiftDuring (i)));
        }
        s.mouseUp (mouse (s, pos, false));
    }

    struct GestureCounter final : juce::AudioProcessorListener
    {
        int begins = 0, ends = 0;
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++begins; }
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int) override { ++ends; }
    };
}

class UiTests final : public juce::UnitTest
{
public:
    UiTests() : juce::UnitTest ("UI", "RoBox") {}

    void runTest() override
    {
        RoBoxAudioProcessor p;
        auto value = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        auto reset = [&p] (const char* id, float v) { auto* param = p.apvts.getParameter (id); param->setValueNotifyingHost (param->convertTo0to1 (v)); };

        Knob drive (p.apvts, ParamID::drive, "Drive", Theme::efx, Format::percent);
        drive.setBounds (0, 0, 72, Knob::preferredHeight);
        auto* slider = dynamic_cast<FineSlider*> (drive.getChildComponent (0));
        expect (slider != nullptr, "knob has no FineSlider");
        if (slider == nullptr)
            return;

        beginTest ("Dragging 25 px turns Drive 10 %; with Shift only 1 %");
        {
            reset (ParamID::drive, 50.0f);
            drag (*slider, 5, 5.0f, [] (int) { return false; });
            expectWithinAbsoluteError (value (ParamID::drive), 60.0f, 0.2f);

            reset (ParamID::drive, 50.0f);
            drag (*slider, 5, 5.0f, [] (int) { return true; });
            expectWithinAbsoluteError (value (ParamID::drive), 51.0f, 0.2f);
            logMessage ("  25 px drag: normal +10 %, with Shift +" + juce::String (value (ParamID::drive) - 50.0f, 2) + " %");
        }

        beginTest ("Pressing / releasing Shift mid-drag doesn't make the knob jump");
        {
            reset (ParamID::drive, 20.0f);
            // 25 px normal, 25 px with Shift, 25 px normal = 10 + 1 + 10
            drag (*slider, 15, 5.0f, [] (int i) { return i >= 5 && i < 10; });
            expectWithinAbsoluteError (value (ParamID::drive), 41.0f, 0.3f);
        }

        beginTest ("One drag = one undo step / automation gesture, Shift or not");
        {
            GestureCounter counter;
            p.addListener (&counter);
            drag (*slider, 10, 3.0f, [] (int i) { return i % 2 == 0; });
            p.removeListener (&counter);
            expectEquals (counter.begins, 1);
            expectEquals (counter.ends, 1);
        }

        beginTest ("Fine moves add up on stepped knobs (Bands)");
        {
            Knob bands (p.apvts, ParamID::bands, "Bands", Theme::voice, [] (double v) { return juce::String (v); });
            bands.setBounds (0, 0, 72, Knob::preferredHeight);
            auto* s = dynamic_cast<FineSlider*> (bands.getChildComponent (0));
            reset (ParamID::bands, 2.0f); // 16 bands
            // 20 tiny Shift moves of 25 px = 500 px = 1/5 of the range = one step (6 choices).
            drag (*s, 20, 25.0f, [] (int) { return true; });
            expectEquals ((int) value (ParamID::bands), 3, "tiny steps were lost instead of adding up");
        }

        beginTest ("Knobs stop at the ends");
        {
            reset (ParamID::drive, 95.0f);
            drag (*slider, 10, 20.0f, [] (int) { return false; });
            expectEquals (value (ParamID::drive), 100.0f);
        }
    }
};

static UiTests uiTests;

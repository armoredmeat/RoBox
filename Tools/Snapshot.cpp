// Renders the RoBox editor to PNG files, with a test vowel running through the plugin so the
// live display shows real data. Usage: RoBoxSnapshot <output folder>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "../Tests/TestSignals.h"

namespace
{
    void set (RoBoxAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    // Push ~half a second of a vowel through the plugin, then let the UI timers run.
    void runAudioAndUi (RoBoxAudioProcessor& p, const std::vector<float>& voice, size_t& pos)
    {
        constexpr int block = 512;
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        for (int b = 0; b < 48; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    buffer.setSample (ch, i, voice[(pos + (size_t) i) % voice.size()] * 2.0f);
            pos += block;
            p.processBlock (buffer, midi);
            if (b % 4 == 0)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (8);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    void save (juce::Component& c, const juce::File& file)
    {
        const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.5f);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::cout << "wrote " << file.getFullPathName() << std::endl;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File folder (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    folder.createDirectory();

    RoBoxAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto voice = TestSignals::vowel (48000, 48000.0, 700.0, 1200.0);
    size_t pos = 0;

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    // 1. Vocoder mode, defaults.
    runAudioAndUi (processor, voice, pos);
    save (*editor, folder.getChildFile ("robox-vocoder.png"));

    // 2. Talkbox mode, set up like the mockup: Square, F2 minor, Mix 85 %.
    set (processor, ParamID::mode, 1.0f);
    set (processor, ParamID::waveform, 1.0f);
    set (processor, ParamID::note, 5.0f);
    set (processor, ParamID::chord, 4.0f);
    set (processor, ParamID::mix, 85.0f);
    set (processor, ParamID::eqLowType, 1.0f);   // Low Cut
    set (processor, ParamID::eqLowFreq, 150.0f);
    set (processor, ParamID::eqMidFreq, 700.0f);
    set (processor, ParamID::eqMidGain, -4.0f);
    set (processor, ParamID::eqMidQ, 1.5f);
    set (processor, ParamID::eqHighGain, 5.0f);
    // Pretend the mouse is over the Formant knob, to show the status bar.
    if (auto* panel = dynamic_cast<MainPanel*> (editor->getChildComponent (0)))
        for (auto* child : panel->getChildren())
            if (Help::find (child).startsWith ("Formant"))
                panel->showHelpFor (child);
    runAudioAndUi (processor, voice, pos);
    save (*editor, folder.getChildFile ("robox-talkbox.png"));

    // 3. Pulse waveform, EQ switched off (Width knob replaces Detune) at 75 % window size.
    set (processor, ParamID::waveform, 2.0f);
    set (processor, ParamID::eqOn, 0.0f);
    editor->setSize (MainPanel::designWidth * 3 / 4, MainPanel::designHeight * 3 / 4);
    runAudioAndUi (processor, voice, pos);
    save (*editor, folder.getChildFile ("robox-pulse-small.png"));

    // 4. Every colour scheme, dark and light (talkbox setup, EQ on, mouse "over" Formant).
    //    One full-size PNG each, plus a contact sheet with all 10 side by side.
    set (processor, ParamID::waveform, 1.0f);
    set (processor, ParamID::eqOn, 1.0f);
    editor->setSize (MainPanel::designWidth, MainPanel::designHeight);

    constexpr int thumbW = MainPanel::designWidth / 2, thumbH = MainPanel::designHeight / 2, gap = 16, labelH = 22;
    juce::Image sheet (juce::Image::RGB, gap * 3 + thumbW * 2, gap + (int) Theme::schemes().size() * (labelH + thumbH + gap), true);
    {
    juce::Graphics sheetG (sheet); // drawing is only finished once this goes out of scope
    sheetG.fillAll (juce::Colour (0xff1A1A1A));
    sheetG.setFont (Theme::font (14.0f, true));

    for (int s = 0; s < (int) Theme::schemes().size(); ++s)
        for (bool light : { false, true })
        {
            Theme::select (s, light, false); // not saved as your preference
            runAudioAndUi (processor, voice, pos);
            const juce::String name = juce::String (Theme::schemes()[(size_t) s].name) + (light ? " light" : " dark");
            save (*editor, folder.getChildFile ("theme-" + name.replaceCharacter (' ', '-').toLowerCase() + ".png"));

            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            const int x = gap + (light ? thumbW + gap : 0), y = gap + s * (labelH + thumbH + gap);
            sheetG.setColour (juce::Colours::white);
            sheetG.drawText (name, x, y, thumbW, labelH, juce::Justification::centredLeft);
            sheetG.drawImage (image, x, y + labelH, thumbW, thumbH, 0, 0, image.getWidth(), image.getHeight());
        }
    }
    {
        const auto file = folder.getChildFile ("themes-all.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (sheet, out);
        std::cout << "wrote " << file.getFullPathName() << std::endl;
    }
    Theme::select (0, false, false);

    editor.reset();
    return 0;
}

// Tests for NoteStack (MIDI Follow, last-note priority).
#include <juce_core/juce_core.h>
#include "dsp/NoteStack.h"

class NoteStackTests final : public juce::UnitTest
{
public:
    NoteStackTests() : juce::UnitTest ("NoteStack", "RoBox") {}

    void runTest() override
    {
        constexpr int dial = 36;

        beginTest ("Nothing held = the dial note");
        {
            NoteStack s;
            expectEquals (s.currentNoteOr (dial), dial);
        }

        beginTest ("Last note pressed wins; releasing it returns to the one still held");
        {
            NoteStack s;
            s.noteOn (40);
            expectEquals (s.currentNoteOr (dial), 40);
            s.noteOn (45);
            expectEquals (s.currentNoteOr (dial), 45);
            s.noteOff (45);
            expectEquals (s.currentNoteOr (dial), 40);
            s.noteOff (40);
            expectEquals (s.currentNoteOr (dial), dial);
        }

        beginTest ("Releasing an older key doesn't change the playing note");
        {
            NoteStack s;
            s.noteOn (40);
            s.noteOn (45);
            s.noteOff (40);
            expectEquals (s.currentNoteOr (dial), 45);
            s.noteOff (45);
            expect (s.isEmpty());
        }

        beginTest ("Re-pressing a held key moves it to the top without duplicating it");
        {
            NoteStack s;
            s.noteOn (40);
            s.noteOn (45);
            s.noteOn (40);
            expectEquals (s.size(), 2);
            expectEquals (s.currentNoteOr (dial), 40);
            s.noteOff (40);
            expectEquals (s.currentNoteOr (dial), 45);
        }

        beginTest ("Overflow forgets the oldest key; stray note-offs and bad numbers are ignored");
        {
            NoteStack s;
            for (int i = 0; i < NoteStack::capacity + 4; ++i)
                s.noteOn (40 + i);
            expectEquals (s.size(), NoteStack::capacity);
            expectEquals (s.currentNoteOr (dial), 40 + NoteStack::capacity + 3);

            s.noteOff (99);   // never pressed
            s.noteOn (-1);    // invalid
            s.noteOn (128);   // invalid
            expectEquals (s.size(), NoteStack::capacity);

            s.clear();
            expectEquals (s.currentNoteOr (dial), dial);
        }
    }
};

static NoteStackTests noteStackTests;

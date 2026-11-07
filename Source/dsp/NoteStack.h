#pragma once

#include <array>

// Remembers which MIDI keys are held, in the order they were pressed, so the carrier
// can follow "last-note priority" like a classic mono synth:
//  - press a key: the carrier jumps (glides) to it
//  - press another while holding the first: it goes to the new one
//  - release the newest: it goes back to the one still held
//  - release everything: back to the Note/Octave dial
// Fixed size, no memory allocation, so it's safe on the audio thread.
class NoteStack
{
public:
    static constexpr int capacity = 16;

    void noteOn (int note) noexcept
    {
        if (note < 0 || note > 127)
            return;
        remove (note); // pressing a held key again moves it to the top
        if (count == capacity)
            removeAt (0); // full: forget the oldest
        notes[(size_t) count++] = note;
    }

    void noteOff (int note) noexcept { remove (note); }
    void clear() noexcept { count = 0; }

    bool isEmpty() const noexcept { return count == 0; }
    int size() const noexcept { return count; }

    // The note the carrier should play, or fallback (the dial) if nothing is held.
    int currentNoteOr (int fallback) const noexcept { return count > 0 ? notes[(size_t) count - 1] : fallback; }

private:
    void remove (int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (notes[(size_t) i] == note)
            {
                removeAt (i);
                return;
            }
    }

    void removeAt (int index) noexcept
    {
        for (int i = index; i < count - 1; ++i)
            notes[(size_t) i] = notes[(size_t) i + 1];
        --count;
    }

    std::array<int, capacity> notes {};
    int count = 0;
};

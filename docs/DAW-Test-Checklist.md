# RoBox 0.9.0: DAW test checklist

Everything here has already passed the automated tests (unit tests + the VST3 host test).
This list covers what only a real DAW and your ears can check. Tick each box; note anything odd
(what you did, what you heard or saw) so it can be fixed before 1.0.

## 1. Install

The VST3 is a folder called `RoBox.vst3` in `build\RoBox_artefacts\Release\VST3\`.
Copy it to the standard VST3 folder. This needs admin rights: open **PowerShell as Administrator**
(right-click the Start button > Terminal (Admin)) and run:

```powershell
Copy-Item -Recurse -Force "C:\Users\Will\Development\RoBox\build\RoBox_artefacts\Release\VST3\RoBox.vst3" "C:\Program Files\Common Files\VST3\"
```

Run it again after every new build to update the installed copy (close the DAW first).

Then in your DAW, rescan plugins if it doesn't find RoBox by itself.

- [ ] RoBox appears in the plugin list as an **effect** (not an instrument), maker "RoBox Audio"
- [ ] It loads on an audio track without errors

## 2. Basic sound

Put RoBox on a **vocal track** (a recorded vocal, or a mic input with monitoring on).

- [ ] Vocoder mode: you hear a robot voice on C2 straight away (preset "Init")
- [ ] Talkbox mode: sounds like a talkbox, with no click when you switch modes while it plays
- [ ] Mix at 0 % = your plain voice; 50 % = an even blend with **no phasey/hollow sound**; 100 % = all effect
- [ ] The output meter moves; it doesn't go red at default settings with a normal vocal level
- [ ] Solo on: you hear only the synth (handy for tuning); Solo off: the effect comes back
- [ ] On a **mono** track and a **stereo** track, both work

## 3. Every preset

Step through all 12 factory presets with the arrows while the vocal plays.

- [ ] Each one sounds different and none is much louder or quieter than the others
- [ ] No clicks or bursts when changing presets mid-playback
- [ ] "MIDI Lead" with no MIDI plays on its dial note (C3); with MIDI it follows the notes (see 4)

## 4. MIDI Follow

How you send MIDI to an effect differs per DAW:
- **FL Studio, Reaper, Bitwig, Cubase, Studio One:** route a MIDI/instrument track's output to the RoBox track (or play the RoBox track's MIDI input directly).
- **Ableton Live:** create a MIDI track, set its **MIDI To** to the vocal track, and choose **RoBox** in the second box.

- [ ] Turn **MIDI** on; playing notes changes the pitch and the light next to MIDI turns on
- [ ] The keyboard display and the note readout follow the notes you play
- [ ] Hold two notes, release the top one: the pitch falls back to the one still held
- [ ] Release all notes: the pitch returns to the Key / Octave you set
- [ ] With MIDI off, notes are ignored

## 5. Automation

- [ ] Draw automation for **Note**, **Mix** and **Mode**; playback follows it (no clicks on Mode changes)
- [ ] Turn a knob while recording automation in Write/Latch mode; it records and plays back
- [ ] The parameter list in your DAW shows 34 RoBox parameters with readable names (no long "MIDI CC" lists)
- [ ] Undo/redo (your DAW's) after moving a knob and after clicking a keyboard key

## 6. Saving

- [ ] Set up a sound, save the project, close the DAW, reopen: the sound and the preset name are the same
- [ ] Two RoBox instances on two tracks keep their own settings after reopening
- [ ] Save a preset of your own (**Save**), load another, then load yours back: it matches
- [ ] Your preset appears in `Documents\RoBox\Presets`; "Open presets folder" in the menu opens it
- [ ] Delete your preset from the menu: it goes to the Recycle Bin

## 7. Window and look

- [ ] Drag the window corner: it scales smoothly from small to large, without stretching
- [ ] Close and reopen the plugin window: it comes back as it was
- [ ] Try the themes (half-circle button): every scheme, dark and light, looks right; your choice is still there in a new project
- [ ] The status bar at the bottom explains each control as you point at it
- [ ] Popup menus and the Save dialog look right (and the Save dialog closes properly if you close the plugin window while it's open)

## 8. Sample rates and buffers

In your audio settings:

- [ ] 44.1 kHz and 48 kHz (and 96 kHz if your interface does it): pitch stays the same (C2 = 65.4 Hz on the readout and by ear)
- [ ] Smallest buffer your setup allows (e.g. 64): no crackles from RoBox
- [ ] Change the sample rate **while the project is open**: RoBox keeps working

## 9. CPU

- [ ] Your DAW's CPU meter barely moves with one RoBox (expect well under 5 %)
- [ ] With a heavy preset (Drone Vox: 32 bands) on several tracks, still fine

## Problems found

| What I did | What happened | DAW + version |
|---|---|---|
| | | |

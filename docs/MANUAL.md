# Manual

Miku-FM is a singing synthesizer for the M-VAVE FM-1, growing into a songwriting workstation (see
[ROADMAP](ROADMAP.md)). This manual covers what the firmware does **today**.

## Writing a melody

1. Turn **SELECT** to pick a step (1–16).
2. Press a key: the step gets that note and you hear it. **REC** makes it a rest instead.
3. Turn **ALGORITHM** to choose how the step sings: A, I, U, E or O. (Today these are five tone colours made from
   harmonics, not yet real vowel formants.)
4. Turn **PRESETS** for the tempo (40–240 BPM; each step is a sixteenth note).
5. **PLAY** plays from the cursor and loops all 16 steps; **PLAY** again pauses, **SEQ** stops.
6. **SAVE** (while stopped) keeps the melody and tempo in flash; they come back at the next power-on.

The first time, a demo melody is loaded.

## Saving and other firmwares

Miku-FM keeps its melody in Dinghy's settings sector of the FM-1's flash. It does not read other firmwares'
projects or samples. See the README for what saving does to other firmwares' data, and back up before installing.

## When something goes wrong

Hold **OCT−** while switching on: the USB rescue, from which another firmware (or the official one) installs. After
two crashes at boot the FM-1 goes there by itself.

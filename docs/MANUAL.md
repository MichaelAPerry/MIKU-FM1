# Manual

Miku-FM is a singing synthesizer for the M-VAVE FM-1, growing into a songwriting workstation (see
[ROADMAP](ROADMAP.md)). This manual covers what the firmware does **today**.

## Writing a melody

1. Turn **SELECT** to pick a step (1-16).
2. Press a key: the voice sings that note and the step gets it. **REC** makes the step a rest instead.
3. Turn **ALGORITHM** to choose the vowel of the step: A, I, U, E or O (a formant voice: the vowel changes the
   resonances of the sound).
4. Turn **PRESETS** for the tempo (40-240 BPM; each step is an eighth note, a note sounds for about 85% of it so
   syllables separate).
5. **PLAY** plays from the cursor and loops all 16 steps; **PLAY** again pauses, **SEQ** stops.
6. **SAVE** (while stopped) keeps the melody, tempo and knobs in flash; they come back at the next power-on.

You can also play the keys live, stopped or during playback. Holding one key and pressing another glides between
them. KNOB 1-4 set vibrato depth, portamento time, breath and brightness.

**HOME** switches between STAGE (the singer, her mouth following the sound, the syllable, tempo, a step strip and the
audio CPU load) and EDIT (the step grid and knob values). While the melody is the demo, STAGE shows its syllables;
otherwise it shows the vowel letter.

The first time, a demo melody is loaded.

## Saving and other firmwares

Miku-FM keeps its melody in Dinghy's settings sector of the FM-1's flash. It does not read other firmwares'
projects or samples. See the README for what saving does to other firmwares' data, and back up before installing.

## When something goes wrong

Hold **OCT−** while switching on: the USB rescue, from which another firmware (or the official one) installs. After
two crashes at boot the FM-1 goes there by itself.

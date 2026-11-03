# Manual

Miku-FM is a songwriter for the M-VAVE FM-1 with a singer on its screen: four tracks (VOCAL, CHORDS, BASS, DRUMS),
patterns, an arrangement, a mixer and 8 song slots. This manual covers what the firmware does **today**; every control
is in [CONTROLS](CONTROLS.md).

## How a song is built

- A **pattern** is one bar of 16 sixteenth steps for one track. Each track has 16 patterns.
- The **song** is a list of up to 64 **rows**. A row names one pattern per track (or silence) and carries a section
  label (INTRO, VERSE, CHORUS, BRIDGE, OUTRO). Rows play one after another; reusing a pattern in several rows is the point.
- The **mixer** has a level, mute, solo and preset per track, plus swing, tempo and master for the song.
- The vocal sings **moras** (a, i, u, e, o, ka, ki, ... n): each vocal step carries its mora, the vowel follows from it.

At power-on the firmware loads the slot you last used. The first time (or if that slot is empty) it plays the demo
「おとのはこ」: an intro, a verse and a chorus with chords, bass and drums.

## Playing the demo

1. **PLAY** starts the song from the cursor row; **SEQ** or **PLAY** again stops it.
2. **HOME** moves through STAGE (the singer), SONG, PATTERN and MIXER.
3. On SONG, **SELECT** picks a row, **PLAY** starts there.

## Writing a song

1. **SONG**: press **EDIT** to add a row; pick the column with **FX / SCL / ENV / LFO** (the track) and turn **PRESETS** to
   choose the pattern for that track in that row (-- = silent). **ALGORITHM** labels the section.
2. **PATTERN**: choose the track, **SELECT** the step, play a key to write it. VOCAL: the key is the note, **ALGORITHM**
   chooses the mora, **KNOB 1** the length. CHORDS: the key is the root, **ALGORITHM** the quality, **KNOB 2** how it
   plays (block, arps, strum). BASS: the key is the note, **ALGORITHM** adds accent or slide. DRUMS: keys 1-6 switch kick,
   snare, clap, hats and perc on and off at the step, keys 7-12 their accents. **REC** clears the step.
3. **PLAY** on PATTERN loops the row you are editing. **GLO** undoes (eight steps back). **ARP** copies steps,
   **ARP + EDIT** pastes them; **EDIT** alone duplicates the pattern so you can vary it.
4. **MIXER**: levels, pan, presets (the chord, bass and drum sounds), mute, solo, swing, tempo, master.
5. **SAVE** (tap, stopped) keeps the song in the current slot. Hold SAVE for the slot menu (load, new song, demo).

Edits while the song plays are allowed: single steps change at once; changes to the row list or undo pause the sequencer for
a few milliseconds. The firmware asks for a second press before dropping unsaved changes.

## STAGE

The singer's mouth follows the voice; her face blinks and smiles on PLAY. Below: the syllable that is sounding (or the
one at the cursor), BPM (and the key octave), the audio CPU load, the row and section, and four dots that light when
the vocal, chords, bass and drums sound. The vocal knobs (vibrato, portamento, breath, brightness) are KNOB 1-4 here and
are saved in the song.

## Saving and other firmwares

Songs live in Dinghy's 8 slots of the FM-1's flash (one song is 4,476 bytes, CRC-checked: a damaged slot is refused,
never half-loaded); the current slot number and the pan settings live in Dinghy's settings sector. Miku-FM does not read other
firmwares' projects or samples. See the README for what saving does to other firmwares' data, and back up before
installing. Saving erases flash, so it only happens while stopped and silent.

## Limits of this version

No song renaming on the device (a new song is "NEW SONG"). Pan is a device setting, not part of the song file. Lyrics, other
vocal consonants, MIDI sync and WAV export from the device are not there yet (see [ROADMAP](ROADMAP.md)).

## When something goes wrong

Hold **OCT−** while switching on: the USB rescue, from which another firmware (or the official one) installs. After
two crashes at boot the FM-1 goes there by itself.

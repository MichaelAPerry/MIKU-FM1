# Manual

Miku-FM is a songwriter for the M-VAVE FM-1 with a singer on its screen: four tracks (VOCAL, CHORDS, BASS, DRUMS),
patterns, an arrangement, a mixer and 8 song slots. This manual covers what the firmware does **today**; every control
is in [CONTROLS](CONTROLS.md).

## How a song is built

- A **pattern** is one bar of 16 sixteenth steps for one track. Each track has 16 patterns.
- The **song** is a list of up to 64 **rows**. A row names one pattern per track (or silence) and carries a section
  label (INTRO, VERSE, CHORUS, BRIDGE, OUTRO). Rows play one after another; reusing a pattern in several rows is the point.
- The **mixer** has a level, mute, solo and preset per track, plus swing, tempo and master for the song.
- The vocal sings **moras** (a, i, u, e, o, ka, ki, ... n): each vocal step carries its mora (ALGORITHM on PATTERN picks it;
  0 = plain "a"). The mora gives the consonant and the vowel. The consonant is a short sound at the start of the note
  (30-70 ms, the same at any pitch or tempo), then the vowel. What each sounds like: k, t, p are a silent closure, a click
  and a breathy puff; g, d, b the same with a quiet hum during the closure; s and sh a hiss (sh darker), z and j the same
  with voice, h a breath through the vowel (hi a little brighter), fu a soft airy blow, ts and ch a stop then a hiss;
  n and m a muffled nasal hum (the singer shuts her lips for m, b and p) sliding into the vowel; y and w glide in from
  i and u; r is a very short tap. The mora "n" is a hum for the whole note. A live key or a
  legato note articulates the consonant too, without restarting the note.

At power-on the firmware loads the slot you last used. The first time (or if that slot is empty) it plays the demo
「おとのはこ」: an intro, a verse and a chorus with chords, bass and drums.

## Playing the demo

1. **PLAY** starts the song from the cursor row; **SEQ** or **PLAY** again stops it.
2. **HOME** moves through STAGE (the singer's face), DANCE (the singer dancing), SONG, PATTERN, MIXER and GEN.
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

## Generating a song (GEN)

The GEN page writes a song for you, as ordinary patterns and rows: everything it makes can be edited on the other pages.

1. Pick the **style** (KNOB 1: POP, BALLAD, DANCE, CHIP), the **key** (KNOB 2) and **major / minor** (KNOB 3). The **seed**
   (KNOB 4, ALGORITHM in hundreds) picks one song among many: the same settings always give the same song.
2. **REC** writes a whole song: intro (2 bars), verse (4), chorus (4), verse, chorus, outro (2); chords, bass, a sung melody
   with lyrics, and drums; the style sets tempo and sounds. The song you had is replaced (it comes back with GLO).
3. **Lock** a track to keep it: press its track button (FX / SCL / ENV / LFO) a second time on GEN. With a track locked, REC
   keeps the rows and the locked track and rewrites the rest to fit, e.g. lock CHORDS and REC again for a new melody, bass
   and drums over the same chords (also over chords you wrote yourself).
4. **ARP** writes a new **phrase**: the pattern of the current track in the cursor row (SELECT), a different one on each
   press. A pattern used in several rows changes in all of them.
5. Each REC or ARP is a **take**: **EDIT** keeps it, **GLO** drops it and brings back the song as it was before it. Leaving the
   page keeps it. The undo history of the other pages starts again after a take.

The melody stays in the key, lands on a chord tone on beats 1 and 3 and ends each section on the chord's root; the lyrics are
random morae (sing-able, not words). GEN settings and locks are not saved.

## STAGE

STAGE is the voice's home: the keys always sing here, and every control shapes her voice. The singer's mouth follows the
voice; her face blinks and smiles on PLAY.

- **SELECT** turns the syllable dial under her face: the pink syllable under the pointer is the one the keys sing (and the
  one PATTERN writes). Selection is silent, including while holding a key; the next key uses the new syllable.
- **SCL** switches the language: JAPANESE sings the syllables as written; ANIMAL turns every held note into random
  babble at a default 140–220 ms per syllable with little pitch jumps, like the voices of animal games; SCAT sings da, ba, bi, do on eighth notes while a note is
  held; VOWELS drops the consonants.
- In ANIMAL, **hold SCL (SEL) and turn PRESETS** to set speech rate (0 slow, 100 fast; 50 default). It changes the next syllable interval without retriggering. This session control is not saved; tempo stays unchanged. Default timing is provisional pending listening review.
- **ALGORITHM** sets her character, the size of the voice: DEEP, LOW, MIKU (the default), YOUNG, TINY.
- The four knobs work in banks. VOICE (the default): breath, brightness, glide, level. **FX**: chorus, delay (a dotted eighth
  at the song tempo, echoes left and right), reverb, drive. **ENV**: attack, release, scoop (sliding up into each note),
  human (small drifts that make her sound alive). **LFO**: vibrato depth, rate and delay, tremolo. Press the bank's button
  again to go back to VOICE. The lit button shows the bank. Turning a knob shows a ring gauge and a picture of what it does:
  the attack and release shape, the scoop into the note, the vibrato wave, the delay's echoes, the reverb's tail, the drive's
  clip curve. The effects and times follow a squared curve, so small settings are easy to dial in.

**Hold** a button (0.4 s) for a deeper layer; its button blinks, and holding it again goes back to VOICE:

- **Hold SEL, SOURCE**: tension (below 50 softer and breathier, above pressed and brighter), fry (a growl an octave down),
  whisper (the voice turns to breath), formant (a fine shift between the characters).
- **Hold FX, FX DEEP**: reverb size, delay time (1/16, eighth triplet, 1/8, dotted 1/8, 1/4 of the tempo), feedback, chorus rate.
- **Hold ENV, ARTICULATION**: consonant length, consonant level, vowel speed, and ON BEAT: at 100 the band waits a moment
  (up to 66 ms) and every sung note starts early by its own consonant, so the vowels land on the beat.
- **Hold LFO, HARMONY**: a second voice sings every note at an interval (off, an octave down, a third, a fourth, a fifth, an
  octave up), with its own mix, detune and character, e.g. a DEEP voice an octave under her.

All of this is saved with the song. Songs saved by older versions load with the original sound (no effects).
The STAGE CPU figure shows what it costs: harmony and the effects together roughly double the audio load.

## DANCE

The singer full-body, dancing: she sways on the beat, sings while the vocal sounds, throws her arms up in the chorus and
bows when the song stops. The knobs, SELECT, SCL and ALGORITHM do what they do on STAGE.

## Saving and other firmwares

Songs live in Dinghy's 8 slots of the FM-1's flash (one song is 4,476 bytes, CRC-checked: a damaged slot is refused,
never half-loaded); the current slot number and the pan settings live in Dinghy's settings sector. Miku-FM does not read other
firmwares' projects or samples. See the README for what saving does to other firmwares' data, and back up before
installing. Saving erases flash, so it only happens while stopped and silent.

## Limits of this version

No song renaming on the device (a new song is "NEW SONG", a generated one is named after its style and seed, e.g. "POP 0001"). Pan is a device setting, not part of the song file. Lyrics, other
vocal consonants, MIDI sync and WAV export from the device are not there yet (see [ROADMAP](ROADMAP.md)).

## When something goes wrong

Hold **OCT−** while switching on: the USB rescue, from which another firmware (or the official one) installs. After
two crashes at boot the FM-1 goes there by itself.

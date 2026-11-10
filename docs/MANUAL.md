# Miku-FM 0.7 guide

Miku-FM is a songwriter for the M-VAVE FM-1 with a singer on its screen. Four tracks (VOCAL, CHORDS, BASS, DRUMS), patterns, an
arrangement, a mixer, a song generator, lyrics, DX7 voices you can edit and import, and ten song slots. This guide covers what
version 0.7 does **today**. The same text is on the website's [guide page](https://michaelaperry.github.io/MIKU-FM1/guide.html);
every control in one table is in [CONTROLS](CONTROLS.md).

## Getting started

- **Install.** Use the [installer page](https://michaelaperry.github.io/MIKU-FM1/install.html) in Chrome or Edge with a USB data
  cable. To go back, the same page restores the official firmware. If anything goes wrong, hold **OCT-** while switching on: the
  USB rescue, from which a firmware installs again. After two crashes at boot the FM-1 goes there by itself. See [INSTALL](INSTALL.md).
- **First sound.** At power-on the FM-1 loads the slot you last used. The first time it is slot 1, the folk song SAKURA SAKURA.
  Press **PLAY** and she sings.
- **Pages.** **HOME** steps through ten pages and back to the first: STAGE, DANCE, SONG, PATTERN, MIXER, GEN, LYRICS, LOOK,
  LIBRARY and BANK. The page name is at the top of the screen.
- **Keys.** The 27 keys play the current track's instrument on every page. On STAGE, DANCE and LIBRARY that is always the voice.
  **OCT-** and **OCT+** move the keys two octaves either way (not saved).
- **Tracks.** **FX / SCL / ENV / LFO** pick the current track (VOCAL, CHORDS, BASS, DRUMS) on most pages; the lit button is the
  track. On STAGE and DANCE they shape the voice instead. On LYRICS, LIBRARY and BANK they do the jobs listed under each page.
- **Play and stop.** **PLAY** plays the song from the cursor row (on PATTERN and LYRICS: it loops the cursor row). **SEQ** stops.
- **Save.** A tap on **SAVE** (stopped) keeps the song and its look in the current slot. Hold it for the slot menu. See
  [Slots and saving](#slots-and-saving).

## How a song is built

- A **pattern** is one bar of 16 sixteenth steps for one track. Each track has 16 patterns.
- The **song** is a list of up to 64 **rows**. A row names one pattern per track (or silence) and carries a section label
  (INTRO, VERSE, CHORUS, BRIDGE, OUTRO). Rows play one after another; reusing a pattern in several rows is the point.
- The **mixer** has a level, pan, sound, mute and solo per track, plus swing, tempo and master for the song.
- The vocal sings **morae** (a, i, u, e, o, ka, ki, ... n): each vocal step carries its mora. The mora gives the consonant and the
  vowel. The consonant is a short sound at the start of the note (30-70 ms, the same at any pitch or tempo), then the vowel.
  A live key or a legato note articulates the consonant too, without restarting the note.
- A song also carries its voice settings (STAGE), its DX7 patches (BANK) and, on the device, its look (LOOK).

## STAGE

STAGE is the voice's home: the keys always sing here, and every control shapes her voice. Her mouth follows the voice; her face
blinks and smiles on PLAY.

- **SELECT** turns the syllable dial under her face: the pink syllable under the pointer is the one the keys sing (and the one
  PATTERN writes). Selection is silent, including while holding a key.
- **SCL** (a short press) switches the language: JAPANESE sings the syllables as written; ANIMAL turns every held note into random
  babble with little pitch jumps, like an animal-game voice; SCAT sings da, ba, bi, do on eighth notes while a note is held;
  VOWELS drops the consonants. In ANIMAL, hold **SCL** and turn **PRESETS** for the speech rate (0 slow, 100 fast, 50 default;
  not saved).
- **ALGORITHM** sets her character, the size of the voice: DEEP, LOW, MIKU (the default), YOUNG, TINY. **PRESETS** is the tempo.
- **ARP** opens the song LIBRARY.
- The four knobs work in banks. VOICE (the default): breath, brightness, glide, level. **FX**: chorus, delay (a dotted eighth at the
  song tempo), reverb, drive. **ENV**: attack, release, scoop (sliding up into each note), human (small drifts that make her sound
  alive). **LFO**: vibrato depth, rate and delay, tremolo. Press the bank's button again to go back to VOICE. Turning a knob shows a
  ring gauge and a picture of what it does.
- **Hold** a button (0.4 s) for a deeper layer; its button blinks, and holding it again goes back to VOICE. **Hold SCL** (SEL):
  tension, fry (a growl an octave down), whisper, formant. **Hold FX**: reverb size, delay time, feedback, chorus rate.
  **Hold ENV**: consonant length, consonant level, vowel speed, and ON BEAT (the vowel lands on the beat). **Hold LFO**: a harmony
  voice at an interval (off, an octave down, a third, a fourth, a fifth, an octave up) with its own mix, detune and character.

All of this is saved with the song. The CPU figure on STAGE shows what it costs: harmony and the effects together roughly double
the audio load.

## DANCE

The singer full-body, dancing: she sways on the beat, sings while the vocal sounds, throws her arms up in the chorus and bows when
the song stops. The knobs, SELECT, SCL and ALGORITHM do what they do on STAGE. **EDIT** opens LOOK.

## SONG

The arrangement: row number, section, and the four pattern numbers (-- = that track is silent in the row). The playing row has a
yellow bar.

- **SELECT** picks the row. **FX / SCL / ENV / LFO** pick the column (the track). **PRESETS** sets the pattern of that cell
  (-- = silent). **ALGORITHM** labels the section.
- **EDIT** inserts a blank row before the cursor, **ARP** duplicates the row, **REC** deletes it, **GLO** undoes (eight steps).
- **KNOB 1** loops the song (right = on). **KNOB 2** tempo. **KNOB 3** transposes VOCAL, CHORDS and BASS by up to 24 semitones.
  **KNOB 4** master level.

## PATTERN

The current track's pattern in the cursor row: a piano roll for VOCAL, CHORDS and BASS, a six-line grid for DRUMS. A yellow bar
marks the playing step.

- **SELECT** picks the step; a key writes it. VOCAL: the key is the note, **ALGORITHM** the mora, **KNOB 1** the length.
  CHORDS: the key is the root, **ALGORITHM** the quality (MAJ, MIN, 7, MAJ7, MIN7, SUS4, SUS2, DIM, AUG, ADD9), **KNOB 2** how it
  plays (block, arps, strum). BASS: the key is the note, **ALGORITHM** adds accent or slide. DRUMS: keys 1-6 switch kick, snare,
  clap, closed hat, open hat and perc on and off at the step, keys 7-12 their accents.
- **KNOB 2** is the velocity for VOCAL and BASS. **KNOB 3** transposes the whole pattern. **KNOB 4** moves the cursor row.
  **PRESETS** picks the pattern for this track in this row.
- **REC** clears the step. **EDIT** duplicates the pattern into a free one so you can vary it. **ARP** copies (tap: this step; hold
  ARP and turn SELECT: a range); **ARP + EDIT** pastes. **GLO** undoes.

Edits while the song plays are allowed: single steps change at once; changes to the row list or undo pause the sequencer for a few
milliseconds.

## MIXER

Four tracks: name, sound, level, pan, mute and solo. **SELECT** picks the track. **ALGORITHM** picks its sound: CHORDS bells,
E.piano, chip arp, pad, organ, pluck, marimba, clav; BASS FM, sub, chip square, acid, sub drop, hard acid; DRUMS 808, chip, punch,
digital. On CHORDS and BASS it goes on past those into the ten DX7 voices. **KNOB 1** level, **KNOB 2** pan (a device setting),
**KNOB 3** swing, **KNOB 4** master, **PRESETS** tempo, **EDIT** mute, **ARP** solo. **REC** opens the DX7 BANK.

## GEN

The generator writes a song for you, as ordinary patterns and rows: everything it makes can be edited on the other pages.

1. **KNOB 1** picks the style. Nine of them: HYPERPOP, FUTURE BASS, JERSEY CLUB, DIGICORE, IDOL POP, then POP, BALLAD, DANCE and
   CHIP. **KNOB 2** is the key and **KNOB 3** major or minor. **KNOB 4** is the seed (**ALGORITHM** moves it by 100): the same
   settings always give the same song. **PRESETS** is the tempo.
2. **REC** writes a whole song: sections, chords, bass, a sung melody with lyrics, drums. The five newer styles bring their own tempo
   and their own DX7 chords and bass. The song you had is replaced (it comes back with **GLO**).
3. Hold **EDIT** and turn **KNOB 2** (ENERGY) or **KNOB 3** (COMPLEXITY) to shape the five newer styles: how open or busy the
   drums are and how many fills and hat rolls there are.
4. **Lock** a track to keep it: press its button (FX / SCL / ENV / LFO) a second time on GEN. With a track locked, REC keeps the
   rows and the locked track and rewrites the rest to fit: lock CHORDS and press REC for a new melody, bass and drums over the same
   chords (also over chords you wrote).
5. **ARP** writes a new **phrase**: the pattern of the current track in the cursor row (**SELECT** picks the row), a different one each
   press.
6. Each REC or ARP is a **take**: **EDIT** keeps it, **GLO** drops it and brings back the song as it was. Leaving the page keeps it.

The melody stays in the key and ends each section on the chord's root; the lyrics are random morae (sing-able, not words). GEN
settings and locks are not saved.

## LYRICS

LYRICS edits the sung syllables of the vocal pattern in the cursor row, four at a time, without touching the piano roll. Each line
shows the syllable (or REST or HOLD), its length in steps, its note, and its marks.

- **SELECT** picks the syllable. **ALGORITHM** changes it: REST, HOLD (the previous vowel carried on to a new note) and the morae.
  A key sets that syllable's note and sings it.
- **KNOB 1** is the length (1-16 steps), **KNOB 2** the note by semitones, **KNOB 3** the vocal pattern of the row, **KNOB 4** the
  row, **PRESETS** the tempo.
- **FX / SCL / ENV / LFO** toggle the marks on the syllable: WORD (the end of a word), PHRASE (the end of a phrase), SEP (a new
  onset instead of LEGATO) and HOLD.
- **EDIT** inserts a syllable, **REC** deletes it, **ARP** (tap) copies the whole phrase and **ARP + EDIT** pastes it over another
  pattern, so one lyric can sing a different melody. **GLO** undoes.

A phrase has to fit in one bar of 16 steps. If a pattern has long or overlapping notes, LYRICS asks you to fix their lengths on
PATTERN first.

## LOOK

LOOK is the singer's stage for the song you have loaded. **SELECT** picks one of 12 outfits (a portrait, a dancing body, a set of
stage colours). Then change what you like:

- **KNOB 1**: the background (ORIGINAL, GRID, HORIZON, CONCERT, LED WALL).
- **KNOB 2**: the frame (NONE, CORNERS, RAILS).
- **KNOB 3** or **ALGORITHM**: the stage effect (OFF, FIREWORKS, SPARKLES, CONFETTI, SMOKE, BEAMS). **REC** fires it once.
- **KNOB 4**: the effect's intensity. **PRESETS**: STYLE or WARM colouring.
- **EDIT** goes back to DANCE. Stop the song and press **SAVE** to keep the look; a mark on the page shows unsaved changes.

The look belongs to the **slot** on the device: each of the ten slots keeps its own. It is not part of an exported song file.

## LIBRARY

LIBRARY lists the ten slots, four at a time, with the name of each song. Slots 1 to 4 start as four Japanese folk songs, written as
ordinary songs you can edit: SAKURA SAKURA, TORYANSE, FURUSATO and KOJO NO TSUKI. Slots 5 to 10 start empty. **STAGE or DANCE:
ARP** opens it; **HOME** gets there too.

- **SELECT** or **PRESETS** moves the cursor. **PLAY** loads the song under the cursor and plays it. **SEQ** goes back to SONG.
- **FX** or **EDIT** loads it (an empty slot says EMPTY SLOT; EDIT starts a new song there). **SCL** starts a new song in the slot.
  **ENV** restores a factory song to how it was shipped (slots 1 to 4). **LFO** opens its LOOK.
- **REC** saves the current song into the slot under the cursor. If another song is there, REC asks a second time: OVERWRITE? REC
  AGAIN.
- Anything that would drop unsaved changes asks first: UNSAVED! AGAIN. Press the same control again within 3 seconds.

## Slots and saving

The FM-1 keeps 10 songs. At power-on it loads the slot you last saved or opened. **SAVE** (tap, stopped) writes the song and its
look to the current slot (SAVED, or SAVE FAILED). Saving erases flash, so it only happens while stopped; the sound drops out for a
moment.

Hold **SAVE** (about a third of a second, or turn SELECT while holding it) for the slot menu: **SELECT** picks the slot, then, still
holding SAVE, press **FX** to load it, **SCL** for a new empty song or **ENV** for the demo 「おとのはこ」. Release SAVE: if you
pressed none of those, the song is saved into the slot shown.

A song is 4,508 bytes and CRC-checked: a damaged slot is refused, never half-loaded. Songs carry their own copy of the DX7 voices
they use, so they play the same whatever is in your user bank.

## BANK: the DX7 voices

BANK is the six-operator FM engine. Chords and bass can each use a DX7 voice instead of the classic sounds. The engine has all 32
DX7 algorithms, feedback, envelopes, LFO, pitch bend, sustain and bass glide. Ten original voices come with Miku-FM: HYPER GLAS and
HYPER 808, FUTURE SAW and FUTURE SUB, CLUB STAB and CLUB BOUNC, DIGI PLUCK and DIGI GROWL, IDOL KEYS and IDOL BASS.

**Open it** with **HOME**, or press **REC** on MIXER. **SCL** picks CHORDS and **ENV** picks BASS (the BANK page works on those two).

### Browse and load

BROWSE is the first page. It shows FACTORY n / 10 or USER n / 32 with the voice name, and the level of each operator.

- **SELECT** browses silently. **ARP** switches between FACTORY (the ten originals) and USER (your 32, once you have imported a bank).
- **REC** loads the voice you see into the track. The keys play it. If the track holds an edited patch, REC asks a second time
  (REPLACE EDIT? AGAIN) so you do not lose it.
- You can also load voices from MIXER: **ALGORITHM** on CHORDS or BASS goes through the classic sounds, the ten DX7 voices and, once
  you have edited, a CUSTOM entry that is your edit.

### Edit

With a DX7 voice loaded, **PRESETS** or **EDIT** steps through the edit pages, and **SELECT** or **ALGORITHM** chooses operator OP1 to OP6. The four knobs edit the four
fields named on the screen. The voice becomes CUSTOM, and the song keeps it.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| GLOBAL | ALG (algorithm 1-32) | FB (feedback) | LFO (speed) | LVL (track level) |
| OP FREQUENCY | OUT (output level) | CRS (coarse) | FINE | DET (detune) |
| OP ENV RATES | R1 | R2 | R3 | R4 |
| OP ENV LEVELS | L1 | L2 | L3 | L4 |
| KEYBOARD SCALE | BREAK (break point) | LEFT (depth) | RIGHT (depth) | RATE (scaling) |
| MODULATION | DELAY (LFO delay) | PMD | AMD | PMS |
| PITCH RATES | R1 | R2 | R3 | R4 |
| PITCH LEVELS | L1 | L2 | L3 | L4 |
| MIX / TUNING | LVL (track level) | PAN | TRANS (transpose) | SYNC (oscillator sync) |
| CURVES / TOUCH | LEFT (curve) | RIGHT (curve) | VEL (velocity sensitivity) | AMS |

**ARP** on OP FREQUENCY switches the operator between RATIO and FIXED frequency; on MODULATION it steps the LFO waveform. The keys
play the voice while you edit. **REC** goes back to BROWSE. Stop the song and **SAVE** to keep the patch in the song.

### The user bank

The user bank is 32 voices of your own, kept on the FM-1 (not in the songs) until you replace them. It starts empty. You fill it by
[importing a cartridge](#importing-a-cartridge-from-dexed-or-the-bank-tool) or by storing your own patches:

- On any edit page, press **GLO**: the page shows REC STORE USER n and GLO NEXT. **GLO** again picks the next slot; **REC** stores the
  current patch there, with its name. Storing into a slot replaces what was in it. You have 8 seconds to press REC.
- With no bank yet, the first store creates one and fills the other 31 slots with INIT VOICE.
- Voice names come with the patch (10 characters). To rename a user voice: stop the song, switch BROWSE to USER with **ARP**, pick the
  slot with **SELECT** and press **EDIT**. **SELECT** moves between letters, **K1** changes the letter, **REC** saves and **GLO** cancels.
- Stores need the song stopped.

### Export

In BROWSE press **GLO**: the FM-1 sends its user bank over USB as a standard DX7 32-voice dump (it says USER BANK SENT, or NO USER
BANK TO SEND). Receive it with the [bank tool](https://michaelaperry.github.io/MIKU-FM1/bank.html) or with Dexed or any MIDI
librarian that can record a SysEx dump from a port, and keep it as a .syx file.

## Importing a cartridge from Dexed or the bank tool

The FM-1 accepts DX7 SysEx over **USB MIDI** (version 0.7 or newer): a **32-voice bulk dump** (a .syx cartridge) or a **single voice**.
The TRS MIDI jack is for notes only.

1. Connect the FM-1 by USB. Stop any song.
2. Send the dump. From the [bank tool](https://michaelaperry.github.io/MIKU-FM1/bank.html): open the .syx, press Connect, choose the FM-1 and press Send. From
   Dexed: send the cartridge (or the current voice) to the FM-1's USB MIDI port.
3. The FM-1 checks the dump and shows what it would do: IMPORT 32 VOICES? REC, VOICE TO USER n? REC, or VOICE TO EDIT? REC. Nothing has
   changed yet.
4. Press **REC** within 10 seconds, on any page, to accept. Let it lapse to refuse.

What each does: a bulk dump replaces the whole user bank and selects USER 1. A single voice goes into the edit buffer of the open
BANK edit page (VOICE TO EDIT, the patch becomes CUSTOM), and anywhere else into the user slot chosen in BROWSE (VOICE TO USER n; with
no bank yet the other 31 are INIT VOICE).

If something is wrong the FM-1 says so and ignores it: BAD CHECKSUM, BAD DX7 LENGTH or INVALID VOICE n (a cartridge with one bad voice is
refused whole). Imports need the song stopped (STOP FIRST). The bank tool has a factory file with the ten Miku-FM voices that loads in
Dexed too.

## MIDI keyboard in

A MIDI keyboard on USB or the TRS jack plays the current track exactly like the keys: the singer on STAGE, DANCE and LIBRARY, otherwise
the selected track (drums use the note modulo 12). The **sustain pedal** (CC 64) holds the notes of the singer, the chords and the bass, classic or DX7.
The **pitch bend wheel** bends DX7 chords and bass; the classic sounds ignore it. CC 120 and 123 release every held note. MIDI does not write pattern
steps, and MIDI clock is ignored.

## Other firmwares, limits, and when something goes wrong

Songs live in Dinghy's 10 slots of the FM-1's flash; the current slot, the pan settings, the looks and the user bank live in Dinghy's
settings sector and blobs. Miku-FM does not read other firmwares' projects or samples, and saving replaces the settings other
Felucca-family firmwares keep: back up before installing (see [INSTALL](INSTALL.md)).

Limits of this version: songs can't be renamed on the device (a new song is NEW SONG, a generated one is named after its style and
seed; user DX7 voices can be renamed); 4/4 only; one bar of 16 steps per pattern; pan is a device setting, not part of the song file; looks are per slot, not in exported
songs. USB video output is not in 0.8. MIDI clock sync and WAV export from the device are not there yet (see [ROADMAP](ROADMAP.md)).

If the firmware misbehaves, hold **OCT-** while switching on for the USB rescue (no sound, only the updater).

## More detail

[CONTROLS](CONTROLS.md) · [DX7 bank engineering notes](DX7_BANK_HANDOFF.md) · [Song format](SONG_FORMAT.md) · [Install and recovery](INSTALL.md) ·
[What has been measured](VALIDATION.md)

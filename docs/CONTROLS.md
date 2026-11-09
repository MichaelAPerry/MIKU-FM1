# Controls

Every control on every page, version 0.7. How to use them is in the [guide](MANUAL.md).

Miku-FM is a four-track songwriter with a singer. **HOME** cycles the ten pages: STAGE, DANCE, SONG, PATTERN, MIXER, GEN, LYRICS, LOOK, LIBRARY, BANK (the page name is on
the screen). One control map holds everywhere; the tables below list what each control does on each page.

Everywhere:

| Control | What it does |
|---|---|
| **FX / SCL / ENV / LFO** | Pick the current track: VOCAL, CHORDS, BASS, DRUMS (its button LED lights). On STAGE and DANCE they shape the voice instead (below) |
| **Key** (27 keys, F3-G5) | Play the current track's instrument (on STAGE and DANCE always the voice, singing the selected syllable): VOCAL sings, CHORDS plays a note, BASS plays a note, DRUMS: keys 1-6 hit kick, snare, clap, closed hat, open hat, perc. A held second vocal key glides (legato) |
| **OCT− / OCT+** | Octave for the keys, −2 to +2 (shown on STAGE; not saved) |
| **PLAY** | Play / stop. STAGE, SONG, MIXER: the song from the cursor row. PATTERN: loop the cursor row |
| **SEQ** | Stop |
| **HOME** | Next page |
| **SAVE** (tap) | Save the song to the current slot (only while stopped): "SAVED" or "SAVE FAILED" |
| **SAVE** (hold) | Slot menu, see below |
| **GLO** | Undo (SONG, PATTERN, LYRICS): up to 8 steps back. GEN: drop the take. BANK: send the user bank (BROWSE) / store the patch (edit pages) |

Keys sound the current track on every page (on STAGE, DANCE and LIBRARY always the voice). On PATTERN they also write the cursor step; on LYRICS they set the note of the cursor syllable.

## STAGE

The voice's home. The singer's face; under it the **syllable dial**: the top of a big dial rising from the bottom edge, the
syllables round its rim with a tick each, the selected one in pink under the pointer; it turns with SELECT. Under the dial:
knob bank (left), language and character (middle), BPM and CPU (right). Turning a knob shows, for a moment, a ring gauge
with its value, its name and a picture of what it does (the envelope, the pitch curve, the vibrato, the echoes, the clip
curve, the harmonics ...). Everything here is saved with the song. The effect amounts, drive, attack, release and vibrato
delay follow a squared curve: fine steps at the low end, big moves at the top.

| Control | What it does |
|---|---|
| SELECT | The syllable the keys sing (a, i, u, ... n, ga ... po); selection is silent, including while holding a key. PATTERN writes the same one |
| SCL (SEL) | Language: JAPANESE (the syllables as written), ANIMAL (random babble at a default 140–220 ms per syllable with little pitch jumps, like an animal-game voice), SCAT (da ba bi do ... on eighth notes while a note is held), VOWELS (no consonants) |
| FX | The knobs become CHORUS, DELAY (dotted eighth at the song tempo), REVERB, DRIVE. Press again: back to VOICE |
| ENV | The knobs become ATTACK, RELEASE, SCOOP (slide up into a note), HUMAN (drift, jitter, shimmer). Press again: VOICE |
| LFO | The knobs become VIBRATO (depth), VIB RATE (3-8 Hz), VIB DELAY, TREMOLO. Press again: VOICE |
| KNOB 1 / 2 / 3 / 4 | VOICE bank (the default): BREATH, BRIGHT, GLIDE (portamento), LEVEL (the vocal track). The bank's button LED is lit (none for VOICE) |
| **Hold** SEL (0.4 s) | SOURCE layer (the button blinks): TENSION (soft/breathy below 50, pressed above), FRY (growl an octave down), WHISPER (voice into breath), FORMANT (fine shift between characters, 50 = none) |
| **Hold** FX | FX DEEP: REV SIZE, DLY TIME (1/16, 1/8T, 1/8, 1/8., 1/4; one step per click), FEEDBACK, CHO RATE (0.1-4 Hz) |
| **Hold** ENV | ARTICULATION: CONS LEN (25-175 %), CONS LEVEL (0-200 %), VOWEL SPEED (25-175 %), ON BEAT (the vowel on the beat: the band waits up to 66 ms x consonant length, each note starts early by its consonant) |
| **Hold** LFO | HARMONY: INTERVAL (OFF, -12, +3, +4, +5, +7, +12; one step per click), HARM MIX, DETUNE (0-30 cents), HARM VOICE (its character) |
| Hold again / short press | Hold the same button again: back to VOICE. A short press goes to that button's normal bank (or, SEL, the next language) |
| ALGORITHM | Character (formant size): DEEP, LOW, MIKU, YOUNG, TINY |
| PRESETS | Tempo, 40-240 BPM. In ANIMAL, hold SCL (SEL) + turn PRESETS: speech rate 0–100 (50 default; session only) |
| ARP | Open the song LIBRARY |
| Key | Sings the selected syllable |

## DANCE

The singer full-body (3x), dancing to the song: one move per beat while playing, singing while the vocal sounds, arms up in
CHORUS rows, a bow when the song stops, a slow sway when stopped. Below her: row and section, the syllable, BPM.
The controls are those of STAGE (syllable, language, knob banks, character), except ARP (song LIBRARY) and **EDIT**, which opens LOOK.

## SONG

The arrangement: row number, section, and the four pattern numbers (-- = that track is silent in the row). The
column of the current track is highlighted; the playing row has a yellow bar. The header shows slot, name, number of rows
and loop.

| Control | What it does |
|---|---|
| SELECT | Cursor row |
| FX / SCL / ENV / LFO | Cursor column (= current track) |
| PRESETS | Pattern number of the cell (-- = silent) |
| ALGORITHM | Section label of the row: none, INTRO, VERSE, CHORUS, BRIDGE, OUTRO |
| EDIT | Insert a blank row before the cursor |
| ARP | Duplicate the row (inserted after it) |
| REC | Delete the row |
| KNOB 1 | Loop the song on / off (turn right / left) |
| KNOB 2 | Tempo |
| KNOB 3 | Transpose VOCAL, CHORDS, BASS, −24 to +24 semitones |
| KNOB 4 | Master level |

## PATTERN

The current track's pattern of the cursor row (header: row R, pattern P). VOCAL, CHORDS and BASS show a piano roll:
16 columns, a bar per note at its pitch, as long as its length; white head = cursor step. DRUMS show a 6 × 16 grid
(kick, snare, clap, closed hat, open hat, perc from the top); a white dot is an accent. A yellow bar marks the playing step
(PLAY here loops the cursor row). If the row has no pattern for this track ("SILENT HERE"), the first key or EDIT gives
it a free one.

| Control | What it does |
|---|---|
| SELECT | Cursor step |
| Key | VOCAL: write the note (length and mora as set below). CHORDS: write the chord with this root. BASS: write the note. DRUMS: keys 1-6 toggle kick, snare, clap, closed hat, open hat, perc; keys 7-12 toggle the accent of those |
| ALGORITHM | VOCAL: the mora (a, i, u, e, o, ka, ki, ... n; the vowel follows the mora, you hear it). CHORDS: quality (MAJ MIN 7 MAJ7 MIN7 SUS4 SUS2 DIM AUG ADD9). BASS: none / ACC / SLIDE / both. Turning it also changes the step under the cursor, if it has a note |
| KNOB 1 | Length in steps (1-64) of the next note, and of the step under the cursor |
| KNOB 2 | VOCAL, BASS: velocity 0-15. CHORDS: mode (block, arp up, down, up-down, strum, the same in 32nds) |
| KNOB 3 | Transpose the whole pattern by semitones (not DRUMS) |
| KNOB 4 | Cursor row (the pattern shown follows) |
| PRESETS | Pattern number of this track in the cursor row (-- = silent) |
| REC | Clear the step |
| EDIT | Duplicate this pattern into a free one and use it in this row |
| ARP | Copy: tap = this step; hold ARP, turn SELECT, release = the steps between (whole pattern: 1 to 16) |
| ARP + EDIT | Paste at the cursor (same track only) |
| GLO | Undo |

## MIXER

Four tracks: name, preset, level bar, pan marker, M (mute) and S (solo). The current track is highlighted.

| Control | What it does |
|---|---|
| SELECT | Current track |
| ALGORITHM | Preset of the track: CHORDS bells, E.piano, chip arp, pad, organ, pluck, marimba, clav; BASS FM, sub, chip square, acid, sub drop, hard acid; DRUMS 808, chip, punch, digital |
| KNOB 1 | Level |
| KNOB 2 | Pan (a device setting, kept with SAVE; the song file has no pan) |
| KNOB 3 | Swing, 0-75 % |
| KNOB 4 | Master |
| PRESETS | Tempo |
| EDIT | Mute |
| ARP | Solo |

## GEN

The song generator (see [MANUAL](MANUAL.md)). Header: the take hint, the cursor row. Below: style, key and scale, seed, tempo,
the four tracks (dark = locked; the white frame is the current track).

| Control | What it does |
|---|---|
| KNOB 1 | Style: HYPERPOP, FUTURE BASS, JERSEY CLUB, DIGICORE, IDOL POP, POP, BALLAD, DANCE, CHIP |
| KNOB 2 | Key, C to B |
| KNOB 3 | Major / minor |
| KNOB 4 | Seed, by 1 (ALGORITHM: by 100) |
| SELECT | Cursor row (where ARP writes a phrase) |
| PRESETS | Tempo |
| FX / SCL / ENV / LFO | Pick the track; pressing the current track's button again locks / unlocks it |
| REC | Generate the song: all unlocked tracks (nothing locked: also new rows, name, tempo, sounds). Starts again from row 1 if playing |
| ARP | New phrase: the current track's pattern in the cursor row (another variation each press) |
| EDIT | Keep the take. **Hold EDIT** + KNOB 2 / KNOB 3: ENERGY / COMPLEXITY of the five newer styles (0-100) |
| GLO | Drop the take: back to the song before the last REC / ARP |

## Songs and slots

The device keeps 10 songs (slots 1–10). At power-on it loads the last saved slot.
Empty slots 1–4 reconstruct the factory song of that slot (SAKURA SAKURA, TORYANSE, FURUSATO, KOJO NO TSUKI); the demo is the fallback
for other empty slots. The LIBRARY page below loads and saves slots.

Hold **SAVE** (about a third of a second, or turn SELECT while holding it) for the slot menu: **SELECT** picks the slot
(its song name is shown), then, still holding SAVE, press **FX** to load that slot, **SCL** for a new empty song (one row,
pattern 1 everywhere), **ENV** to reload the demo. Release SAVE: if you pressed none of those, the song is saved into the slot
shown. Loading, new and demo replace the song in memory: if it has unsaved changes the first press only says "UNSAVED! AGAIN",
and the same press again within 3 seconds goes through.

## At power-on (Dinghy's platform)

| Hold while switching on | What happens |
|---|---|
| **OCT−** | USB rescue: no sound, only the updater, so another firmware can be installed |

If the firmware crashes twice at boot, the FM-1 goes to the USB rescue by itself.

## LYRICS

The sung syllables of the vocal pattern in the cursor row, four lines at a time (syllable, LEN, note, marks). The track is VOCAL.

| Control | What it does |
|---|---|
| SELECT | Cursor syllable |
| ALGORITHM | The syllable: REST, HOLD (carry the previous vowel to a new note), then the morae |
| Key | Set the note of the cursor syllable (and sing it) |
| KNOB 1 | Length, 1-16 steps |
| KNOB 2 | Note, by semitones |
| KNOB 3 | Vocal pattern of the cursor row |
| KNOB 4 | Cursor row |
| PRESETS | Tempo |
| FX / SCL / ENV / LFO | Toggle the marks: WORD end, PHRASE end, SEP (separate onset; off = LEGATO), HOLD |
| EDIT | Insert a one-step syllable at the cursor |
| REC | Delete the syllable |
| ARP | Copy the whole phrase (tap) |
| ARP + EDIT | Paste the copied phrase over this pattern |
| GLO | Undo |

## LOOK

The singer's stage for the loaded song: outfit, background, frame, stage effect. Kept per slot with SAVE.

| Control | What it does |
|---|---|
| SELECT | Outfit, 1-12 |
| PRESETS | Colouring: STYLE / WARM |
| KNOB 1 | Background: ORIGINAL, GRID, HORIZON, CONCERT, LED WALL |
| KNOB 2 | Frame: NONE, CORNERS, RAILS |
| KNOB 3, ALGORITHM | Effect: OFF, FIREWORKS, SPARKLES, CONFETTI, SMOKE, BEAMS |
| KNOB 4 | Effect intensity, 0-100 % |
| REC | Fire the effect |
| EDIT | Back to DANCE |
| SAVE | Keep the song and its look (stopped) |

## LIBRARY

The ten slots, four at a time, with the song name under each. Opened with HOME or ARP on STAGE / DANCE.

| Control | What it does |
|---|---|
| SELECT, PRESETS | Cursor slot |
| PLAY | Load the song under the cursor and play it |
| SEQ | Back to SONG |
| FX | Load the slot (factory song for an empty slot 1-4) |
| EDIT | Load the slot, or start a new song in an empty one |
| SCL | New song in the slot |
| ENV | Restore the factory song (slots 1-4) |
| LFO | Open the slot's LOOK |
| REC | Save the current song into the slot (a second REC when another song is there: OVERWRITE? REC AGAIN) |

Actions that drop unsaved changes say UNSAVED! AGAIN: repeat within 3 seconds.

## BANK

The DX7 voices of CHORDS and BASS. Opened with HOME or REC on MIXER. SCL and ENV pick CHORDS and BASS (FX and LFO do nothing here).
**BROWSE** is the first page; PRESETS or EDIT steps through the ten edit pages when a DX7 voice is loaded.

| Control | BROWSE | Edit pages |
|---|---|---|
| SELECT | Browse silently (FACTORY 10 or USER 32) | Operator OP1-OP6 |
| ALGORITHM | | Operator OP1-OP6 |
| PRESETS | Step to the edit pages | Page |
| EDIT | Step to the edit pages | Next page |
| KNOB 1-4 | | The four fields named on the screen |
| ARP | Switch FACTORY / USER | OP FREQUENCY: RATIO / FIXED. MODULATION: LFO waveform |
| REC | Load the voice into the track (second press if it holds an edit) | Back to BROWSE |
| GLO | Send the user bank as a DX7 dump (USB) | Pick a user slot to store into (again: the next); REC then stores the patch |
| Key | Play the loaded voice | Try the voice |

Edit pages: GLOBAL, OP FREQUENCY, OP ENV RATES, OP ENV LEVELS, KEYBOARD SCALE, MODULATION, PITCH RATES, PITCH LEVELS, MIX / TUNING,
CURVES / TOUCH; the fields are listed in the [guide](MANUAL.md#edit).

**SysEx in.** A DX7 bulk dump or single voice sent over USB (Dexed, any librarian, the website's bank tool) shows IMPORT 32 VOICES? REC,
VOICE TO USER n? REC or VOICE TO EDIT? REC: press **REC** within 10 s on any page. Bad data: BAD CHECKSUM, BAD DX7 LENGTH, INVALID VOICE n.
Imports and stores need the song stopped.

**MIDI in.** Notes (USB or TRS) play the current track like the keys. On a DX7 voice on CHORDS or BASS, CC64 sustains and the wheel bends.
[Details](DX7_BANK_HANDOFF.md).

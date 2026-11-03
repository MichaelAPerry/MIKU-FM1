# Controls

Miku-FM 0.4 is a four-track songwriter. **HOME** cycles the pages: STAGE, SONG, PATTERN, MIXER (the page name is on
the screen). One control map holds everywhere; the tables below list what each control does on each page.

Everywhere:

| Control | What it does |
|---|---|
| **FX / SCL / ENV / LFO** | Pick the current track: VOCAL, CHORDS, BASS, DRUMS (its button LED lights) |
| **Key** (27 keys, F3-G5) | Play the current track's instrument: VOCAL sings, CHORDS plays a note, BASS plays a note, DRUMS: keys 1-6 hit kick, snare, clap, closed hat, open hat, perc. A held second vocal key glides (legato) |
| **OCT− / OCT+** | Octave for the keys, −2 to +2 (shown on STAGE; not saved) |
| **PLAY** | Play / stop. STAGE, SONG, MIXER: the song from the cursor row. PATTERN: loop the cursor row |
| **SEQ** | Stop |
| **HOME** | Next page |
| **SAVE** (tap) | Save the song to the current slot (only while stopped): "SAVED" or "SAVE FAILED" |
| **SAVE** (hold) | Slot menu, see below |
| **GLO** | Undo (SONG, PATTERN): up to 8 steps back |

Keys sound the current track on every page. On PATTERN they also write the cursor step.

## STAGE

The singer, the vocal syllable (the one sounding, or the one at the cursor), BPM and octave, CPU, the row and section,
and four dots that light while VOCAL, CHORDS, BASS, DRUMS sound.

| Control | What it does |
|---|---|
| SELECT | Cursor row |
| ALGORITHM | Preset of the current track (CHORDS 8, BASS 4, DRUMS 2) |
| PRESETS | Tempo, 40-240 BPM |
| KNOB 1 / 2 / 3 / 4 | Voice: vibrato, portamento, breath, brightness (0-100, saved in the song) |

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
| ALGORITHM | Preset of the track: CHORDS bells, E.piano, chip arp, pad, organ, pluck, marimba, clav; BASS FM, sub, chip square, acid; DRUMS 808, chip |
| KNOB 1 | Level |
| KNOB 2 | Pan (a device setting, kept with SAVE; the song file has no pan) |
| KNOB 3 | Swing, 0-75 % |
| KNOB 4 | Master |
| PRESETS | Tempo |
| EDIT | Mute |
| ARP | Solo |

## Songs and slots

The device keeps 8 songs (slots 1-8). At power-on it loads the slot you last saved to or loaded; if that is empty or
damaged it plays the demo 「おとのはこ」.

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

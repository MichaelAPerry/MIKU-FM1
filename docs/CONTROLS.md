# Controls

Two pages; **HOME** switches between them. The controls are the same on both.

- **STAGE** (at power-on): the singer, the current syllable, the tempo, a small CPU% (the worst audio block, as a
  share of its 5.8 ms), and a strip of the 16 steps.
- **EDIT**: the 16-step grid (top row steps 1-8, bottom row 9-16) and the four knob values (V P B T).

In the strip and the grid pink is the cursor, yellow the playhead, teal a step with a note, grey a rest. A step is an
eighth note.

| Control | What it does |
|---|---|
| **Key** | Sing that note live, stopped or playing; also writes it into the cursor step. A second key while one is held glides to it (legato); letting go of the last key ends the note |
| **SELECT** (turn) | Move the cursor to another step |
| **REC** | Make the step a rest |
| **ALGORITHM** (turn) | Change the step's vowel: A, I, U, E, O |
| **PRESETS** (turn) | Tempo, 40-240 BPM |
| **KNOB 1** | Vibrato depth (V), 0-100 |
| **KNOB 2** | Portamento time (P), 0-100 (0-500 ms) |
| **KNOB 3** | Breath (B), 0-100 |
| **KNOB 4** | Brightness (T), 0-100 |
| **OCT− / OCT+** | Transpose the voice by octaves, −2 to +2 (shown as OCT+1 on STAGE; not saved) |
| **PLAY** | Play from the cursor; again to pause |
| **SEQ** | Stop |
| **SAVE** | Save the melody, tempo and knobs (only while stopped) |
| **HOME** | STAGE / EDIT |

## At power-on (Dinghy's platform)

| Hold while switching on | What happens |
|---|---|
| **OCT−** | USB rescue: no sound, only the updater, so another firmware can be installed |

If the firmware crashes twice at boot, the FM-1 goes to the USB rescue by itself.

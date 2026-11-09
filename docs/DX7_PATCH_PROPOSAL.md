# DX7 patches: what FM-1 owners use, and original candidates for the GEN styles

Status: **proposal for owner listening.** Nothing here changes the compiled bank (`firmware/audio/dx7/bank.h`).
It answers review finding F1 in `docs/PR21_CLAUDE_REVIEW.md`.

## What FM-1 owners actually load (researched 2026-11-09)

1. **The FM-1's own 128 factory voices.** These are the most-used patches, because they are the default.
   A community reconstruction ([KingParamount/fm1-factory-presets](https://github.com/KingParamount/fm1-factory-presets))
   says that 126 of the 128 are byte-identical to voices in BlackWinny's *Dexed_cart 1.0* compilation, and that 40 trace
   to Yamaha DX7 ROM/VRC cartridges. Examples: PIANO 1 = ROM1A "E.PIANO 1", ORGAN 1 = "E.ORGAN 1",
   BASS 1 = "*Bass-Synt", SYN BASS1 = "DENTIST". Notably, **SYN PAD 5 is "Slow3D Pad"**, the same voice PR21 ships.
   The bank layout is A piano/organ/lead/pad, B guitar/bass/synth bass, C brass/woodwind/string/voice,
   D percussion/effects.
2. **Dexed cartridges loaded over SysEx**, usually BlackWinny's *Dexed_cart 1.0* compilation, sent with Dexed or a browser
   librarian ([dx7-to-fm1.dev](http://dx7-to-fm1.dev/),
   [benny-sparra/fm1-dx7-patch-importer](https://github.com/benny-sparra/fm1-dx7-patch-importer)).
   Source: [The Synth Source guide](https://thesynthsource.com/dx7-banks-m-vave-fm-1/).
3. **Personal compilations** of "FM staple sounds, DOS, and Genesis games" taken from dxcompanion.uk
   ([KVR thread](https://www.kvraudio.com/forum/viewtopic.php?t=632127)).
4. **Commercial banks** made for the FM-1, such as Natlife Sounds "Ambient & Dreams".

None of the sources names a specific crowd-favourite patch. Owners talk about loading cartridges, not about one voice.

**Licensing consequence:** none of these sources gives a redistribution licence for the voice data. The reconstruction
repository puts only its own transport work under CC0 and says it "claim[s] no rights in the voice parameters." The
patches people use are therefore exactly the uncleared material behind finding F1. They are fine for users to load
themselves, but they are not cleared for MIKU to compile into firmware. The clean route for a built-in bank is
**original patches**. Users can keep loading whatever they like if a future SysEx/bank import is added.

## Original candidates, two per GEN style

Authored for this project in `tools/dx7_candidates.py`. That file holds every parameter, there are no third-party bytes,
and the voices fall under the project licence. Running it writes `assets/dx7/candidates.packed`
(10 × 128-byte packed voices). The renders below come from the device renderer via `tools/dx7_audition.c`. All voices
play the same C–Am–F–G progression at 140 BPM: chord voices as four-voice half-note chords, bass voices as a one-voice
eighth-note line.

| # | Name | GEN style | Track | Algorithm | Design idea | Listen |
|---:|---|---|---|---:|---|---|
| 0 | HYPER GLAS | HYPERPOP | chords | 5 | detuned glassy bell pairs, 3.5× and 7× shimmer, light vibrato | [MP3](evidence/dx7-candidates/cand-0.mp3) |
| 1 | HYPER 808 | HYPERPOP | bass | 2 | sine body with a pitch-envelope "zap" on attack | [MP3](evidence/dx7-candidates/cand-1.mp3) |
| 2 | FUTURE SAW | FUTURE BASS | chords | 22 | one feedback operator driving three detuned carriers (saw-stack feel), soft attack | [MP3](evidence/dx7-candidates/cand-2.mp3) |
| 3 | FUTURE SUB | FUTURE BASS | bass | 1 | near-pure sub with a little second harmonic | [MP3](evidence/dx7-candidates/cand-3.mp3) |
| 4 | CLUB STAB | JERSEY CLUB | chords | 5 | short, no-sustain brassy stab | [MP3](evidence/dx7-candidates/cand-4.mp3) |
| 5 | CLUB BOUNC | JERSEY CLUB | bass | 1 | punchy decaying bass with a small pitch kick | [MP3](evidence/dx7-candidates/cand-5.mp3) |
| 6 | DIGI PLUCK | DIGICORE | chords | 9 | bright, bitty pluck with 5×, 11× and 17× modulators | [MP3](evidence/dx7-candidates/cand-6.mp3) |
| 7 | DIGI GROWL | DIGICORE | bass | 5 | half-ratio sub plus a feedback-7 growl pair, slight tremolo | [MP3](evidence/dx7-candidates/cand-7.mp3) |
| 8 | IDOL KEYS | IDOL POP | chords | 5 | bright tine electric-piano design (14× tine, velocity-sensitive) | [MP3](evidence/dx7-candidates/cand-8.mp3) |
| 9 | IDOL BASS | IDOL POP | bass | 5 | round finger-style pluck bass | [MP3](evidence/dx7-candidates/cand-9.mp3) |

Levels from `levels.txt`: candidate RMS is 272–1171, against 941–3070 for the current factory eight under identical
playback. The short stabs and the two-carrier basses (CLUB STAB, DIGI GROWL, DIGI PLUCK) are the quietest. Before any
of these replace the bank, set a level trim per voice by listening, using operator output levels or a track level.
Nobody has listened to these on an FM-1 speaker yet. Treat them as first drafts to audition and tweak in BANK, not as
finished sounds.

## Proposed next steps (owner decides)

1. Listen to the ten MP3s next to the current eight
   ([existing auditions](evidence/dx7-bank/)). Keep, tweak or drop each one.
2. If you approve, build the firmware bank from the chosen originals (`gen_dx7_bank.py` with `candidates.packed`), map
   GEN styles to them (each style's chord/bass pair above), and remove the Dexed/SynprezFM voices from release images.
   That closes F1.
3. Optional: add user DX7 SysEx/bank import so owners can load the cartridges they already use (Dexed_cart, FM-1
   factory) themselves, without MIKU redistributing them.

Reproduce:

```bash
python3 tools/dx7_candidates.py
```

```bash
cc -std=c11 -O2 -I. tools/dx7_audition.c firmware/audio/dx7/dx7.c firmware/audio/fmband/fmband_mx.c firmware/audio/fmband/fmband_tables.c -lm -o build/dx7_audition
```

```bash
build/dx7_audition assets/dx7/candidates.packed docs/evidence/dx7-candidates cand
```

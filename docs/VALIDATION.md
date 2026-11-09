# Validation

> Current DX7 milestone: [real six-operator renderer, dedicated BANK/operator editor, embedded patch saving, native video/audio evidence and current measurements](DX7_BANK_HANDOFF.md). This supersedes earlier DX7-plan-only status.


> Current policy and UI: [fixed 64 KiB flash reserve removed at owner request; neon console UI implemented; eight-voice DX7 audition bank prepared offline](NEON_UI_AND_DX7_BANK.md). Earlier fixed-flash-reserve instructions are superseded.


> Hardware/DX7 review: [both known FM-1 flash variants pass emulator persistence checks; DX7 engine reuse is evaluated but not integrated](HARDWARE_AND_DX7_REVIEW.md).


> Current device-wide visual pass: [pink/blue borders and gradients across all nine musical pages, popups and SAVE window, with native recordings and current measurements](evidence/device-theme/README.md). This supersedes the earlier menu-only UI scope and resource figures.


> Current device UI follow-up: [selectable per-song LOOK controls, electronic scenes, recordings and resource measurements](DEVICE_LOOK_HANDOFF.md). This supersedes earlier global-look settings and pre-follow-up audio/resource figures below.


## Current native integration validation

Host Release: **38/38** checks pass. Default, MIDI+UVC and offline video-benchmark
profiles build with the unchanged resource floors. Default/video emulator smoke
runs each captured124928 audio frames with RMS2606 and23 screen colours.
Actual slot10 save/CRC and fresh-emulator-boot playback pass. Native GEN/LYRICS,
paired portrait/body/effects screens and all twelve companion costumes were
exercised. The target expanded-fireworks benchmark produced3 JPEGs with no
failures and no full-scale audio samples; maximum tile1.419ms, maximum frame
39.593ms in the emulator. This is not physical USB enumeration or throughput.

See [current handoff](../CLAUDE_HANDOFF.md),
[durable snapshot](evidence/NEXTGEN_NATIVE_INTEGRATION_SNAPSHOT.json), and
[actual screenshots/listening examples](evidence/native-integration/README.md).
Fresh host tooling requires `python3 -m pip install -r tools/requirements-dev.txt`
(or an equivalent virtualenv); CI installs those dependencies explicitly.

The historical milestones below remain useful for baseline comparisons.

Levels, never interchangeable: **built** (compiled), **host tests**, **emulator** (fm1-emu running the real package),
**hardware** (a physical FM-1).

## Step 0: clean-up (2026-11-03)

### Host
- CMake Release build with `-Wall -Wextra -Wpedantic`: 0 warnings; `ctest` 6/6 pass (core, song_io, band, runtime,
  stage, demo_wav). The stage test's two pixel assertions compared pixels that are legitimately the same colour; fixed.

### Device build
- Dinghy `f77bf03` (upstream fix for the UC25HQ80 flash of newer FM-1s), toolchain `jieli-linux-toolchains-20260730.1`
  (clang 4.0.1, pkgman.jieliapp.com), SDK files `uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin` from
  fw-AC79_AIoT_SDK `d179b44` (AC79NN_SDK_V1.2.1_2023-12-13), all three matching Dinghy's pinned SHA-256.
- `bash native/build.sh --release 0.1`: `miku-fm-0.1.fwsc` 612,223 B (SHA-256 `74a5a32a…a9d40c37`), identity
  **FM-1_39000100**, decoded from the package bytes with Dinghy's `product_of()`. Dev build: identity FM-1_39000000.
- Dinghy's checks all ok: image 55,692 of 581,564 B; RAM .data+.bss 15,904 of 98,304 B; pool 30,720 of 344,064 B;
  `.ram_text` 916 instructions, no calls; register access in hal/ only.
- Store magic "MIKU" (0x554B494D) in place of Dinghy's "DNGY": Miku-FM never parses Dinghy's or a fork's settings.
- Identity: no identity starting `FM-1_39` on the Hub's live table (rendered with Playwright, 124 identities) and no
  `FM-1_39` in public GitHub code. FM-1_940/949, used before, belong to sloopDX and SPAMPY.

### Emulator (joakimeriksson/fm1-emu 9de1ef0)
- `tools/emu_smoke.sh build/native/miku-fm-0.1.fwsc`: boots, PLAY sounds (124,928 frames, RMS 1857), the screen
  draws (8 colours), no faults.
- Save/reboot: REC on steps 1 and 2, SAVE, flash dumped; a fresh boot with that flash restored shows steps 1–2 as
  rests, the rest of the melody unchanged. Only the settings sector (0xFC000) differs from a boot without SAVE;
  it holds magic "MIKU", length 33 and the melody. (Local two-line fm1-emu patch, not committed: `FM1_FLASH_OUT`
  dumps the chip at the end, `FM1_FLASH_IN` restores one after the package's boot.)
- Site: `tools/site_emu.sh` + `tools/site_front.sh` around `miku-fm-0.1.fwsc`; front page and fm1-emu page load in
  headless Chromium with no console errors, the catalog shows "Miku-FM 0.1", Start boots it to the melody page.
- fm1-emu emulates the CPU and peripherals; it is not hardware. Timing, flash wear and the audio output stage are not
  reproduced faithfully.

## Step 1: formant voice and singer on the device (2026-11-03)

### Host
- `tools/sprite_pack.py --check`: header up to date (65,928 B of pixel data). `ctest`: 6/7 at the time of writing; the
  `fvoice` test failed while the voice was being retuned in the working tree (not part of this step).

### Device build
- `native/build.sh` and `--release 0.2` build. Dinghy's checks ok: image 111,540 of 581,564 B (0.1: 55,692); RAM
  .data+.bss 16,400 of 98,304 B; pool 30,720 of 344,064 B (the face is drawn through Dinghy's canvas buffer);
  `.ram_text` 916 instructions, no calls; register access in hal/ only. `miku-fm-0.2.fwsc` 612,223 B, identity FM-1_39000200.

### Emulator (fm1-emu, same build as Step 0)
- `tools/emu_smoke.sh build/native/miku-fm-0.2.fwsc`: 124,928 frames, RMS 1790, 23 screen colours.
- PLAY at 300 ms: STAGE page with the face's mouth open on the syllable; the first note of the WAV has f0 390 Hz
  (G4 = 392 Hz, -0.5%, autocorrelation at 44.1 kHz). Peak sample 4,915 of 32,767: the voice is quiet.
- Key n7 pressed while stopped: sings C4 (f0 261 Hz).
- Save/reboot: KNOB 1 +10 clicks (V 35 to 55), SELECT +1, a key written there, SAVE, flash dumped; a boot with that flash
  restored shows V55 and the changed first steps (C4 sung at step 1).
- CPU% shown by the firmware: 3% (worst 256-frame block over 5.8 ms, decaying). The emulator's clock is not the
  device's, so this is not a device measurement; the device figure is still to be measured.
- Screens: `stage_singing.png`, `edit.png` (replaced by the four pages of 0.4 in `docs/screens/`).

### 0.3: level and octave (2026-11-03)
- Defaults now match the demo the owner chose (variant A): breath 10, brightness 100, portamento 50 ms, velocity 110;
  app gain ×1.5 with a clamp. fm1-emu: smoke RMS 1985 (0.2: 1226), PLAY peak 8,249 (0.2: 5,906) with the emulator's
  MASTER position; the voice peaks ~16k on the host, so ~24k (−3 dBFS) at full MASTER.
- OCT+ during playback: the melody's f0 went G4 392 → G5 788, then A5 865/882, C6 1026, D6 1131 (vibrato and scoop
  account for the spread); STAGE shows "148 OCT+1". CPU shown: 5% (emulator clock).
- Device build `--release 0.3`: image 113,740 B; identity FM-1_39000300; Dinghy's checks ok.

## Step 2 (0.4): the four-track songwriter on the device (2026-11-03)

### Host
- `ctest`: 10/10 pass (core, song_io, band, runtime, stage, demo_wav, fvoice, song2, fmband, engine); the build has 0 warnings
  (`-Wall -Wextra -Wpedantic`). `tools/sprite_pack.py --check`: header up to date.
- `engine` test (`tests/test_engine.c`): the demo from row 2 for two bars (3.3 s): peak 27,775 of 32,767 (no hard clip),
  playhead on row 4 afterwards; with only VOCAL soloed, the first vocal note has f0 390.3 Hz (G4 = 392.0, -0.4%); `lock`
  holds the sequencer and `unlock` resumes it.
- `tools/song_render` (the same `firmware/app/engine.c` as the audio interrupt) renders the whole demo once through:
  22.5 s, peak 28,916 (soft limiter above 24,576), `otonohako.wav` (not committed).

### Device build
- `native/build.sh` and `--release 0.4`: Dinghy's checks ok. Image 143,596 of 581,564 B (0.3: 113,740), RAM .data+.bss
  29,952 of 98,304 B, pool 30,720 of 344,064 B, `.ram_text` 916 instructions, no calls, register access in hal/ only.
  `miku-fm-0.4.fwsc` 612,223 B, identity FM-1_39000400. RAM: Song 4,462 B, Undo 2,594 B, slot buffer 4,476 B, engine (voice,
  chords, bass, drums, sequencer) a few KB.

### Emulator (fm1-emu, same build as Step 0)
- `tools/emu_smoke.sh`: 124,928 frames, RMS 1392, 22 screen colours (PLAY at 200 ms is audible, the screen is not blank).
- PLAY of the demo from row 1 at 200 ms, 6.5 s (0.4 package): WAV 279,296 frames, peak 10,135, RMS 1,754 (the emulator's
  MASTER position is low); the intro (2 bars, 3.2 s) is chords, bass and drums, the vocal enters on row 3; its first note
  has f0 390.3 Hz at three points 100 ms apart (G4, -0.4%).
- CPU% shown on STAGE (worst 256-frame block as a share of 5.8 ms, decaying): 6% in the intro, 10-13% in the verse, 11% in the
  chorus. **This is the emulator's clock, not a device figure**; the device figure is still to be measured.
- Pages: `docs/screens/{stage,song,pattern,mixer}.png` (STAGE: the singer on "WA" or "DO", dots for the four tracks lit; SONG: row 7 playing, row 8 selected;
  PATTERN: a chorus vocal bar with the playhead; MIXER).
- Editing: PATTERN with DRUMS (keys 1-6 toggle, 7-12 accents) and VOCAL on a silent row (a free pattern is assigned, KNOB 1
  length 2 to 4, ALGORITHM mora "u") looks as designed.
- Save/reboot: on the vocal row 1 (silent in the demo) a key wrote a note at step 4 (a new pattern 9 was assigned), MIXER KNOB 1 +5
  clicks (vocal level 110 to 120), SAVE: the screen says SAVED; flash dumped (`FM1_FLASH_OUT`) and restored (`FM1_FLASH_IN`)
  into a fresh boot of the same package: SONG shows pattern 9 in row 1, PATTERN shows the note at step 4, MIXER shows the vocal level
  at 119 of 127 px-measured (120). (The dump includes the app, so it only boots with the package that made it.)
- Slots: in that boot, hold SAVE + ENV loads the demo ("DEMO LOADED", pattern 9 gone), hold SAVE + FX loads slot 1 again
  ("LOADED", pattern 9 back). The unsaved-changes guard and the empty slot message are coded, not looked at.
- A first version of the SAVE hold took the press edge of a bounce for a new press and saved after a chord; it now tracks its
  own state (`sv_on`); the emulator run above is with the fix.

### Concurrency (design, not measured)
Single-step edits are volatile byte writes in a safe order; structural edits stop the sequencer for a few milliseconds via
`en_lock`; see the comment at the top of `native/dinghy.c`. Not checked on a device, only in the emulator.

### Not validated (Step 2)
- Hardware: the sound of the band, CPU load (the emulator figure above is not a device figure), flash saves of the 4,476-byte
  songs, key and encoder feel, the SAVE hold timing, screen drawing speed against the audio interrupt.
- Long editing sessions while playing; 8 slots filled; a damaged slot (decode refuses it, not exercised on the emulator).
- Pan is not part of the song file (MKS2 v1 has no pan field): it is a device setting saved with SAVE.

### Not validated (Step 1)
- Hardware: sound, CPU load, face drawing speed against USB and key scan, flash save with the longer struct.
- Blink and SF_HAPPY on PLAY are coded but not looked at in a snapshot.

### Not validated
- Anything on a physical FM-1: install from V15, from another firmware, installing another firmware from Miku-FM,
  save across a real power cycle, the newer UC25HQ80 flash.
- The release and Pages workflows on GitHub (written after MPFM1's; not yet run).
- MIDI, USB audio and the USB rescue with Miku-FM.

## Step 3 (0.5): Japanese consonants (2026-11-03)

Design: source-filter, no samples. `fv_note_on_mora(note, cons, vowel, vel)`: per consonant class a table of up to 4 phases
(`tools/gen_fvoice_tables.py`, reproducible) drives per control block (16 frames) the formant shape (vowels plus voice bar,
nasal n, nasal m, flap), voicing, output level, one noise band (LFSR through a 2-pole resonator, added after the envelope)
and aspiration (LFSR through the vowel formants). The consonant starts AT the note start; the vowel is in place
`fv_cons_ms(cons)` (10-66 ms) later, so the vowel onset lands that late (no look-ahead; a sequencer could start the note that
much earlier). Durations are counted in blocks, so they do not depend on pitch or tempo.

### Host (`tests/test_mora_voice.c`, measured on the rendered signal; `ctest` 11/11, 0 warnings in the touched files)
- Plosives (note 55): closure RMS is digital silence (>= 250 dB below the vowel; target 12 dB); burst onset 8.0-13.4 ms;
  burst centroid t 4.4-4.5 kHz > k 1.0 (o), 1.4 (a, u), 2.1 (e), 2.8 (i) kHz > p 0.6 kHz, for all five vowels.
- Fricatives: s centroid 6.0 kHz > sh 3.4 kHz > 2 kHz; 93% (s) and 81% (sh) of the first 40 ms is above 2 kHz (voicing off);
  level -7 dB (s) / -6 dB (sh) re the vowel on "a" (-14/-13 on "u").
- Nasals: energy <500 Hz is 0.90-0.98 of the total at notes 48 and 55 (n, m, ん); level -6 to -8 dB (n, m) and -3 to -5 dB (ん)
  re the vowel. At 72 and 84 the f0 is above 500 Hz, so the measure is <1 kHz (0.97-1.00); level -8 to -10 dB. The murmur
  level is raised with pitch by a table (`fv_nbo`), otherwise it was -15 to -17 dB at notes 72 and 84.
- Periodicity (autocorrelation at the f0 lag, 8-22 ms, note 55): voiced g d b n m 1.00, z j 0.82-0.83; unvoiced k t p s sh h f 0.01-0.12.
- Glides: F2-band centroid of ya 2909 -> 1825, yu 2909 -> 1829, yo 2205 -> 1115 Hz over 4-250 ms; wa F1-band 415 -> 1010; wo F2-band 1558 -> 1113.
- Flap r (legato after a vowel): dip 7.8-9.7 dB, 14-25 ms below -3 dB (limit 30).
- Pitch independence (notes 60 vs 84): `fv_cons()` run-out identical for all classes (exact); the settling time of (mora minus
  plain vowel) agrees within 1.3 ms for all classes except y and w (not checked: the soprano zone tables bring i, u and the vowel closer at 84).
- Vowel after the consonant equals the plain vowel: worst harmonic level difference 0.015 dB (all consonants x all vowels).
- Mouth: closed (5) during m, b, p closure and the whole ん hum, the vowel mouth for the others (k s sh t ch ts n h f y r w g z j d).
- Legato after a sustained "a": the consonant is articulated (dips 12 to 78 dB deep) and the envelope is not re-triggered; note-off
  during a consonant ends cleanly.
- Stress: 30 s of random morae x notes 48-96 x legato/non-legato (799 notes, velocity 64-127): peak 24,877, no sample at the clip, deterministic (bit-identical hash).
- Existing tests unchanged. One behaviour change: the vowel morph now arrives exactly (it used to stall a few LSB short), so
  steady vowels differ from 0.4 by a rounding amount.
- `fvoice` with consonant NONE was bit-identical to 0.4 before that rounding fix (hash of 40 notes).

### Device
- Compile gate (`pi32v2/bin/clang -target pi32v2 -Os -ffunction-sections -fno-builtin -Wall`): no warnings, no libc calls;
  `nm` shows only module symbols undefined in engine.o, nothing in fvoice.o. fvoice.o .text 1,900 -> 3,058 B, .rodata 4,976 -> 6,492 B;
  engine.o .text 1,422 -> 1,444 B. `FvVoice` is 376 B. mora.c unchanged.
- Per-sample multiplies (read from the code): 26 before; 28 always now (voicing, aspiration); 32 while a noise band sounds
  (noise resonator 3 + level 1), i.e. in the 5-65 ms of a consonant. Block work: 18 formant coefficients with an extra branch each, a few eases.
- Host, `fv_render` 256 frames, -O2, noisy machine, best of 7: 5.8 us (0.4) -> about 6.5 us steady vowel, 7.2 us with the noise band.
- `native/build.sh --release 0.5`: Dinghy's checks all ok: image 146,172 B of 581,564 (0.4: 143,596), RAM .data+.bss 30,032 B of
  98,304 (0.4: 29,952), pool 30,720 B, `.ram_text` 916 insns, no calls. `miku-fm-0.5.fwsc` 612,223 B, identity FM-1_39000500.

### Emulator (fm1-emu)
- `tools/emu_smoke.sh`: 124,928 frames, RMS 1504, 22 screen colours.
- PLAY of the demo at 200 ms, 11.5 s: STAGE CPU 11% on the verse (R4), 12% in the chorus (R7) (0.4: 10-13% / 11%).
  **This is the emulator's clock, not a device figure**; the device figure is still to be measured. No screenshot of m/b/p/n lips caught.

### Not verified
- Nothing was listened to (no audio output here); levels and timbres are tuned by measurement and spectrograms only. The weakest
  guesses: h, fu, the voiced fricatives and the nasal murmur timbre. Not tested on hardware.
- Renders: `tools/mora_render` (every mora, note 72), `tools/song_render --solo vocal --rows 2 5` (verse vocal alone).

## Crackle fix: plosive release bursts (2026-11-03)

Owner: white-noise crackle in the vocal solo, about every other syllable. Diagnosis in `docs/HANDOFF.md`: the k/t/p release
bursts were 6-7 ms blocks of flat noise at the vowel's level (higher than the vowel on high notes). A burst phase now reaches
its level within one block and decays exponentially to the next phase (`bst=1` in `tools/gen_fvoice_tables.py`; per-phase decay
in `fv_cpn`); k/t/p burst starts 0.55/0.50/0.50 -> 0.32.

### Host
- `t_burst` (k t p x a i u e o x notes 48/72/96 x fresh / over the release of a previous note), level re the power mean of the
  five vowels on that note, pooled: k -9.3/-10.7/-7.3, t -8.9/-10.3/-6.9, p -9.7/-11.1/-7.7 dB (limit -6; each burst < -3);
  first-third over last-third energy k 5.5, t 7.6, p 6.2 dB (limit 4). The 0.5 voice fails it 67 times (bursts at -2 dB that grow).
- Demo vocal solo, burst RMS re the following vowel (k 69, t 69, k 72, t 76, t 81): before -0.1/-0.6/-3.5/+4.9/+0.8 dB,
  after -6.3/-6.6/-11.2/-1.8/-6.0 dB. Strong >16 kHz events 21 -> 12 (the rest are burst onsets). No events in vowels before or after.
- The other plosive checks hold: closure is digital silence, burst onset 8.0-13.2 ms, centroids t 4.6 > k 1.0-2.6 > p 0.6 kHz.
- Stress (30 s random morae): peak 24,877, no clip, deterministic (new hash, as the bursts changed). `ctest` 11/11, 0 warnings.

## Step 4: song generator, GEN page (2026-11-03)

Design: `firmware/song/gen.c` (integer, no libc, deterministic). `gen_song` writes chords from a diatonic progression per
section (6 per scale, verse and chorus different), then bass, melody and drums against the chord of each row as it is in the
song; patterns with the same musical context are shared (a 20-row song uses 7-10 patterns per track). `gen_phrase` rewrites
one pattern with another variation. Locked tracks are never touched; with a lock the arrangement stays.

### Host (`firmware/song/tests/test_gen.c`; `ctest` 12/12, 0 warnings)
- 288 songs (4 styles x 2 scales x 12 keys x 3 seeds): every song encodes and decodes (all fields in range); 20 rows,
  intro/verse/chorus/outro in place, vocal silent in intro and outro; every chord root, melody and bass note in the key;
  bass on the chord root or fifth; melody 51..82 (MIDI); on beats 1 and 3 the melody is a chord tone in 8996 of 8996 notes.
- Deterministic: same settings, same song, whatever song was there before (the mix levels stay); seeds 1234 and 1235 differ.
- Locks (over the demo): each locked track keeps its patterns and rows byte for byte, the others change, name and tempo stay;
  with the demo's chords locked, the new melody is on a chord tone on all 16 strong beats. All four locked: nothing changes.
- Phrase: variation 0 equals what gen_song wrote; another variation changes that one pattern only (other patterns, tracks
  and rows identical) and is reproducible; a silent row gets a free pattern; a locked track and a row past the end are refused.
- Renders (`song_render --gen STYLE KEY SCALE SEED`): POP C major 1, BALLAD A minor 7, DANCE A minor 42, CHIP G major 300:
  35-60 s, peak 28,570-32,518, no sample at full scale (3 samples above 32,000 in the ballad).

### Device
- Build: Dinghy checks ok; image 146,440 -> 152,924 B; RAM .data+.bss 30,032 -> 34,544 B (4,462 B of it the take backup).
- fm1-emu (scripted keys): HOME x4 opens GEN; K1/K2/K3 set DANCE, A, MIN; SCL twice locks CHORDS (shown dark); REC "NEW SONG";
  PLAY sounds; ARP on CHORDS "TRACK LOCKED"; FX, ARP "NEW PHRASE"; GLO "DROPPED" and playback goes on. emu_smoke unchanged
  (RMS 1504, 22 colours).
- Not checked: the sound of generated songs on hardware; how long REC takes on the device (on the host it is well under 1 ms).

## The voice's home: STAGE knob banks, effects, character, languages (2026-11-04)

Design: `firmware/audio/vfx/` (drive -> tremolo -> chorus -> delay -> reverb on the voice, integer, buffers in Dinghy's
`.pool`), voice character as five formant tables (`fv_vow[5]`, factors 0.84 .. 1.18; MIKU = the old table bit for bit),
languages in `firmware/app/engine.c`, all settings in the song (`Voice`, MKS2 version 2; version 1 still read).

### Host (`ctest` 12/12, 0 warnings)
- Default voice: the demo render is bit-identical to before (`cmp`); every effect at 0 again gives exactly the dry voice.
- Each effect at 60 (vocal solo, two bars): level drive +1.7, tremolo -2.1, chorus +0.4, delay +0.4, reverb +0.3 dB re dry;
  chorus / delay / reverb are stereo (left-right difference 1600-1900 rms), drive and tremolo mono; no sample at full scale.
- Tails: reverb, delay and chorus at 100, stop, 6 s later the output is exactly 0 (feedback multiplies round toward zero).
- Character DEEP..TINY: spectral centroid of G2 averaged over the five vowels 393, 402, 422, 452, 460 Hz (rises).
- Languages on a held 2 s note: JAPANESE 1 syllable, ANIMAL 24, SCAT 9 (eighths at 120 BPM), VOWELS 1 and no consonant.
- MKS2 v2 round-trips; a v1 file loads with the default voice; wrong size/version pairs and out-of-range voice fields are refused.

### Device
- Build: image 171,652 -> 177,564 B; RAM .data+.bss 34,608 -> 34,752 B; .pool 30,720 -> 114,308 B of 344,064 (effect buffers).
- fm1-emu: SELECT moves the syllable wheel; FX bank, reverb 36, drive 10, character TINY, ANIMAL, LFO bank, tremolo 60 all
  show and sound; emulator CPU on STAGE 6% (dry) -> 19% (all effects + ANIMAL). Not a device figure: measure on hardware.
- Slots: a version-1 song in a slot loads (size fallback in `sload`); saving writes version 2.

## STAGE visuals and knob curves (2026-11-04)

- Syllable dial: the top of a dial (centre below the screen, radius 150 px) with five syllables on its rim, ticks and a pink
  pointer; it eases to the new syllable in about 100 ms (30 ms frames). Knob turns show a 270-degree ring gauge, the name and a
  picture for each of the 16 knobs plus character, language and tempo. All checked in fm1-emu screenshots.
- Knob curves: chorus/delay/reverb wet and drive gain follow v*v (0..100); attack and release v*v/20 ms (0..500), vibrato
  delay v*v/10 ms. Defaults moved to keep the old sound: attack 13 (8 ms), release 35 (61 ms), vibrato delay 45 (202 ms).
  Engine test at 60: drive +3.1, tremolo -2.1, chorus/delay/reverb +0.1..0.2 dB re dry; `ctest` 12/12.
- Device build: image 179,9xx B, 0 warnings; emu_smoke unchanged (RMS 1504).

## Hold-a-button layers: SOURCE, FX DEEP, ARTICULATION, HARMONY (2026-11-04)

A long press (0.4 s, it acts while still held) on SEL / FX / ENV / LFO latches a second knob layer (its button blinks);
settings in the song (MKS2 version 3, 32-byte voice block; versions 2 and 1 still read). At neutral (the defaults) every
new control is exact: the demo, the vocal solo, ANIMAL, SCAT/TINY and a generated song render bit-identical to main.

### Host (`ctest` 12/12, 0 warnings; `tests/test_engine.c` "layers")
- Tension 0/50/100: harmonic centroid 648 / 1175 / 1258 Hz. Fry 80: power at f0/2 0 -> 0.047 of f0. Whisper 100: f0 power
  -27 dB, level +0.5 dB. Formant 0/50/100: 402 / 420 / 449 Hz (G2, five vowels).
- Delay 1/16 and 1/4 at 120 BPM: first echo 125-150 and 510 ms after the note (10 ms windows). Reverb size 0/100: tail 1 s
  after the note 0 / 63 rms. Delay line 1.49 s (a quarter fits down to 44 BPM); the right echo comes 15 ms after the left.
- Consonant length 0/100: "k" lasts 27 / 192 blocks. Consonant level 0: the k burst 1421 -> 25 rms.
- On-beat 0/100: the vowel of "ka" 45.9 / 5.8 ms after the kick (the band waits 66 ms, the note starts 40 ms early).
- Harmony +7: power at the fifth x750,000. Everything at 100 (all effects, harmony +12 at full mix, on-beat, fry, tension,
  all levels 127) over 20 s of the demo: peak 32,663, no sample at the cap; after stop no voice gated, the queue empty.
- Bug found and fixed on the way: the engine applied commands (a live note) before reading the song's voice settings, so
  the first live note started with the previous block's settings. Settings are now read first.
- Cost (host, relative to the default demo): effects all at 100 x1.6, harmony x1.45, on-beat x1.05, source + ANIMAL x1.1,
  everything at once x2.4. Device estimate from the owner's 19 % (0.5, voice only): about 30 % with typical effects, about
  45 % with absolutely everything at maximum. The reverb runs at half rate and the LFOs update every 8/16 frames to get there.

### Device
- Build: image 183,788 B, RAM .data+.bss 35,492 B, .pool 254,784 of 344,064 B (effects 147 KB, band delay 64 KB, mix buffers).
- fm1-emu: long presses open SOURCE, FX DEEP, ARTICULATION, HARMONY (each with its four-knob list); every new knob shows its
  picture; stepped knobs step (DLY TIME 1/8., 1/8; INTERVAL OFF, -12, +3, +4, +5; HARM VOICE MIKU -> LOW); short presses keep
  their job (SEL: next language; LFO: the LFO bank). Playing with harmony, on-beat and the source layer: emulator CPU 7-8 %,
  output peak 8,911, no clip, stereo. emu_smoke unchanged.



## Next-generation feature branch (2026-11-08)

Source baseline `88b0ef3`; branch `feature/miku-nextgen-performance`. Details: [development log](NEXTGEN_DEVELOPMENT.md).

- Host Release: **19/19 CTest pass**, no build warnings. New tests: real native SELECT/rate handler against actual engine, every native DANCE atlas pixel, voice pitch/vowel compensation, lyrics/articulation, contemporary GEN, modern instrument baselines/envelopes, pedal routing/analysis and MIDI-safe performance packet. Packet assertions remain enabled in Release.
- `gen_fvoice_tables.py`, `gen_fmband_tables.py`: deterministic, no generated diff. `python3 -I tools/sprite_pack.py --check`: up to date,65928 pixel bytes. `git diff --check` passes.
- Actual `native/build.sh` with pinned Dinghy/JieLi/SDK: **image185208/581564 B**, **static RAM35540/98304 B**, **pool254784/344064 B**. Baseline183788/35492/254784; deltas+1420/+48/0. Register checks and916-instruction call-free `.ram_text` pass. Prototype host-only modules are not included in these linked figures.
- Final `emu_smoke.sh`:124928 frames, RMS1504.38, peak7393, zero full-scale samples,57LCD frames. Emulator ISR8.6%. DANCE:93952 frames, peak9235,55LCD frames, ISR10.3%, corrected original sprite inspected. MIXER:151296 frames, peak1297, ISR3.8%, HARD ACID selection visually confirmed. These are emulator measurements, not physical ISR time, DAC output or deadline guarantees.
- Targeted phase1 emulator SELECT: idle old preview peak7887/RMS1917.1 versus new45568 frames exact silence. Held C4 with two SELECT changes is bit-identical to no-turn reference across89600 stereo frames; final4000 frames silent after release. ANIMAL RATE66 screenshot retains BPM148 and language; release silent. PATTERN's intentional preview remains separately guarded.
- ANIMAL engine rendered four-second note:51→23 onsets, mean79.31→177.91ms; old70–89.71/new141.22–219.82ms; peaks24297/22592, zero full-scale samples. Timing/release/language-isolation tested at sample resolution. Cadence remains provisional pending owner listening review.
- Voice comparison: all five vowels/MIDI48–96; opt-in bounded gain with unchanged oscillator phase and no new clipped samples. Existing raw U92 overload remains. A96 RMS614→1228; high-note loss is reduced, not solved. No listening-quality claim.
- Lyrics actual voice audition:2.18s, peak16909/RMS3718, no cap hits; sample-indexed mouth/consonant CSV. Contemporary GEN:360 configurations roundtrip/determinism/locks/local regeneration; ten WAV/MKS review examples, zero full-scale samples at candidate master80. Band A/B and original-preset hashes documented in [band](NEXTGEN_BAND.md).
- Pedal ASan/UBSan tests pass, unity WAV byte-identical, pre-FX analysis independent of output mute, disconnect flushes input state. Effects render peak28655 with no cap hits; levels are not loudness matched. Live USB transport not implemented.
- Experimental pi32v2 compile-only: lyrics638 B text, GEN3254 B text/rodata, pedal870 B text, performance wire588 B text, zero static data/BSS; no libc/float dependencies. GEN uses5032B stack scratch and must be reworked before native inclusion. Wire struct assignment initially lowered into memcpy; explicit scalar copy fixed it, verified no undefined symbols.
- Companion Chromium/Playwright review passed: real-engine JSON/WAV import, seek/state display, original atlas load, invalid file rejection and no page errors. Screenshot `build/evidence/companion.png`. Recorded engine state is not live USB; full-body mouth compositing remains unfinished.

Evidence under ignored `build/evidence/` and `build/nextgen/`: WAVs, editable generated songs, PNGs, CSVs, source/package hashes, build logs and resource JSON. `tools/nextgen_review.py` assembles `build/evidence/review.html`. Individual NEXTGEN documents contain regeneration commands.

Not measured: physical-device audio/ISR load, real USB endpoint throughput/underruns/latency, simultaneous audio/MIDI/UVC descriptors, UVC frame rate, final perception/genre authenticity or approved visual design. No device flashed; no release, live website deployment or main merge.


## Compact wardrobe and native UVC follow-up (2026-11-08)

- Host Release26/26 after adding compact codec/generator, real native LCD draw comparison, bounded UVC class, incremental JPEG, real native video-producer tests and benchmark-listing decoding. Separate `python3 tests/test_native_video.py` compiles actual patched pinned Dinghy USB code against a fake SIE: descriptors/EP0 Probe/Commit/EP4 payload/DMA ownership/alt/reset/suspend/MIDI rescue all pass. Neither fake-SIE nor emulator proves real host enumeration.
- All original131848 portrait/dance pixels, palettes, dirty rectangles and scaled LCD output remain exact. Compact generated data32884B versus65928B. Native default image152696B/static35540B/pool254784B:32512B image saved, zero extra static/pool.
- Optional native `--video`: image159080B/static33732B/pool265216B; original loader9067B retained. This profile disables UAC/CDC, keeps MIDI and adds UVC160x120 nominal15fps. Default USB behavior is unchanged. No combined playback/capture/MIDI/UVC claim.
- Minimum free reserves now checked in native builds: image65536B/static16384B/pool32768B. Video profile retains422484B/64572B/78848B. These are development floors, not measured future feature allocations or runtime stack guarantees.
- Compact default and video package emulator smoke both124928 frames/RMS1504/23screen colors. Compact DANCE:58624 frames,35LCDframes, emulator ISR7.5%; image visually inspected. Video idle-source build still synthesizes normally without a host.
- Integer JPEG26/26 fixtures independently decode; original/compact renderers yield identical bytes. Actual scene sizes3475–5027B. Real JPEG3794B packetizes into8 UVC packets and reassembles byte-identically. Encoder/context2240B plus single8192B output consumes10432B pool; no video framebuffer/core1 allocation.
- Target `--video-bench` ran actual native audio+video code:3 JPEGs,0failures, recovered3794B image byte-identical to host reference. Emulator max MCU1.403ms/maxframewall32.206ms,totalISR13.1%;80640 audioframes,peak9653,zero fullscale. This is offline encoding, not delivered USB video FPS or physical-device timing. Benchmark parsing and reproduction tools committed.
- Tables/sprites regenerated/check unchanged; sanitizer checks for codec/JPEG/class pass (LeakSanitizer unavailable under ptrace; those modules allocate no heap). No hardware flash, release, deployment or main merge.

Review assets: revised eleven-outfit board includes corrected07 Signal Suit,08 Summer Beat,11 Moon Circuit witch; both face and dance requirements retained. These are concepts, not eleven completed native outfits. See [headroom report](NEXTGEN_MEMORY.md) and [wardrobe](NEXTGEN_WARDROBE.md).

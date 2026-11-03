# Validation

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


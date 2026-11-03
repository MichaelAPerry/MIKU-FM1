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
- Screens: `docs/screens/stage_singing.png`, `docs/screens/edit.png`.

### 0.3: level and octave (2026-11-03)
- Defaults now match the demo the owner chose (variant A): breath 10, brightness 100, portamento 50 ms, velocity 110;
  app gain ×1.5 with a clamp. fm1-emu: smoke RMS 1985 (0.2: 1226), PLAY peak 8,249 (0.2: 5,906) with the emulator's
  MASTER position; the voice peaks ~16k on the host, so ~24k (−3 dBFS) at full MASTER.
- OCT+ during playback: the melody's f0 went G4 392 → G5 788, then A5 865/882, C6 1026, D6 1131 (vibrato and scoop
  account for the spread); STAGE shows "148 OCT+1". CPU shown: 5% (emulator clock).
- Device build `--release 0.3`: image 113,740 B; identity FM-1_39000300; Dinghy's checks ok.

### Not validated (Step 1)
- Hardware: sound, CPU load, face drawing speed against USB and key scan, flash save with the longer struct.
- Blink and SF_HAPPY on PLAY are coded but not looked at in a snapshot.

### Not validated
- Anything on a physical FM-1: install from V15, from another firmware, installing another firmware from Miku-FM,
  save across a real power cycle, the newer UC25HQ80 flash.
- The release and Pages workflows on GitHub (written after MPFM1's; not yet run).
- MIDI, USB audio and the USB rescue with Miku-FM.

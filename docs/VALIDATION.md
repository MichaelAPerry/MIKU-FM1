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

### Not validated
- Anything on a physical FM-1: install from V15, from another firmware, installing another firmware from Miku-FM,
  save across a real power cycle, the newer UC25HQ80 flash.
- The release and Pages workflows on GitHub (written after MPFM1's; not yet run).
- MIDI, USB audio and the USB rescue with Miku-FM.

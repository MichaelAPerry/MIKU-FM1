# Emulator-first device integration (current blocker)

Full-system emulator: https://joakimeriksson.github.io/fm1-emu/
MONK-1 has a separate WebAssembly app preview: https://charlesvestal.github.io/fm1-monk/emu/

These are not interchangeable: the full-system FM-1 emulator consumes installable FM-1 firmware, while the MONK browser preview compiles MONK's source for WebAssembly.

## Verified by source inspection

MONK-1 firmware (https://github.com/charlesvestal/fm1-monk) includes
`firmware/src/main.c` and `app/main_fm1.c`. It drives an audio DMA
half-buffer of 256 stereo frames at 44,100 Hz, using int32 sample words
with 24-bit amplitude; it has a 240x240 framebuffer/panel platform layer,
flash storage, and an OTA packaging script.

Its build requires the external JieLi pi32v2 compiler and JieLi AC79 SDK
(see upstream BUILDING.md). MONK's firmware is GPL-3.0-only, so it cannot
simply be transplanted into this MIT repository while retaining MIT as
the license for the combined derived firmware.

## Implemented in this repository

`firmware/platform/fm1_runtime.c` now exposes
`miku_runtime_render(..., int32_t *stereo, 256)` at 44,100 Hz with
track-event scheduling and 24-bit PCM words suitable for integration
into an existing FM-1 audio-half callback. Host tests exercise repeated
256-frame renders.

This is an **audio callback adapter, not a device HAL, firmware image,
or emulator test**. No screenshot or boot is asserted.

## Next required work (blocking emulator milestone)

1. Select a GPL-compatible device HAL path and document corresponding
   redistribution terms; keep original MIT core notices intact.
2. Implement the actual PI32v2 reset/boot, LCD, buttons, encoder, flash,
   USB update and DMA interrupt using a supported upstream platform.
3. Link this runtime into that build and produce a `.fwsc` with JieLi
   tools, then inspect size/map and watchdog startup.
4. Boot the package in the **existing full-system FM-1 emulator** and
   test UI, controls, synthesis and song persistence. Hardware flashing
   is strictly a separate validation gate.

The unavailable external toolchain and missing board application are
currently explicit blockers, not evidence that firmware works.

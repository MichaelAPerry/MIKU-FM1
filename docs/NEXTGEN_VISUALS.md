# Performance visuals foundation

Current integration status: [NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md)
and [CLAUDE_HANDOFF.md](../CLAUDE_HANDOFF.md) supersede earlier original-only,
host-only, provisional source-grid and memory figures in this milestone document.

The new `firmware/ui/performance.c,h` module separates scene geometry, original outfit metadata, and animation state from LCD writes. It is usable by the native renderer or a computer companion using the same C module. It does not create an independent synthesizer or duplicate character pixels.

Only `ORIGINAL` is registered: atlas ID 0 references the existing 61×71, eight-frame `SINGER_STAGE` atlas at 3×, position (28,0). All source art and the packed header remain intact. The atlas retains its Piapro Character License requirements; see [LICENSING.md](../LICENSING.md). Additional outfit IDs require separate owner-approved art and renderer support. Invalid selections return failure without changing the state.

## State and renderer contract

`perf_update` accepts absolute milliseconds, absolute musical beat position Q8 (including fractional beat), playing, section, actual engine mouth/vocal envelope, audio level and transient strength. It returns sprite, mouth, stage and status dirty flags. It never draws, reads the USB stack, or touches the audio ISR. Feed state from the main loop; draw only as bandwidth permits. Frame selection depends on absolute time/beat position, not how many screen redraws fit.

The default animation preserves the existing gestures: four idle frames on the beat, two singing gestures while the voice level is >=40, arms on odd chorus beats (section 3), 1500 ms bow after stop, live singing gestures at 250 ms and stopped sway at 700 ms. Clock-wrap tests cover the bow deadline. Mouth state comes from the real voice and is clamped to the supported five vowels plus closed state; the existing body atlas has no separate mouth variants, so a future companion must apply that state using an approved mouth layer. Do not claim that the eight full-body frames themselves provide vowel-specific lip sync.

`perf_pixel(state,x,y)` returns RGB565 for the scene underneath transparent sprite pixels. It is constant original navy (12,20,40) at default settings. It supports procedural GRID and HORIZON exploration backgrounds, CORNERS and RAILS frames, and original cyan or amber exploration palettes. This tiny registry is an implementation exercise for review, not an approved visual lineup or genre mapping. Footer/status pixels (y >=214) belong to the caller.

Audio-reactive light takes bounded incoming level/transient values plus an absolute beat pulse. It has no simulated audio signal. Level analysis must be supplied by the actual firmware audio routing; this module performs no audio analysis itself. The scene renderer uses a fixed number of operations per pixel, with no framebuffer allocation, loops with data-dependent limits, floating point, sprite expansion, or storage writes.

## Integration example

```
PerfState scene; /* persistent main-loop state */
perf_init(&scene);
/* on each state update, independently of LCD redraw timing: */
PerfInput in = { now_ms, beat_q8, playing, section,
                 engine_mouth, engine_level, audio_level, transient };
uint32_t dirty = perf_update(&scene, &in);
/* renderer uses scene.sprite to index perf_outfit(scene.outfit)->atlas,
   and perf_pixel for transparent pixels. LCD byte order belongs to driver. */
```

A full theme change must repaint the area outside the sprite rectangle too. Reacting to every light-level change with a full LCD redraw is not recommended until bandwidth is measured. No default native control mapping is selected by this module.

## Reproducible previews and tests

```
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_performance.c firmware/ui/performance.c -o build/performance_tests
build/performance_tests
cc -O2 -Wall -Wextra -Wpedantic -I. tools/performance_preview.c firmware/ui/performance.c -o build/performance_preview
build/performance_preview build/evidence/stage
```

The tool writes 240×240 PPM previews (`stage-original`, `stage-grid`, `stage-horizon`) from actual packed firmware sprites plus the shared C scene module, with a black caller-owned footer. They are concept previews, not emulator screenshots or new character designs. PNG copies were generated under ignored `build/evidence/` for review. The preview correctly uses flattened pixel parity for the odd-width 4bpp atlas.

Tests pass with `-Wall -Wextra -Wpedantic`, no warnings. They verify default background pixels, atlas dimensions/frame count, invalid-selection atomicity, real mouth/level mapping, chorus gestures, independent redraw cadence, bow clock rollover, all background/frame/palette combinations, and footer ownership.

Host `cc -Os` module object: 1073 bytes code/constants +24 bytes registry data, no bss. Each `PerfState` is 24 bytes. No new sprite bytes or pool allocation. Actual JieLi flash, static RAM placement, emulator CPU/display throughput and hardware measurements remain the coordinator's integration gates. No physical FM-1 was flashed.

## Owner decisions still open

- Whether GRID/HORIZON, either frame, or the amber exploration palette suit MIKU.
- New costume concepts and artwork; only the existing original outfit is implemented.
- A device control mapping and whether stage themes persist independently or in songs.
- Any genre-to-theme mapping and choreography beyond the original eight-frame vocabulary.
- Companion resolution, renderer, mouth layer and animation designs.

Native DANCE now uses this shared state module, updated once per main-loop pass. The original outfit/background remain the device defaults. Background/frame candidates are preview-only pending review. The odd-width sprite decoder was also fixed to use linear pixel parity; a separate test checks every packed atlas pixel.

Next engineering task: measure full-theme redraw cost, then expose review-only theme previews through a deliberate control once its behavior is approved.

Follow-up: native portrait/dance drawing now uses the lossless compressed atlases in `singer_compact.h` and `sprite_codec.c`, preserving exact pixels with a32512B linked image reduction. The optional native video profile shares this atlas. See [memory report](NEXTGEN_MEMORY.md) and [wardrobe revision3](NEXTGEN_WARDROBE.md).

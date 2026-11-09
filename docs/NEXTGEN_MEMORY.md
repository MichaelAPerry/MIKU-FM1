# Wardrobe and USB-video headroom (2026-11-08)

> Current device-wide visual pass: [pink/blue borders and gradients across all nine musical pages, popups and SAVE window, with native recordings and current measurements](evidence/device-theme/README.md). This supersedes the earlier menu-only UI scope and resource figures.


> Current device UI follow-up: [selectable per-song LOOK controls, electronic scenes, recordings and resource measurements](DEVICE_LOOK_HANDOFF.md). This supersedes earlier global-look settings and pre-follow-up audio/resource figures below.


## Current integrated build

All twelve outfits, native lyrics/GEN, ten slots, four factory songs and effects
are linked. Actual image/static/pool usage: default **504416/35728/255624** bytes;
MIDI+UVC **510900/33924/266080** bytes. The video profile retains **70664** image
bytes, **64380** static bytes and **77984** pool bytes free. The unchanged floors
remain 65536/16384/32768 free. No item was dropped. Additional wardrobe packing
is335463 bytes; nearby-row deltas saved4304 bytes without changing pixels or
adding decoder depth. See [current snapshot](evidence/NEXTGEN_NATIVE_INTEGRATION_SNAPSHOT.json)
and [native integration notes](NEXTGEN_NATIVE_INTEGRATION.md).

The following figures describe the earlier original-only compression baseline.

The owner authorized compact wardrobe work and adjustable outfit counts while retaining overhead for USB video. These are actual linked native measurements, not sums of host object sizes.

| Build | Image /581564 B | Static RAM /98304 B | Pool /344064 B |
| --- | ---: | ---: | ---: |
| Before compact artwork |185208|35540|254784|
| Compact default |152696|35540|254784|
| Compact MIDI + UVC development profile |159080|33732|265216|

The default firmware saves **32512 bytes** after decoder code/metadata are linked. The compressed original pixel data and row maps total32884 bytes versus65928, a33044-byte storage saving. Both twelve-face and eight-dance atlases are represented exactly, including palettes, transparency and portrait dirty rectangles. No frame or color was removed. Existing imagegen sources and original packed header remain unchanged.

The video profile has **422484 bytes of image space,64572 bytes of static RAM space and78848 bytes of pool space free**. Static RAM is lower because this isolated profile disables the existing USB audio-capture implementation; it retains MIDI and adds UVC. This does not establish simultaneous USB audio input/output/MIDI/video support. The normal build retains its existing USB behavior.

## Allocation choices

- LCD portraits and dancing sprites decode one row into a90/61-byte local array. There is no expanded animation framebuffer.
- JPEG source reads the same compressed dance atlas as the LCD. Linker symbols confirm the original raw `singer_face_*` and `singer_stage_*` arrays were discarded from native video. The original header supplies only compile-time enums/dimensions.
- Optional video allocates one8192-byte JPEG output and2240-byte encoder context in the pool:10432 bytes total. No second framebuffer or core1 job/stack is introduced. One immutable frame remains owned by USB until DMA completion/flush; no encoding into that buffer while busy.
- The encoder advances at most one16x16 MCU per main-loop pass. Audio and USB interrupts never render or encode images. No blocking lock, allocation, screen or flash operation is added to audio ISR work.
- The current actual160x120 JPEG scenes occupy3475–5027 bytes at quality65. The advertised8192-byte cap is a bound, not the expected average. No future costume/video-scene size guarantee follows from these fixtures.

## Enforced development margins

`config/resource_budget.json` currently requires at least65536 bytes of image space,16384 bytes of static RAM space and32768 bytes of pool space free **after linked allocations**. `native/build.sh` runs `tools/check_resources.py` against the real app binary and linker symbols and refuses a profile below those margins. Sidecar `*-resources.json` files record used/free/reserved/margin figures.

These are conservative development floors for further USB routing, buffers, visual code, effects and integration work; they are not measured future allocations or a promise that every simultaneous USB configuration fits. Static figures also do not measure worst-case runtime stack use. Changes to the budget must be deliberate and recorded, not silently waived to fit more artwork.

New outfits must be packed and measured with matching portraits/dance frames before choosing the final count. The existing outfit's compression ratio cannot simply be multiplied: distinctive hats, braids, clothing, palettes and dance motion may require more unique data. The eleven revised wardrobe concepts remain review art, not eleven native skins.

## Validation

Host row/pixel tests compare all131848 original pixels and every palette/dirty rectangle. The native LCD adapter test executes actual face/dance draw functions and checks every scaled pixel plus odd-origin portrait dirty rectangles. Codec corruption/capacity tests, deterministic generator checks and sanitizer tests pass. Actual native default and video builds pass register-access and call-free `.ram_text` gates. Both packages pass `emu_smoke.sh` with124928 frames/RMS1504/23 screen colors. The compact DANCE screenshot also displays correctly.

See [video class](NEXTGEN_VIDEO.md), [native adapter](NEXTGEN_VIDEO_NATIVE.md), [integer JPEG](NEXTGEN_VIDEO_RENDER.md), [sprite format](NEXTGEN_SPRITE_PACK.md), and [wardrobe concepts](NEXTGEN_WARDROBE.md). Real USB host enumeration, delivered frame rate/latency, hardware ISR/stack measurements and USB audio/video coexistence remain unverified. No physical device was flashed.

## Target encoder emulator benchmark

`--video-bench` encodes exactly three frames while a live vocal and backing song are active, then leaves the last JPEG untouched for memory inspection. It bypasses the USB sender and is not a real streaming test. The target binary completed all three frames with zero encoder failures. Its3794-byte recovered JPEG is byte-identical to the host original-atlas reference and decodes correctly to160x120.

Measured on fm1-emu: maximum single tile1.403ms; maximum complete frame wall time32.206ms (includes intervening main-loop/interrupt activity); total ISR13.1%;80640 captured audio frames, peak9653, no full-scale samples. These measurements support continuing the nominal15fps prototype, not a claim of actual delivered camera frame rate, hardware audio deadlines or USB latency. Physical timing and streaming remain required.

Reproduce:

```sh
JIELI_TOOLCHAIN=... AC79_SDK=... bash native/build.sh --video-bench
python3 tools/video_emu_bench.py
python3 tools/video_bench_report.py build/evidence/video-bench
```

`video_bench_report.py` distinguishes the emulator's numeric unknown-instruction listings from raw byte listings so counter endianness is decoded correctly. Unit coverage checks that conversion. Evidence: `build/evidence/video-bench/{boot.log,symbols.json,measurements.json,actual-native.jpg,actual-native.png,audio.wav}`. The standard `--video` package remains separate from this benchmark.

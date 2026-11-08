# Lossless compact original singer atlases

`tools/sprite_compact.py` derives `firmware/ui/singer_compact.h` from the canonical `singer_sprites.h`. It does not overwrite artwork, alter a pixel, rename a character, invent an outfit, or change the existing sprite packer. It uses Python's standard library only and is deterministic. Both the twelve portraits and eight full-body dance frames are included. Packed pixels remain Hatsune Miku fan art under the Piapro Character License; attribution and noncommercial restrictions in [LICENSING.md](../LICENSING.md) still apply.

## Measured storage

These figures count encoded bytes, row-ID maps and offset tables, excluding palettes/descriptors and decoder code:

| Atlas | Original 4bpp | Shared direct rows | Shared rows + bounded delta | Saved vs original |
| --- | ---: | ---: | ---: | ---: |
| Portraits, 12 × 90 × 90 | 48,600 | 24,341 | 18,441 | 30,159 |
| Dance, 8 × 61 × 71 | 17,328 | 14,857 | 14,443 | 2,885 |
| Both | 65,928 | 39,198 | 32,884 | 33,044 (50.12%) |

The portraits benefit from shared head/shoulder rows and small eye/mouth patches: 554 distinct rows, 291 delta records. Dance frames share less: 552 distinct rows, 169 delta records. Palettes total 64 bytes and portrait dirty rectangles 48 bytes, copied exactly. Each atlas has one descriptor with immutable pointers. On this host `cc -Os` decoder code/constants total 1,495 bytes, with no static RAM or pool. Native linker results must establish the actual flash saving after all original pixel-array references are removed. Do not count an unused original header plus compact header as a flash saving without checking the final image.

No estimate is made for an unapproved future outfit. Each additional design must be packed and measured. Reserving the recovered space for USB video experimentation is an engineering budget choice, not proof that simultaneous UAC/MIDI/UVC is feasible.

## Format and bounded decoder

Each frame has a 16-bit row-ID map. Exact rows are globally deduplicated within the atlas. The row-ID selects a pair of 16-bit byte offsets into the immutable data pool. Each row chooses the shortest of:

- Tag 0: original palette indices packed as two nibbles per byte, row-local with zero pad nibble when width is odd.
- Tag 1: one byte per run: high nibble is count minus one (1–16 pixels), low nibble is the palette index.
- Tag 2: a direct base row ID, x offset, patch width, then a tagged raw/RLE patch. The base is the corresponding neutral-frame row; delta bases must be direct rows. No recursion, chains or runtime frame reconstruction.

Raw wins size ties, supporting constant-time pixel lookup on detailed rows. Delta is used only if smaller than either direct encoding. The generator verifies every unique row against the source after encoding. It rejects source frame byte counts that differ from the canonical dimensions.

`spr_decode_row(&SC_FACE_ATLAS,frame,y,out,capacity)` writes exactly the atlas width in unpacked palette indices. Portrait needs 90 bytes and dance 61 bytes of caller-owned reusable row memory. No full frame, decompressed atlas cache, heap, floating point, libc calls, or hidden workspace. Validation completes before the first output byte is written; errors leave the destination unchanged. Row decoding validates and scans at most one direct base plus one patch, so work is bounded by row width (maximum supported width 255). Zero remains transparent.

`spr_pixel(atlas,frame,x,y,&index)` supports occasional random pixel/dirty-region access with no row buffer. It validates and scans RLE, so a native full-row renderer should decode each row once, not call it for every output pixel. `spr_validate` checks every record, delta base and frame row ID before accepting an atlas. Invalid modes, ranges, padding, truncated records, row lengths, references and delta cycles fail safely. As with other pointer APIs, the descriptor's borrowed arrays must really contain the declared element counts; this is not a parser for arbitrary raw file pointers.

## Native integration contract

Include `sprite_codec.h`, `singer_compact.h` and compile/include `sprite_codec.c` outside the audio ISR. Generated symbols:

- `SC_FACE_ATLAS`, `SC_STAGE_ATLAS`.
- `SC_FACE_PAL[16]`, `SC_STAGE_PAL[16]` (original RGB565 palette values; LCD byte swap belongs to its driver).
- `SC_FACE_DIRTY[48]`, the original twelve `(x,y,w,h)` rectangles flattened, indexed as `frame*4`.

Reuse one 90-byte row buffer when drawing portrait dirty rectangles or dance rows, and overlay palette index zero against the procedural background as before. Scale after decoding. The old header may supply enums/dimensions during integration if none of its original pixel arrays remain reachable; inspect linker output to confirm they were discarded. A dedicated metadata header could remove that source-level dependency later.

The dance atlas has odd width 61. Its original 4bpp stream uses flattened pixel parity `(y*width+x)&1`, not `x&1`. The compact decoder already accounts for row-local padding. Do not reintroduce the original odd-row nibble swap while replacing render paths.

## Reproduction and validation

```
python3 -I tools/sprite_compact.py
python3 -I tools/sprite_compact.py --check
python3 -I tests/test_sprite_codec.py
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_sprite_codec.c firmware/ui/sprite_codec.c -o build/sprite_codec_tests
build/sprite_codec_tests
```

C tests compare all 131,848 source pixels across both atlases, both row and pixel APIs, palettes and every dirty rectangle. They test row boundaries, caller output guards, malformed IDs/offsets/tags/runs/patches, too-small buffers, odd-width padding, and all 2,560 single-byte mutations of a representative delta record. Python tests check deterministic/current generated output and randomized odd/even-width rows. All pass warning-free.

AddressSanitizer and UndefinedBehaviorSanitizer also pass using `ASAN_OPTIONS=detect_leaks=0`; this environment's ptrace prevents LeakSanitizer startup, and the codec allocates no memory. The host row benchmark (500 frames, including per-row validation) measured ~7.9 microseconds per 90×90 portrait frame and ~4.5 microseconds per 61×71 dance frame at `-O2`. Host times are not JieLi/display timings. Actual LCD throughput, emulator CPU, firmware flash/static RAM and USB video coexistence remain integration measurements. No physical hardware was modified or flashed.

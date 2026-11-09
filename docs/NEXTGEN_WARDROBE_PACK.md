# Paired wardrobe sprite pipeline

`tools/wardrobe_pack.py` packs actual outfit-specific source artwork into the native portrait and dance paths. It does not recolor the original outfit to stand in for a new design, fabricate poses from one image, or overwrite existing character assets. Both original atlases remain entry 0. New source artwork and normalized packed pixels remain Hatsune Miku fan art under the Piapro Character License; see [LICENSING.md](../LICENSING.md).

## Source manifest and frame contract

The manifest defaults to `assets/singer/outfits/manifest.json`:

```
{
  "version": 1,
  "budget_bytes": 330000,
  "outfits": [
    {
      "id": "example",
      "name": "EXAMPLE",
      "status": "prototype",
      "sheet": "example/sheet.png",
      "layout": "paired4x5",
      "margin": 0
    }
  ]
}
```

A paired sheet has four equal columns and five equal rows, with magenta or transparent separation between cells. The first three rows contain twelve portraits; the last two contain eight full-body dance frames. Nonsquare cells and image dimensions not divisible by four/five are supported with deterministic integer boundaries. Genuine blank gutters near nominal boundaries are recovered (within 15% horizontally and 28% vertically), accommodating generated portrait rows slightly shorter than body rows. Without a real blank gutter, rounded equal divisions remain the fallback. `margin` trims a stated number of source pixels inside each cell, for explicitly provided borders. It does not detect or silently remove arbitrary text/grid lines.

Portrait frame order: neutral, blink, A, I, U, E, O, lips closed N/M, happy, surprised, tilt left, tilt right. Dance order: idle0–3, singing0–1, arms, bow. Frames must be real source artwork for their intended state; the packer cannot prove that an illustration depicts a correct vowel or gesture. The independent visual review remains necessary.

Separate source sheets are also supported with `layout: "uniform"`, `faces: "slug/faces.png"` (4×3) and `stage: "slug/stage.png"` (4×2). `layout: "legacy"` runs the original sprite packer's pitch/grid recovery with overridden source paths before common-canvas normalization; this is for source sheets matching that older format. Existing `sprite_pack.py` is imported without edits; its transparency, palette, RGB565 and dirty-rectangle helpers are reused.

`pending` entries are excluded explicitly. `prototype` and `approved` entries require both actual source atlases and are emitted in manifest order. Missing ready files, malformed metadata, empty images, absent animation, and budget overflow fail generation; an outfit is never silently dropped to fit flash. The registry distinguishes prototype from approved artwork. The creative director retains final design approval.

## Packing and runtime API

All portrait frames share one union crop and nearest-neighbor transform into a 90×90 canvas. Body frames share a foot baseline and one common transform into 61×71. Source aspect ratio is preserved; padding centers portraits and aligns bodies to the floor. Portrait states 1–9 are aligned against their own neutral frame outside a normalized expression box (default x25, y28, w42, h39), then composed from that same outfit’s stable head/hat/garment plus its actual generated eye/mouth region. This is the original packer’s shared-head contract, avoiding animation jitter and redundant storage. Real whole-frame tilts 10/11 remain untouched. No expression/body pixels are pasted from another outfit. True alpha transparency is honored when present; otherwise the existing magenta/fringe key is used. A single deterministic palette of at most fifteen opaque colors plus transparent index zero covers all frames of each atlas. Quantization error, source bounds, per-frame opaque counts, distinct frame counts and source SHA-256 are recorded for review.

The compact row/delta codec then preserves those quantized palette indices exactly. Dirty rectangles are measured from the actual packed neutral portrait rather than copied from the original outfit. Both atlas dimensions, palette positions and frame counts stay compatible with the existing renderer's frame indices.

Generated `firmware/ui/wardrobe_sprites.h` exposes:

```
typedef struct {
    const char *id, *name;
    const SprAtlas *faces, *stage;
    const uint16_t *face_palette, *stage_palette;
    const uint8_t *face_dirty; /* twelve flattened x,y,w,h rectangles */
    uint8_t approved;
} WardrobeSprites;

WD_OUTFIT_N
WD_ADDITIONAL_BYTES
WD_OUTFITS[index]
```

Entry zero references existing compact original data without a second pixel copy. Each new outfit supplies its own twelve portrait and eight body frames, two palettes and actual dirty rectangles. Decode one row using `spr_decode_row` into the existing reusable 90-byte scratch row; no full-frame decompression, heap or ISR work is added. The renderer must use the selected entry consistently for frame pixels, palette and dirty bounds. A change of outfit requires a complete redraw before using its incremental dirty rectangles.

Rows are deduplicated inside each atlas and choose raw 4bpp, nibble RLE or one-level neutral-row patches. The current implementation does not claim cross-outfit row sharing: palette layouts and drawn geometry differ, and each pool has bounded 16-bit offsets. Additional shared layers require measured data and must not replace actual costume artwork with the original.

## Reproduce and review

```
python3 -I tools/wardrobe_pack.py --report build/evidence/wardrobe-pack.json --preview-dir build/evidence/wardrobe
python3 -I tools/wardrobe_pack.py --check
python3 -I tests/test_wardrobe_pack.py
cc -O2 -Wall -Wextra -Wpedantic -I. tests/test_wardrobe_pack.c firmware/ui/sprite_codec.c -o build/wardrobe_pack_tests
build/wardrobe_pack_tests
```

The preview directory receives palette-decoded portrait and dance contact sheets at native cell sizes. Review those rather than only the high-resolution concept board. Reports record which source sheet produced each pack. Source art, packed previews and actual emulator screenshots represent different validation levels.

The additional flash budget defaults to 330,000 bytes, conservatively below the measured video build's available image space after a 64 KiB reserve. Accounting includes encoded rows/maps, two 32-bit-target atlas descriptors, registry entry, palettes, dirty bounds, names and conservative alignment. On overflow, `--report` still writes measured totals, the command fails and the previous generated header remains untouched. The actual native linker budget guard is authoritative; other firmware code, USB stack, buffers and video work still consume resources. No number of hypothetical outfits is promised from a per-outfit average.

Python fixture tests verify pending/ready rules, missing assets, duplicate IDs/C-name collisions, normalization, real-gutter recovery, shared-head invariants with untouched tilts, palette/decode roundtrip, deterministic output, explicit budget failure and decoded previews. Their synthetic images are test fixtures only and are never embedded as production outfits. The C test walks every registered outfit's twelve portraits and eight dance frames, validates every row/palette index and confirms that dirty rectangles enclose every changed pixel. Source-level visual and native emulator validation must additionally confirm the intended expression, complete garment silhouette, common foot/head anchors and selector behavior.

No physical FM-1 is flashed by this pipeline. Native flash/static RAM/pool/ISR/display measurements are recorded by the integration coordinator after the real artwork is packed.


## First production-art packing milestone

The first three source sheets (`off-duty`, `signal-suit`, `moon-circuit`) each pack twelve distinct portrait states and eight distinct dance poses. Their normalized palette-decoded previews were visually inspected: blink/happy eyes, O mouths, source-specific garments, hats and actual tilt poses remain visible. An early equal-grid extraction admitted neighboring body hair into the last portrait row on two generated sheets; real-gutter recovery removed that contamination before this milestone.

The first three outfits use 101,079 additional accounted bytes in total, versus 161,619 before shared-head normalization. Approximately nine comparable additions would fit the 330,000-byte asset allowance, but only measured actual packs and the native linker determine the final wardrobe count. More than these three new designs must still be generated and integrated to meet the requested minimum of ten total outfits. Prototype metadata does not stand in for final owner design approval.

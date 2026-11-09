# Native integration validation — 2026-11-09

This report separates host modules, built firmware, emulator observations and
physical-device evidence. No physical FM-1 is modified or flashed by this work.
A concept sheet or a host-only module is not evidence that an outfit, lyric editor
or genre is available on the device.

## Starting audit

At commit `591abbf`, the Release host build and all26CTest tests pass. Logs are
`build/evidence/integration-host-{configure,build,tests}.log`. The original three
reference art files match `origin/main` byte for byte. The concept wardrobe catalog
has11entries but zero matched production portrait/dance source pairs. Its initial
inspection explicitly uses `--allow-pending`; that reports ready0,not11skins.

The song model at this starting point stores4508-byte MKS2 version3 files. The
lyric compiler is reusable host/firmware source but the native song has no phrase
storage or articulation playback, and the native generator still uses its earlier
style controls. Integration changes are being validated after they land; the
starting audit must not be cited as validation of those later features.

## Independent wardrobe source QA

Run after production sources and the packing manifest are ready:

```
python3 tools/wardrobe_validate.py assets/singer/outfits/manifest.json \
  --expected-ready 11 --out build/evidence/wardrobe-validation
```

The tool reads manifest-relative `faces` and `stage` paths with uniform4x3
portrait and4x2dance grids, or a `sheet` path with `layout:"paired4x5"` whose first
three rows are portraits and last two rows are dance poses. Grid edges independently recover full blank magenta/alpha gutters near the
nominal divisions, recording the actual coordinates and every cell box. Without
a gutter the nominal rounded division is retained. Unequal cells are reported
for visual inspection; several generated source sheets have portrait rows
shorter than their body rows, so equal divisions alone import neighboring hair. It preserves hashes, detects blank cells and duplicate complete
source pairs, checks that original reference assets remain untouched, and creates
one20-state contact sheet per outfit. It rejects missing production pairs by
default. The example count11is the full production candidate set; choose the accepted
count explicitly after the measured image budget, with at least10total outfits
including the original if needed. Pending entries are excluded only with the explicitly requested
`--allow-pending` inspection option. `--expected-ready` makes a shortfall fail.

The metrics do not certify artistic identity, matched clothing, intelligible mouth
shapes or owner approval. Those require visual inspection of the source contact
sheets, packed decoder output, native STAGE and native DANCE. No palette swap is
counted as proof of a newly designed outfit. A native registry count alone cannot
prove that the selected portrait and body use the same source outfit.

## Required integration observations

The final evidence must include:

- A stable-source native build, source commit/hash manifest and flash/static-RAM/
  pool checks, including the optional video build's reserved headroom.
- Native STAGE and DANCE screenshots for selected outfits with matching clothing,
  preserving existing mouth-state mappings and original art selection.
- GEN page screenshots showing the modern style selection and rendered generated
  songs, with deterministic output, track locks and codec round-trips tested.
- LYRICS page editing and persistence, real engine audio renders, articulation/
  rests/sustain tests, actual mouth events, and old-song neutral playback checks.
- Emulator smoke and audio clipping/underrun observations, with emulator ISR
  percentages kept distinct from physical timing.

Native build/emulator runs wait for the coordinator's stable-source notification
so concurrent platform resets cannot mix partially edited source into evidence.

## Current integration results

Pending stable production art and native integration. This section will be
replaced with concrete commands, results, screenshots and limitations after the
new code is ready; it is not a completion claim.

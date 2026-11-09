# Neon console UI, released flash headroom and DX7 audition bank

> Current DX7 milestone: [real six-operator renderer, dedicated BANK/operator editor, embedded patch saving, native video/audio evidence and current measurements](DX7_BANK_HANDOFF.md). This supersedes earlier DX7-plan-only status.


Current implementation: `026f682` on feature/miku-nextgen-performance.
Related commits: `1dee4b0` removes the fixed flash floor; `0df5a45` prepares the
DX7 shortlist/extractor. PR21 remains draft. This supersedes all earlier
instructions requiring 65536 bytes of unused application image space.

## What the owner authorized

The owner explicitly removed the arbitrary 64 KiB development flash reserve,
asked us to plan a compact DX7 bank fitting the candidate electronic genres, and
authorized a more stylish device-wide UI. No physical flashing, main merge,
release or live-site deployment was authorized.

## Resource policy now

config/resource_budget.json sets image.minimum_free to **0**. The application
must still fit the actual **581564-byte** image limit. There is no allocated
64 KiB buffer to free: this change removes a build-policy floor, not memory used
by the existing JPEG encoder. Existing framebuffer-free rendering, 8192-byte JPEG
buffer, audio buffers and runtime allocations remain accounted for normally.

The static RAM and shared-pool margins (16384/32768 bytes) are unchanged. They
are separate from the discussed flash allowance and are not the removed video
reserve. Future feature budgets are estimates to measure, not another arbitrary
fixed flash partition. The regression test proves that an image exactly at its
limit passes with zero reserve, an oversized image fails, and RAM overflow still
fails. Missing/invalid linker symbols continue to fail closed.

## Implemented visual direction

Proposed and implemented a neon concert-console treatment using the owner's
pink/blue palette:

- Brighter blue-to-violet/pink header bands, with a double accent rule.
- Small graphical page badges and bright title lettering with shadow.
- Deeper blue/purple panel gradients, inset highlight lines and corner details.
- Sparse background texture, maintaining a readable dark surface beneath text.
- Stronger mixer rails, preserving track colors and mute/solo meanings.
- Wardrobe stage preview grows from 140x96 to 140x126, with a larger correctly
  proportioned character, paired portrait, theme hint and persistent EDIT/SAVE
  hints. Four parameter cards remain directly mapped to K1-K4.

The shared treatment covers all nine musical pages and the held-SAVE window;
STAGE/DANCE retain their character artwork with matching control strips. Boot,
recovery and update diagnostics retain the platform rendering. Existing encoder,
confirmation, stop-first and song/look persistence semantics are unchanged.
REC still triggers LOOK's effect; its footer hint was displaced by the enlarged
preview. The larger preview refreshes on edits rather than continuously animating.
This is a reviewable design iteration, not final creative approval of the art,
genre lineup or UI appearance.

The visuals reuse the existing clipped canvas and artwork. This was a design
change rather than spending the newly available flash on redundant bitmaps.
No UI drawing, USB work or flash writes were moved into the audio ISR.

Review [before/after wardrobe](evidence/neon-console/wardrobe-before-after.png),
[all native windows](evidence/neon-console/all-pages.png), and
[actual firmware walkthrough](evidence/neon-console/walkthrough.mp4).

## Concrete DX7 shortlist (historical, superseded)

**Superseded:** this eight-voice Dexed shortlist and its extraction tool and plan
(`config/dx7/review_bank.json`, `tools/prepare_dx7_review_bank.py`) were removed from the repository
because the voices had no clear redistribution licence. The factory bank is now ten original voices;
see `assets/dx7/README.md`. What follows is kept as history.

config/dx7/review_bank.json identified eight existing patches in the banks bundled
with [Dexed](https://github.com/asb2m10/dexed/tree/fea2f0f0f2c397799f59b27eb11c038cd34b22c5/assets).
The source revision/archive and individual source banks are pinned in the plan.
These are provisional audition candidates, not claims of superior sound or an
approved musical lineup. Source identity was checked; listening still requires
the reused six-operator renderer.

| Existing patch | Intended role | Candidate uses |
|---|---|---|
| glassbell | Glassy hook | Hyperpop, Digicore |
| PLUCVIBE 1 | Digital chord pluck | Future Bass, Electropop |
| HYPE BASS2 | Aggressive FM bass | Hyperpop, Digicore |
| TIGHT BASS | Short rhythmic bass | Jersey Club |
| Slow3D Pad | Evolving chord bed | Future Bass |
| SYN-LEAD32 | Melodic lead | Hyperpop, Electropop |
| FmRhodes18 | Expressive keys | Electropop |
| SPARKLE 1 | Bright digital accent | Digicore, Future Bass |

The offline preparation tool validates standard 32-voice Yamaha bank headers,
length, 7-bit payload and checksum, locates exact patch names uniquely, and emits
an eight-voice raw packed payload plus source indices/algorithms/SHA-256 metadata.
The actual preparation succeeded: **8 x 128 = 1024 bytes**. Reproducibility metadata
is committed in [verified shortlist](evidence/neon-console/dx7-shortlist-verified.json).
The source ZIP and extracted patch bytes remain ignored local review inputs; no
redistribution rights for arbitrary patches are claimed by the engine license.

```sh
python3 tools/prepare_dx7_review_bank.py /path/to/builtin_pgm.zip \
  --out build/dx7-review
```

This is an offline preparation path, not a device bank loader or a rendered audio
comparison. Do not advertise DX7 playback/import as implemented. The current
firmware still uses its existing two-operator band and separate formant singer.
No placeholder preset selector has been added that would falsely imply playback.

Next concrete DSP task: adapt and benchmark a small existing MSFA/Dexed-compatible
renderer, using generated tables as needed for the integer-only device rules.
Start with this bank's chord/lead and bass roles. Keep old preset IDs/sounds as a
baseline, the four tracks and singer unchanged. Measure full-band ISR time,
voice state RAM, linked code/tables, clipping and release behavior, then render
contrasting examples for owner selection. The 1024-byte payload is a known cost;
renderer, bank integration and optional user import costs remain unmeasured.
Do not replace the removed flash floor with an invented engine-size reservation.

## Validation and actual linked costs

Release host build: **41/41 CTests pass** (12.09 seconds). The added DX7 tests
check source identity/order, patch indices, algorithm metadata, missing patches,
truncated/nonstandard messages, non-7-bit data and bad checksums. Existing
resource tests now also cover zero-reserve exact-fit versus overflow.
Original fvoice/fmband tables and original singer sprite regeneration pass.

Actual default and MIDI+UVC native builds pass with the new zero flash floor:

| Profile | Image used/free | Static RAM used/free | Pool used/free |
|---|---:|---:|---:|
| Default |507864 / 73700|35760 / 62544|255624 / 88440|
| MIDI+UVC |514516 / 67048|33984 / 64320|266080 / 77984|

The visual change adds 468 default / 472 video image bytes, zero static RAM and
zero pool bytes relative to the previous theme. The prepared DX7 payload is not
linked into these figures; adding it would consume 1024 bytes plus actual
renderer/integration code. All 67048 currently free video image bytes are
available for measured development rather than only the amount above 64 KiB.

The actual default firmware boots in fm1-emu, plays its native song and visits
all nine pages, the voice knob popup and held-SAVE window. Committed PNGs are
native LCD captures; MP4 adds nearest-neighbor enlargement and captions outside
the screen. Audio clipping metrics are in the accompanying JSON. This is not a
new CPU deadline, sustained UVC or physical LCD measurement. No physical device
was changed, no website deployed and no main merge/release performed.

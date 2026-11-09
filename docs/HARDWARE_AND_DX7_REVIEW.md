# FM-1 hardware variants and DX7 reuse review

Review implementation guard: `bd400b9`. Branch: feature/miku-nextgen-performance.
This is compatibility verification and a sound-engine feasibility review, not a
DX7 integration claim. PR21 stays draft. No physical FM-1 was modified/flashed.

## Documented hardware change

The newer FM-1 batch replaces the Puya P25Q80 (JEDEC 0x856014) with UC25HQ80
(0xB36014). Both are 1 MiB SPI NOR parts. The upstream platform author documents
this exact change in [Dinghy's compatibility fix](https://github.com/zvenson/dinghy/commit/f77bf03283bbb2c05294b01cdad68a30b410f93d)
and [Dinghy's README](https://github.com/zvenson/dinghy#readme).
Earlier builds rejected the newer chip and could appear to save while retaining
changes in RAM only. This is a persistence issue, not a different DX7 patch format.

MIKU's native/build.sh already pins that exact compatibility-fix commit. Auditing
the actual fetched HAL confirms FL_JEDEC_OK accepts both IDs. The application's
platform.c, recovery.c and update loader's loader.c use the shared flash-map
predicate. No platform migration or second musical firmware image is needed for
this documented chip difference. This does not establish that every possible
unreported hardware revision is identical.

Three compile-time assertions now require both chip IDs to stay accepted and
0xFFFFFF to remain rejected. These compile against the actual platform header
included by platform.c, not a copied compatibility table. Actual default and
MIDI+UVC builds pass. The assertions add no linked code/data bytes.

## Emulator validation and its limits

The ignored fm1-emu recorder was extended with FM1_FLASH_JEDEC to set the existing
SPI flash model's public JEDEC response. The updated reproducible helper source
is [fm1-record.rs](evidence/device-scenes/fm1-record.rs). No emulator source is
part of the device binary.

For each known ID, the actual current default firmware:

1. Boots, enters DANCE then LOOK, selects Neon Punk and saves.
2. Writes slot 1 at 0x97000 (4508 bytes) and settings at 0xFC000 (44 bytes).
3. Produces valid outer CRC32 values for both records.
4. Restores the captured flash in a separate fresh emulator process.
5. Restores Neon Punk/LED wall/rails/confetti/style/intensity 80.

The full flash dumps from the two known-ID runs are byte-identical (SHA-256
`ae526f555b7a3b34cdadf17a85ff5101af004ae37f91454210eaaa70c7f7257d`).
Their fresh-boot LCD pixels are also identical. With unknown ID 0xFFFFFF, both
song/settings sectors remain erased; unsupported flash is not silently enabled.
See [screenshots and record measurements](evidence/hardware-variants/measurements.json)
and [side-by-side restored screens](evidence/hardware-variants/both-restored.png).

Example recorder environment (after compiling the helper in fm1-emu):

```sh
FM1_FLASH_JEDEC=B36014 FM1_FLASH_OUT=/tmp/fm1-test.bin \
  fm1-record build/native/miku-fm.fwsc 3000 /tmp/first.wav \
  '400:HOME,450:HOME^,700:EDIT,750:EDIT^,1000:e0:4,1600:SAVE,1650:SAVE^' FRAME_DIR
FM1_FLASH_JEDEC=B36014 FM1_FLASH_IN=/tmp/fm1-test.bin \
  fm1-record build/native/miku-fm.fwsc 2600 /tmp/reboot.wav \
  '400:HOME,450:HOME^,700:EDIT,750:EDIT^' REBOOT_FRAME_DIR
```

Frame directories must exist. Repeat for 856014; FFFFFF is the rejection check.
These tests exercise ID detection and real firmware storage/reboot paths against
the emulator's generic SPI model. They do not measure physical chip timing,
voltage/erase behavior, OTA interruption recovery or an actual update on either
unit. Physical confirmation on both variants remains required before claiming
hardware qualification. The OTA loader was source-audited and built; no actual
firmware-update transaction was exercised by these save/reboot tests.

All 40 host CTests pass (10.95 seconds). Resource usage remains exactly:

| Profile | Image | Static RAM | Pool | Image free |
|---|---:|---:|---:|---:|
| Default |507396|35760|255624|74168|
| MIDI+UVC |514044|33984|266080|67520|

The video profile has 1984 bytes above the unchanged 65536-byte image reserve.

## What stock DX7 support does and does not provide

The [official FM1 Open README](https://github.com/m-vave/fm1_open/blob/main/README.md)
documents DX7 single-voice and 32-voice bank import/export and a 128-voice library.
It also states that its device engine is a binary SDK with separate terms. That
firmware's support is not automatically present when MIKU replaces its app/engine.
A preset describes an engine's settings; it is not executable synthesis code.

MIKU's actual firmware/audio/fmband/fmband.h and fmband_ch.c implement four-voice
**two-operator** chord/keys FM. Bass has its own compact engine, and drums have a
dedicated synthesized engine. There is no six-operator DX7 renderer or DX7 bank
import path in MIKU today. The existing mora/formant singer is a separate engine.
Copying DX7 SysEx into MIKU's preset tables cannot faithfully recreate those sounds.
Reducing six operators to two would be approximation, not DX7 compatibility.

## Recommended reuse path

Use an existing DX7-compatible engine rather than write a six-operator model from
scratch. [Dexed](https://github.com/asb2m10/dexed#readme) documents its MSFA engine
and licensing (Dexed GPLv3, MSFA Apache 2.0). It is a candidate for a narrowly
scoped embedded backend; its desktop JUCE/plugin/UI wrapper is not needed.
No third-party engine source was added by this review. Exact revision, runtime
allocation, integer rendering/table initialization and notices still need audit.
Any float-dependent initialization must be replaced with generated constants or
otherwise adapted to this project's existing integer-only device rule.

First build an isolated host/target experiment with a small bank of voices and
compare it with the existing band. Keep MIKU's singer, native pages, song library,
sequencer and four-track architecture. Preserve old instrument choices/IDs as a
baseline. Chord/lead and bass are the plausible DX7 destinations; keep the
specialized drum engine unless an A/B render shows a useful reason to change it.
DX7 electric pianos, bells, FM basses and digital pads are plausible improvements;
it is not evidence that every modern supersaw, drum kit or genre will improve.
The owner should choose the final preset shortlist from listening examples.

This is a bounded but substantial backend change, not a drop-in patch-bank load.
Four six-operator voices require 24 operator evaluations per sample versus the
current eight base operators, before envelopes and other differences. That is an
operator-count comparison, not a measured CPU multiplier. Benchmark actual
44.1 kHz blocks alongside the singer, effects, bass, drums and native visuals
against the 256-sample / 5.805 ms callback deadline.

A standard packed DX7 voice uses 128 bytes, a 32-voice bank payload 4096 bytes;
that bank alone exceeds the 1984 bytes above today's video reserve, before any
renderer/import/UI code. Even a tiny compiled bank needs measured image savings.
The budget issue is flash headroom, not proof that the CPU or hardware cannot
perform DX7 synthesis. Do not remove outfits or waive the USB reserve silently.
Use bank data with suitable redistribution rights, or user-supplied files.

A compiled curated bank is a smaller first integration than arbitrary bank import.
Later import needs checksum/range validation, bank storage, device preset selection,
MIDI/SysEx coexistence with updater traffic, and a decision about how exported
songs retain user patch identity/content. Do not assume existing MKS files already
carry arbitrary DX7 voices. Preserve old saves and test deterministic rendering,
clipping, sustained chords, note stealing, releases and the full band mix.

Next task: profile a small reused DX7 backend and two contrasting preset examples
against the current instruments before deciding whether to integrate it or spend
resources improving the compact backend. Do not migrate away from Dinghy or
replace the voice architecture as a side effect of importing presets.

> Historical handoff captured before the dedicated PR21 review guide. Paths in this archived text are relative to the repository root. Current entry point: ../../CLAUDE_CODE_REVIEW.md.

# MIKU-FM1 — current engineering handoff for Claude review

> Current DX7 milestone: [real six-operator renderer, dedicated BANK/operator editor, embedded patch saving, native video/audio evidence and current measurements](docs/DX7_BANK_HANDOFF.md). This supersedes earlier DX7-plan-only status.


> Current policy and UI: [fixed 64 KiB flash reserve removed at owner request; neon console UI implemented; eight-voice DX7 audition bank prepared offline](docs/NEON_UI_AND_DX7_BANK.md). Earlier fixed-flash-reserve instructions are superseded.


> Hardware/DX7 review: [both known FM-1 flash variants pass emulator persistence checks; DX7 engine reuse is evaluated but not integrated](docs/HARDWARE_AND_DX7_REVIEW.md).


> Current device-wide visual pass: [pink/blue borders and gradients across all nine musical pages, popups and SAVE window, with native recordings and current measurements](docs/evidence/device-theme/README.md). This supersedes the earlier menu-only UI scope and resource figures.


> Latest UI pass: [visual wardrobe and song-library menus, native walkthrough and current build measurements](docs/evidence/menu-redesign/README.md).


> Current device UI follow-up: [selectable per-song LOOK controls, electronic scenes, recordings and resource measurements](docs/DEVICE_LOOK_HANDOFF.md). This supersedes earlier global-look settings and pre-follow-up audio/resource figures below.


Repository: <https://github.com/MichaelAPerry/MIKU-FM1>
Branch: **feature/miku-nextgen-performance**
PR: **[21, draft](https://github.com/MichaelAPerry/MIKU-FM1/pull/21)**
Main base: `88b0ef3e568cf615eb5dfaa42d170d8c0f75d431`.

Start with this file and [the detailed native integration notes](docs/NEXTGEN_NATIVE_INTEGRATION.md).
The earlier handoff is preserved in
[the pre-integration archive](docs/archive/CLAUDE_HANDOFF_PRE_NATIVE_INTEGRATION.md).
Its original-only wardrobe, host-only lyric/GEN and 26-test figures are historical.

## Authorization and current scope

The owner explicitly authorized pushing to the feature branch and requested a
verbose handoff for Claude review before main. They subsequently asked to finish
native lyric/GEN integration, real paired costume animations, fireworks and
other stage sheets, and confirmed a **ten-song library: four preloaded editable
songs plus six free slots**. Complete first verses were selected, not excerpts.
The library contains ordinary playable/editable projects, not embedded recordings.

Do not merge, push main, publish a release, deploy Pages, or flash hardware
without separate owner authorization. A main push has a Pages deployment side
effect. Keep PR21 a draft. No physical device has been modified or flashed.
The branch is a substantial tested milestone; it does not complete every item
of the original multi-phase assignment. Read the limitations before approving it.

The owner remains creative director for the genre shortlist, character designs,
hairstyles, final animation concepts and major interface behavior. Current
outfit options and contemporary genres are review candidates. Do not infer
approval of the final lineup merely from their presence in the development UI.

## What now works in native firmware

1. Silent STAGE/DANCE SELECT, with held-note/playing waveform regression tests.
2. Slower ANIMAL cadence and explicit SCL+PRESETS speech-rate gesture, without
   slowing Japanese/SCAT/VOWELS; existing release tests remain.
3. Native GEN calls the contemporary generator through an 840-byte pool
   workspace rather than its old roughly 5 KB stack wrapper. Five candidates
   coexist with the four retained legacy styles. Locking, deterministic seeds,
   phrase regeneration and take/drop semantics remain.
4. Native LYRICS edits normal vocal patterns, with duration, pitch, mora,
   rests, holds, word/phrase boundaries, articulation, clipboard and undo.
   Sequencer/engine flags survive delayed consonant queues and harmony voices.
5. ORIGINAL plus eleven actual paired outfits: twelve portrait states and eight
   dance poses for each. Matching portraits, body sprites, palettes and dirty
   rectangles switch together. The native USB JPEG callback uses the selection.
6. Native VISUALS exposes outfit, background, frame, effect, intensity and
   explicit effect triggering. Fireworks, sparkles, confetti and smoke share a
   compact sixteen-frame sheet; beams are procedural. Animation is bounded,
   integer-only, absolute-time and outside the audio ISR.
7. SONG LIBRARY browses ten editable save slots. Sakura Sakura, Tōryanse,
   Furusato and Kōjō no Tsuki are preloaded first-verse arrangements with complete
   melody forms and endings. The other six slots are available to the owner.
8. Ten-slot storage preserves the old eight addresses. Factory fallback never
   writes flash or replaces saved user songs. Native save/load/restore guards
   and visual preference migration are implemented and tested.
9. The local companion shows all twelve actual packed costumes with matching
   vocal portraits and effects using recorded real-engine state/WAV playback.
   Native UVC remains a separate development profile; the companion does not
   pretend to have live USB telemetry.

The original four tracks, 16 patterns per track, 64 arrangement rows, original
source art, baseline voice and self-contained native workflow remain. Website
release assets and the live browser emulator were not redeployed or replaced.

## Linked resources and headroom

Limits: 581564-byte image, 98304-byte static RAM, 344064-byte shared pool.
These figures come from actual target builds, not host object sizes.

| Profile | Image used / free | Static used / free | Pool used / free |
|---|---:|---:|---:|
| Default native |531820 / 49744|42512 / 55792|255624 / 88440|
| Native MIDI+UVC |538444 / 43120|40720 / 57584|266080 / 77984|
| Offline video benchmark |538328 / 43236|40720 / 57584|266080 / 77984|

The owner removed the fixed image-space floor. The actual image limit remains
581564 bytes; static/pool free-space margins remain 16384/32768 bytes. The current
video profile has **43120 genuinely free image bytes**, with no 64 KiB reservation.
The offline benchmark row now includes DX7 playback. Measure upcoming DX7 and USB work;
do not reintroduce an arbitrary flash reservation or silently drop outfits.

All eleven additional wardrobe packs total **335463 accounted bytes**, including
metadata, against a 340000-byte packing budget. A final review caught generated
black grid lines and nonuniform sheet extents; source crops were corrected and
regression-tested. Optional neighboring-row deltas then recovered **4304 bytes
losslessly**, with one-level decoding unchanged. All 26 companion image/metadata
exports were checked against the decoded firmware rows; no art pixels changed
in that compression pass. Original compact headers still regenerate exactly.

Video uses one 8192-byte JPEG buffer plus a roughly 2240-byte encoder context,
not a framebuffer or second-core render job. UAC/CDC are deliberately disabled
in this experimental MIDI+UVC profile. This is not a simultaneous audio/MIDI/UVC
configuration claim.

## Timing and audio evidence

The prior milestone offline native benchmark captures three frames during real vocal
and band playback, with Moon Circuit, grid/corners and the expanded fireworks
phase active. It uses the target firmware instructions in fm1-emu:

- Three frames, zero encoder failures, final JPEG 4400 bytes.
- Maximum one-tile main-loop encode step: **1419.125 µs**.
- Maximum frame wall time: **39592.708 µs**.
- Emulated ISR share: **7.2%**; audio-render moving peak: **759.125 µs** against
  a 256/44100 callback period of **5804.989 µs**.
- 137984 captured audio frames, peak 8231, zero full-scale samples.

The audio timing counter is a decaying moving peak, not a proven global worst
case. These are emulator measurements, not physical ISR deadlines, delivered
USB frame rate or enumeration/throughput results. A separate host JPEG check
now covers 720 scene combinations; maximum JPEG is **5967/8192 bytes**.

The four factory-song WAVs render through the actual device engine. Peaks are
30338–31512 with zero full-scale samples. Their full first-verse listening
copies and editable `.mks` files are committed under
[review songs](docs/evidence/native-integration/songs/).
Uncompressed WAV hashes/RMS/durations are in
[song measurements](docs/evidence/native-integration/song-measurements.json).

## Validation completed

- Release host build and **46/46 CTest checks**.
- Original fvoice/fmband table generation and original sprite regeneration.
- Original compact-header regeneration; full wardrobe packing/checks; independent
  eleven-pair source audit with original source hashes unchanged.
- All face/body compact decoders, portrait dirty rectangles and native original
  LCD pixel comparisons, including the final odd-height background row.
- Factory deterministic construction, valid fields, MKS2 encode/decode round trips.
- Actual native functions against flash mocks for all ten save/load slots,
  factory fallback, non-destructive restoration, confirmations keyed to target,
  and visual preference packing. Slot addresses checked separately.
- Actual emulated native save into slot 10 at **0xBB000**, record length 4508,
  correct outer CRC; fresh emulator boot restored that slot and played it.
- Native screen/audio exercises for GEN, LYRICS, library, portrait and fireworks.
- Local Chromium companion regression across all twelve paired costumes,
  real mouth-state playback, effects and malformed-file rejection.
- Actual default, MIDI+UVC and offline-benchmark target builds with resource guards.
- Default/video emulator smoke runs and the offline target JPEG/audio benchmark.

The slot-10 emulator round trip used a local diagnostic FM1_FLASH_IN/OUT patch in
an ignored emulator checkout. It is not a physical flash test or part of the
firmware image. Durable screenshots and measurements are committed in
[review evidence](docs/evidence/native-integration/README.md). Build logs and
uncompressed WAVs remain ignored and can be regenerated.

## Important file map

| Area | Files |
|---|---|
| Native controls and integration |`native/dinghy.c`, `native/lyrics_ui.inc`, `native/visuals_ui.inc`, `native/library_ui.inc`|
| Native builds / ten-slot map |`native/build.sh`, `tools/patch_dinghy_slots.py`|
| Lyrics and sequencer |`firmware/song/lyrics.*`, `lyric_edit.*`, `seq.*`, `firmware/app/engine.*`|
| Contemporary GEN |`firmware/song/gen_next.*` and its host/context tests|
| Factory songs |`assets/songs/factory/`, `firmware/song/factory.*`, `factory_tables.h`, `tools/gen_factory_songs.py`, `tools/factory_export.c`|
| Wardrobe |`assets/singer/outfits/manifest.json`, eleven source sheets, `tools/wardrobe_pack.py`, `wardrobe_validate.py`, `sprite_compact.py`, generated `wardrobe_sprites.h`/`wardrobe_meta.h`|
| Stage / JPEG |`firmware/ui/performance.*`, `stagefx.*`, `stagefx_sprites.h`, `firmware/usb/jpeg/performance_frame.c`|
| Companion |`companion/`, `tools/export_companion_art.py`, `firmware/app/performance_wire.*`|
| Resource enforcement |`config/resource_budget.json`, `tools/check_resources.py`|
| USB driver prototype |`native/video_usb.inc`, `tools/patch_dinghy_video.py`, `firmware/usb/video/`, `firmware/usb/jpeg/`|

## Reproduce the host and native checks

Host tooling dependencies are explicitly installed in CI; the device firmware
has no Python/runtime dependency. On a clean host, use a Python virtualenv and
install `tools/requirements-dev.txt` before these commands:

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
python3 tools/gen_fvoice_tables.py
python3 tools/gen_fmband_tables.py
python3 -I tools/sprite_pack.py --check
python3 tools/sprite_compact.py --check
python3 tools/wardrobe_pack.py --check
python3 tools/wardrobe_validate.py assets/singer/outfits/manifest.json --expected-ready 11
python3 tools/gen_factory_songs.py --check
python3 tools/export_companion_art.py --check
```

Native uses Dinghy `f77bf03283bbb2c05294b01cdad68a30b410f93d`, the documented
Jieli toolchain and AC79 SDK `d179b4484759423312073f5fbb232501aa491047`.
Set `JIELI_TOOLCHAIN` and `AC79_SDK` to their local paths. Do not build multiple
profiles concurrently because the script uses one pinned Dinghy checkout.

```sh
bash native/build.sh
bash native/build.sh --video
bash native/build.sh --video-bench
bash tools/emu_smoke.sh build/native/miku-fm.fwsc build/evidence/default-smoke PATH_TO_FM1_EMU
bash tools/emu_smoke.sh build/native/miku-fm-video.fwsc build/evidence/video-smoke PATH_TO_FM1_EMU
```

Current expanded-burst benchmark:

```sh
python3 tools/video_emu_bench.py --out build/evidence/wardrobe-video-bench --ms 3300 --keys '400-450:HOME,550-600:HOME,700-750:HOME,850-900:HOME,1000-1050:HOME,1150-1200:HOME,1300-1350:HOME,1450:SELECT:+11,1500:K1:+1,1550:K2:+1,1600:K3:+1,1850-1900:PLAY,2000-2800:n7'
python3 tools/video_bench_report.py build/evidence/wardrobe-video-bench
```

For library screenshots use eight explicit nonoverlapping HOME taps beginning
at 400 ms. The emulator's implicit tap lasts 150 ms, so adjacent taps at
150-ms boundaries can merge; explicit ranges avoid misleading page evidence.
SELECT `+9` in the library followed by REC saves to slot 10. With the local
flash diagnostic patch, export flash and restore it on a fresh run to verify
boot selection; never use this script as a physical-device flash command.

Factory audio:

```sh
mkdir -p build/evidence/factory-songs
build/host/firmware/song/factory_export build/evidence/factory-songs
build/host/firmware/app/song_render build/evidence/factory-songs/SAKURA.wav build/evidence/factory-songs/SAKURA.mks
```

Repeat for TORYANSE, FURUSATO and KOJO. `python3 -m http.server 8765` serves the
local companion; `python3 companion/test_review.py` requires local Chromium and
Playwright. This serves local files and does not publish or deploy the website.

## Required review and unfinished work

1. **Creative review:** approve/revise outfits, hairstyles, effects and genre
   lineup; listen to the four song arrangements and limited `rya` pronunciation.
2. **Native UI:** review the three added pages, confirmations, single-row lyric
   editing and half-tempo/two-step score representation. A full multi-row native
   text interface is not implemented. Furusato's 3/4 timing remains continuous.
3. **Physical validation:** real display/DMA contention, audio artifacts/underruns,
   actual save/reboot, updater/rescue, USB camera enumeration and delivered
   video bandwidth/latency. The software evidence does not replace this.
4. **USB pedal:** host DSP exists, but actual computer-audio input/routing and
   effects access before final mixing are not integrated into native UAC.
5. **Live computer telemetry:** SysEx encoding/recorded review works; a live
   device/host performance transport remains unfinished.
6. **Full-body lips:** paired portrait mouth states work; per-phoneme mouth
   compositing onto every dance pose remains unfinished.
7. **Voice and band:** baseline engine preserved; pitch compensation and modern
   chord profiles remain opt-in/API work requiring listening/resource decisions.
   Existing high-register raw clipping risks remain documented in the archived
   voice evidence. Do not describe the entire DSP upgrade as completed.

The next concrete engineering task is the physical native-video validation
matrix, followed by a measured UAC input-buffer/routing prototype. Keep Dinghy,
all four tracks, old save addresses, the real-firmware emulator and resource
floors. Any change to outfit count or reserved headroom must be explicit and
reviewable. No main integration is authorized by this handoff.

# Claude Code handoff — independent review of PR21

## Assignment and exact scope

Review https://github.com/MichaelAPerry/MIKU-FM1/pull/21 independently. The branch is
`feature/miku-nextgen-performance`, targeting `main`; it is deliberately a draft.
The implementation tip before this documentation handoff is
`c394043e04dd861a3413e8c9d9fcce48e6ac4325`. The latest actual DX7 implementation is
`2bb1662`; `c394043` adds its evidence and engineering handoff. Record the exact
HEAD you review, because later fixes may advance the branch.

The PR base at handoff is `88b0ef3e568cf615eb5dfaa42d170d8c0f75d431`. Review the
whole PR, not only the last DX7 commit. The branch is large (345 changed files
before this handoff); generated sprite/table files and evidence account for much
of it. Prioritize authored source and regenerate/validate generated content.
Do not assume a passing test or this document proves a claim correct.

Deliver findings, severity, exact file/line, a concrete trigger, impact and a
suggested fix/test. Distinguish actual defects from unimplemented original scope,
creative choices and physical tests not performed. Do not merely summarize the
changes or accept the implementation's own claims as the review conclusion.

## Authorization boundaries

The owner authorized development and pushes to the feature branch, and requested
Claude review before integration. No authorization exists to merge/push main,
publish a release, deploy the website, or modify/flash a physical FM-1. Main pushes
can trigger website deployment. Keep PR21 draft. Review/report first; if the owner
requests fixes, keep them on the feature branch and document validation.

The owner remains creative director for the final genre lineup, patch shortlist,
outfits, choreography and major interface behavior. Current candidate styles and
art options are review material. Do not turn implementation presence into creative
approval. This PR does not complete the entire original multi-phase assignment.

## Read first

Read applicable AGENTS.md instructions, then:

1. README.md; docs/HANDOFF.md; docs/PROJECT_STATE.md; docs/VALIDATION.md.
2. docs/SONG_FORMAT.md; docs/CONTROLS.md; docs/MANUAL.md; LICENSING.md.
3. docs/DX7_BANK_HANDOFF.md — current DX7 controls, evidence, measurements, limits.
4. docs/DEVICE_LOOK_HANDOFF.md and docs/NEXTGEN_NATIVE_INTEGRATION.md — song library,
   lyrics/GEN, paired assets, per-song scenes and earlier integration details.
5. config/resource_budget.json and docs/evidence/NEXTGEN_NATIVE_INTEGRATION_SNAPSHOT.json.

Earlier documents contain historical numbers/status sections. Current DX7 evidence
and this guide supersede “DX7 renderer not integrated,” “original outfit only,”
“lyrics/GEN host-only,” and the old64 KiB flash-reserve requirement. Keep historical
records as history rather than treating their conflicting claims as current facts.
`native/README.md` and the root handoff index now point to the current implementation.

## Current product behavior

- STAGE/DANCE syllable SELECT is silent; intentional PATTERN audition is separate.
- ANIMAL default cadence is140–220 ms, with session rate control; other languages
  retain independent timing and existing release behavior.
- The existing formant singer remains the foundation, with opt-in compensation
  work and lyric/articulation support. Do not infer studio-quality vocals from
  parameter additions or a small test corpus.
- Four tracks remain: vocal, chords, bass, drums. Native lyrics and candidate GEN
  use editable patterns, deterministic seeds, locks and phrase regeneration.
- Ten song slots; four editable first-verse arrangements preloaded: Sakura Sakura,
  Toryanse, Furusato, Kojo no Tsuki. Existing saved records take priority.
- Original plus eleven paired outfits, matching portraits/dance sprites; per-song
  device looks, backgrounds, frames and effects are selectable natively.
- Pink/blue UI covers musical pages and SAVE. There are now ten HOME pages,
  with BANK appended; existing nine indices are retained.
- The new bank actually uses a six-operator MSFA-derived renderer. It is not an
  editor pretending the older two-operator band is DX7. Eight existing packed
  DX7 patches are compiled in; chords use4 voices and bass1.
- MIXER REC opens BANK. SELECT browses silently, REC loads, keys audition.
  EDIT/PRESETS choose pages; SELECT/ALGORITHM choose OP1–OP6 in edit views;
  K1–K4 edit real packed operator/global fields. Edited patches save with songs.
- Classic sounds remain available for old projects. DX7 song snapshots are
  embedded, so later factory-bank changes cannot retimbre saved projects.

## Review priorities — concrete risk areas

### 1. Audio backend and real-time correctness

Inspect firmware/audio/dx7/dx7.c,h, dx7_constants.h, generated tables and provenance.
Verify32 routes, feedback bus semantics (especially the added sample-major paths
for algorithms4/6), amplitude scaling, key/velocity/detune/fixed-frequency math,
envelope termination, voice stealing and cached-block boundaries. Test pathological
valid patches, not only the eight bundled programs. Check signed arithmetic,
shifts, bounds and behavior at extreme legal parameters.

Independent fixtures cover eight patches with PMD/AMD disabled, sampled at32 points
per patch. They establish useful core agreement, not every DX7 behavior. Algorithm4/6
feedback changes have dedicated tests but need comparison against a trusted full
reference; the original MSFA reference omitted that case. AMD and sync-off phase
modeling are approximate. No Yamaha hardware-bit-exact claim is justified.

Inspect firmware/app/engine.c,h for keyboard AND sequencer routing, patch CRC/cache
updates, legacy/DX7 source transitions, simultaneous release tails, mixed gain,
activity publication and shared-song access under en_lock. Confirm patch edits
never silently reattack/retune held notes. Confirm no blocking USB/flash/screen,
heap, runtime libm or forbidden platform access enters the audio path.

### 2. Song compatibility and persistence

Inspect firmware/song/song.c,h, song_io.c, dx7_edit.c,h and native storage functions.
Classic exports remain4508-byte v4; DX7 projects use4764-byte v5, appending two128-byte
snapshots before CRC. Readers accept v1–v5. Old firmware rejects new v5 files.
Check version/length combinations, active versus inactive patch validation,
transactional rejection, CRC boundaries, maximum buffers and all consumers using
SONG_ENC_SIZE versus SONG_ENC_MAX. Search the entire tree for fixed-size assumptions.

Classic IDs0–7 retain their meaning; chord/bass DX7 IDs64–71 and edited ID72 are
new. Ensure no source ID is used blindly as an index into old preset-name tables.
Ten16 KiB slot strides and old eight addresses must remain unchanged. Verify partial
write/error behavior and factory fallback; header-last validity is not the same
as retaining a prior project through a power interruption.

Per-song looks are device-slot settings (44-byte preference record, legacy migration),
not embedded in MKS files. DX7 patch data IS embedded. Do not confuse these two
portability contracts. Examine SAVE's long-tail deferral and settings-save failure
handling as well as successful mock/emulator paths.

### 3. Device controls and held-note behavior

Inspect native/dinghy.c and *_ui.inc, especially bank_ui.inc. Exercise browse versus
load versus edit; stale cursor when switching tracks; all operator/page mappings;
packed-field masks preserving neighbors; ratio/fixed and waveform toggles; reset,
back, save, playback and error/confirmation states. Check whether hints and values
fit240x240 and whether advanced controls are understandable on the real panel.

Review physical-key note-off ownership across page/track changes, repeated notes,
overlapping same-pitch keys, octave changes, queue-full conditions, and old PATTERN
and LYRICS editing/audition paths. Silent SELECT must still create no extra note-on,
forced release, pitch change or stuck note during idle, held-note and song playback.

### 4. GEN, lyrics, library and visuals

Review determinism/track locks and snapshot retention for DX7 instruments. GEN's
candidate presets are NOT yet automatically remapped to the new DX7 bank; approved
mapping is future work. Locked DX7 tracks preserve their patch snapshots.

Review phrase flags, delayed consonant/harmony event ownership and older vocal
flag imports. Review library overwrite confirmations keyed to target, factory
restore semantics and all ten addresses. Review paired face/body switching,
dirty rectangles, sprite bounds/compression, band clipping, redraw cost and
visual-state synchronization. Artistic preference remains the owner's decision.

### 5. Hardware, USB and resource claims

Pinned Dinghy f77bf03283bbb2c05294b01cdad68a30b410f93d accepts Puya0x856014 and
UC25HQ80 0xB36014; compile assertions retain both and reject unknown IDs. Verify
app, recovery and loader use the same guard. Emulator JEDEC tests exercise actual
save/reboot paths; they do NOT qualify physical chip timing or OTA interruption.

The fixed64 KiB flash reserve was explicitly removed by the owner. Do not restore
it as a hard requirement or call the remaining space “only2 KB.” Actual application
limit581564 bytes remains enforced. RAM/pool margins16384/32768 remain configured.
Review current measured allocations and stack/ISR behavior rather than inventing
another fixed allowance. Ensure further work remains within actual device limits.

USB video is experimental. Current MIDI+UVC profile disables UAC/CDC. A three-frame
offline JPEG benchmark does not prove sustained FPS, enumeration or delivered USB
throughput. USB pedal input/routing, live companion telemetry and simultaneous USB
audio/MIDI/UVC are not completed. Do not approve or describe them as implemented.

### 6. Licensing and evidence integrity

Verify MSFA Apache-2.0 provenance/notices and compatibility with the GPLv3 application.
Verify the redistribution basis for the DX7 program data (the review brief was written for the first
eight-voice Dexed/Synprez shortlist, since replaced by ten original voices),
including that code-engine licensing alone is not proof about a patch archive.
Verify pinned source/program hashes, generated header reproducibility and attribution.
Keep PCL character-art terms, notices and noncommercial restrictions intact.

Check source hashes/profile provenance of the evidence. Cross-profile emulator
restores must copy data sectors only; a whole default-build flash dump overwrites
video-build code and invalidates a supposed video benchmark. See the diagnostic
emulator patch/helper committed with evidence. No physical-flash commands are
needed to perform this review.

## Reproduce checks

```sh
python3 -m pip install -r tools/requirements-dev.txt
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
python3 tools/gen_fvoice_tables.py
python3 tools/gen_fmband_tables.py
python3 -I tools/sprite_pack.py --check
python3 tools/gen_dx7_bank.py --check
python3 tools/gen_dx7_tables.py --check
```

Inspect git diff after generators. The first two regenerate checked-in output;
changes are findings to explain, not files to commit blindly. Full CTest includes
codec, wardrobe, library, compatibility, GEN, voice and DX7 tests.

For the native toolchain/SDK setup use docs/VALIDATION.md and native/build.sh.
The validation workspace used build/tooling/jieli-linux-toolchains-20260730.1 and
build/tooling/sdk. Set JIELI_TOOLCHAIN and AC79_SDK to your real local paths:

```sh
bash native/build.sh
bash native/build.sh --video
bash native/build.sh --video-bench
bash tools/emu_smoke.sh build/native/miku-fm.fwsc build/review-smoke-default /path/to/fm1-emu
bash tools/emu_smoke.sh build/native/miku-fm-video.fwsc build/review-smoke-video /path/to/fm1-emu
```

Native builds share/reset the ignored build/dinghy dependency checkout: run these
builds sequentially. Do not develop permanent fixes inside that ignored checkout;
persistent platform changes belong in repository build/patch scripts.

Optional renderer sanitizer check:

```sh
cc -std=c11 -O1 -g -fsanitize=address,undefined -I. \
  tests/test_dx7.c firmware/audio/dx7/dx7.c \
  firmware/audio/fmband/fmband_mx.c firmware/audio/fmband/fmband_tables.c \
  -o build/dx7-sanitized
ASAN_OPTIONS=detect_leaks=0 build/dx7-sanitized
```

Independent reference regeneration needs the pinned original MSFA jni sources:
`python3 tools/gen_dx7_reference.py /path/to/upstream/app/src/main/jni --check`.
Those Android sources are not required for normal CTest/device builds. Additional
native recording/flash diagnostic details are in docs/DX7_BANK_HANDOFF.md and
committed evidence, not a reason to substitute a browser synthesizer.

## Evidence and current measured baseline

Start with docs/evidence/dx7-bank/device-walkthrough.mp4, dx7-device-pages.png,
restored-operator.png, audio/full-band.mp3 and audio/dx7-1-level-matched.mp3.
Raw listening copies, editable projects, source metadata, persistence and timing
JSONs are adjacent. docs/DX7_BANK_HANDOFF.md maps each artifact and bank program.
The earlier broader feature evidence remains in docs/evidence/native-integration/.

| Profile | Image used/free | Static used/free | Pool used/free |
|---|---:|---:|---:|
| Default |531820 /49744|42512 /55792|255624 /88440|
| MIDI+UVC |538444 /43120|40720 /57584|266080 /77984|
| Offline JPEG |538328 /43236|40720 /57584|266080 /77984|

Reported validation to independently verify:46/46 local host tests, renderer
ASan+UBSan, generated files, actual three-profile native builds, default/video
emulator smoke. GitHub host CI succeeded at c394043 (run42652444649); that CI is
host build/CTest, not physical-device or native-USB qualification.

Native emulator callback maximum:1.881 ms normal46-second exercise;2.718 ms in a
6.5-second high-load configuration versus5.805 ms deadline, zero overruns. Initial
setup1.063 ms. JPEG benchmark:3 frames, no failures,4779 bytes, max tile2.307 ms,
max frame wall101.068 ms,17.9% emulated ISR share, zero audio overruns. The101 ms
frame is a concrete reason to investigate video scheduling/throughput; it does
not establish a steady frame rate. No physical FM-1 was touched.

## Required review output

Write `docs/PR21_CLAUDE_REVIEW.md` (or provide the equivalent review report) with:

1. Exact reviewed head/base and working-tree state.
2. Findings ranked by severity, with file/line, reproducer and impact.
3. Checks actually rerun and results; checks unavailable and why.
4. Compatibility/data-loss, audio/deadline, UI and license conclusions separately.
5. Original-request gaps and creative/hardware decisions still outstanding.
6. Recommendation for this milestone, with unresolved blockers stated plainly.

If there are no findings in an area, say what was checked and the limits of that
check. Do not mark the full original assignment finished. Do not merge/deploy,
flash hardware, or silently change the bank/genres/artwork as part of a review.

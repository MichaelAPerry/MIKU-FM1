# MIKU-FM1 — detailed engineering handoff for Claude review

Updated: **2026-11-09 UTC**. Owner/product designer: Michael Perry.
Repository: <https://github.com/MichaelAPerry/MIKU-FM1>.
Development branch: **`feature/miku-nextgen-performance`**.
Review PR: **[PR21, draft](https://github.com/MichaelAPerry/MIKU-FM1/pull/21)**.
Original and currently verified main base: **`88b0ef3e568cf615eb5dfaa42d170d8c0f75d431`**.
Implementation snapshot before this documentation handoff: **`984383b`**. Fetch the latest feature-branch tip for review; later handoff commits document the same implementation.

## 1. What the owner authorized, and what this handoff does not authorize

The owner explicitly authorized pushing the completed development commits and artwork **to a feature branch** on 2026-11-09 and requested a verbose handoff for Claude to review **before integration into main**. An earlier automatic approval review had blocked the push for lack of explicit publication authorization; that authorization has now been supplied. It is not permission to skip technical review.

- Keep work on `feature/miku-nextgen-performance`; PR21 stays a draft pending review and owner decisions.
- **Do not merge, push to main, publish a release/tag, deploy the live website, or flash hardware without the owner's separate authorization.**
- The owner remains the final decision maker for genres, costumes, hairstyles, animation concepts, new interfaces and major behavior changes.
- Keep Dinghy. No firmware-platform migration was approved or performed.
- Preserve existing four-track songs, the native 240×240 interface, existing original artwork, saved-song compatibility, updater/rescue workflow, website and real-firmware emulator.
- The owner was unhappy that an earlier draft PR was presented before the complete assignment was fulfilled. **This branch is a development milestone, not completion of the full next-generation request.** Do not describe the prototypes or concept board as finished product features.
- Do not send external review requests, messages or emails on the owner's behalf. The owner said they will have Claude review the branch; this document makes that review self-contained.

## 2. Read this first: implemented versus unfinished

| Area | What exists | Where it runs | What is still missing |
| --- | --- | --- | --- |
| Silent syllable selection | STAGE/DANCE SELECT no longer posts an audition note or release deadline | Default native firmware | Owner live-performance feel check; intentional PATTERN preview remains separate |
| ANIMAL cadence | Integer 140–220 ms default, adjustable session rate | Default native firmware | Final listening choice, richer pauses/phrasing, persistence decision |
| Formant loudness | Bounded opt-in pitch/vowel compensation, baseline unchanged at zero | DSP API and host comparison; code is available in native voice | Native control/persistence, wider character/source/legato validation, remaining high-note loss |
| Word/phrase building | Transactional mora compiler, duration/rest/boundary/articulation/melisma units | Host reusable library/tool | Bounded native phrase player, editing UI, reusable sequence storage/versioning |
| Backing instruments | Two additional bass presets, two drum kits; two opt-in chord profiles | Bass/drums selectable in native MIXER; chord profiles API audition | Chord profile selection/persistence, further mixing/effects/aliasing work, listening approval |
| Modern GEN | Five genuinely distinct candidate engines, energy/complexity, deterministic editable songs and locks | Host experimental generator | Owner lineup approval; eliminate large stack scratch; native UI integration |
| Stage system | Shared clock-driven animation state, original outfit registry, procedural background/frame concepts | Default state native; other themes preview tools | Selectable native themes, performance effects/choreography beyond baseline, production outfits |
| Sprite storage | Lossless shared-row/RLE/delta decoder for both faces and dancers | Default native and optional video | Additional measured production costume packs and paired switching |
| Wardrobe | Eleven paired full-body/portrait concepts, including corrected bodysuit and witch | Committed review artwork | **No new production portrait/dance atlases; native registry still has ORIGINAL only** |
| USB video | Optional native MIDI+UVC profile, integer incremental JPEG, driver adapter and class/ownership tests | Actual target build and emulator; fake-SIE protocol tests | **Real host enumeration, delivered video, latency and sustained stream validation** |
| USB pedal | Integer stereo input-analysis/effect/mix pipeline | Host library and WAV processor | **USB playback OUT endpoint, feedback/rings/clock handling and native routing** |
| Computer performance events | MIDI-safe codec, actual-engine JSON/WAV/SysEx export, recorded-state companion | Host/offline browser review | Native live sender/receiver, reconnect/queue policy, true body mouth compositing |

The optional UVC profile is **MIDI + video with native physical audio output**, not proof of simultaneous USB audio playback/capture + MIDI + video. It deliberately disables UAC and CDC. Default firmware retains its original USB profile.

## 3. Review priorities and what Claude should return

Review actual code and tests, not only this document. Return findings with severity, concrete file/line references, a trigger or failure scenario, and a fix recommendation. State which claims you independently reproduced and which remain historical evidence.

1. **Live voice regressions:** no encoder note-ons, forced releases, held-pitch changes or stuck notes; existing PATTERN preview remains intentional and isolated.
2. **USB/update isolation:** patched driver only in opt-in video app, original loader source preserved, default USB untouched, EP1 MIDI/updater intact, correct descriptors and class request routing.
3. **Ownership/races:** main-loop JPEG publication, TIMER5-only class/SIE mutation, DMA lifetime through completion, close/reset/suspend/timeout and late-publication cancellation. Look for re-use of a buffer before the last payload has actually completed.
4. **Real-time/memory:** no JPEG/render/blocking operations in either ISR, bounded packet/MCU work, no raw atlas duplication, no libc/float/heap/64-bit division in device DSP, runtime stack scrutiny in addition to linker figures.
5. **Compatibility:** old preset output hashes, old MKS2 files, unchanged four-track/grid/model layout, correct default/bypass behavior. Do not consume reserved song bits casually.
6. **Experimental scope:** decide whether this large draft should be split into smaller integration PRs. Stable fixes/compact assets and opt-in prototype code have different readiness levels.
7. **Evidence quality:** emulator timing is not hardware timing; offline JPEG encoding is not actual USB delivery; generated images and sound renders are not owner approvals.

Recommended review result: a list of blockers, nonblocking follow-ups, tests performed, and a recommendation on a conservative merge scope. Do not auto-merge after a green host suite.

## 4. Required background documents and current source map

Read `README.md`, `docs/PROJECT_STATE.md`, `docs/VALIDATION.md`, `docs/SONG_FORMAT.md`, `docs/CONTROLS.md`, `docs/MANUAL.md`, and `LICENSING.md`. The current `docs/HANDOFF.md` is an index to this document. Historical stacked handoffs were preserved under `docs/archive/HANDOFF_BEFORE_CLAUDE_REVIEW.md`; their old automatic-merge instructions and measurements are **not current authority**.

| Files | Responsibility |
| --- | --- |
| `native/dinghy.c` | Actual native controls, rendering, audio callback, optional main-loop video producer |
| `native/build.sh` | Pinned Dinghy/toolchain integration, profile isolation, package outputs, resource guard |
| `firmware/app/engine.c,h` | Audio command ring, sequencer dispatch, ANIMAL/SCAT timing, voice/harmony/mix |
| `tests/test_live_select.py` | Compiles actual native SELECT/rate branches against actual host engine |
| `tests/test_engine.c` | Audio timing, release, language isolation, effects and engine regressions |
| `firmware/audio/fvoice/*`, `tools/gen_fvoice_tables.py` | Existing formant engine plus opt-in compensation table |
| `tests/test_fvoice.c`, `tests/test_mora_voice.c` | Voice spectrum/pitch/envelope/consonant and compensation coverage |
| `firmware/song/lyrics.c,h`, `firmware/song/tests/test_lyrics.c` | Reusable word/phrase compiler and real-voice articulation tests |
| `firmware/audio/fmband/*` | Chord/bass/drum DSP, presets, band audition tool and tests |
| `firmware/song/gen_next.c,h`, `firmware/song/tests/test_gen_next.c` | Contemporary candidate generator; original `gen.c,h` retained |
| `firmware/ui/performance.c,h` | Original outfit metadata, scene state, background/frame/light functions |
| `firmware/ui/sprite_codec.c,h` | Bounded lossless row and pixel decoder |
| `firmware/ui/singer_compact.h`, `tools/sprite_compact.py` | Generated compact character pixels; original header retained as reference |
| `tests/test_sprite_codec.*`, `tests/test_sprite_decode.py` | Exact pixels, corrupt input, generation, actual native LCD rendering |
| `firmware/usb/jpeg/*` | Bounded incremental integer JPEG, actual performance-pixel callback, provenance |
| `firmware/usb/video/uvc.c,h` | Probe/Commit and frame/payload ownership core |
| `native/video_usb.inc`, `tools/patch_dinghy_video.py` | Optional real Dinghy EP0/EP4 adapter and SHA-checked patch |
| `tests/test_native_video.py` | Real patched USB source compiled against fake SIE HAL; separate integration gate |
| `tests/test_native_video_main.py` | Actual native video producer: incremental progress, immutable frame, disconnect/restart, benchmark gating |
| `firmware/audio/pedal/*` | Independent stereo analysis/effect/routing DSP and tests |
| `firmware/app/performance_wire.c,h`, `tests/test_performance_wire.c` | Fixed 28-byte MIDI-safe state codec |
| `companion/*`, `tools/performance_export.c` | Recorded actual-firmware performance review; no JavaScript synth |
| `config/resource_budget.json`, `tools/check_resources.py` | Minimum free margins checked against real native image/linker symbols |
| `tools/video_emu_bench.py`, `tools/video_bench_report.py` | Target encode benchmark and reliable memory-dump/counter interpretation |
| `assets/singer/wardrobe/*` | Review-only concept sheets and unapproved catalog |
| `docs/NEXTGEN_*.md` | Per-module design, measurements, reproduction and explicit limitations |

## 5. Native live-performance changes

### Silent SELECT

STAGE/DANCE encoder movement only changes `ed_mora` and requests visual redraw. It posts neither `EN_ON` nor `EN_OFF` and creates no `pv_off` deadline. The existing dial animation remains. A held note is left alone; the next key posts the selected mora normally. PATTERN's ALGORITHM handler retains a separate stopped/no-held-key 300 ms audition.

Host tests execute the actual source branch and check the complete engine/mailbox and rendered samples. Real-package emulator evidence: idle SELECT is exact silence versus the old preview's peak 7887/RMS 1917.1; a held C4 with SELECT changes is bit-identical across 89,600 stereo frames to the no-turn reference; release ends silently.

### ANIMAL

The same integer helper schedules the first and every subsequent ANIMAL interval. Rate 50 defaults to 140–220 ms. Rate 0 gives 180–260 ms, rate 100 gives 100–180 ms. STAGE/DANCE mapping: **hold SCL/SEL + turn PRESETS** while in ANIMAL. It consumes the short language tap and keeps tempo unchanged. Rate affects the next interval without resetting the held-note timer. It is session-only, initialized to 50 at boot, not saved in MKS2.

Existing intended ANIMAL pitch jitter remains; the base note is retained. JP, SCAT and VOWELS have independent behavior. Variable pauses, word-like phrasing and a final listening-selected cadence are not implemented. Review long-press SOURCE-layer interaction and encoder feel on a real device when authorized.

Four-second rendered note: 51→23 onsets; mean interval 79.31→177.91 ms; old range 70–89.71/new 141.22–219.82 ms. Peaks 24,297/22,592, no full-scale samples, no stuck gate. Sample-level tests exercise boundary note-off, rate changes/clamping and language isolation.

## 6. Voice, lyrics and instrument status

### Compensation

`FvVoice.level_comp` is zero by default; 100 enables a measured pitch/vowel correction up to +6.02 dB. It is limited to the calibrated MIKU character/fine-formant neutral case. It uses the original voice architecture, excludes consonant phases, ramps gain and preserves oscillator timing. Native code contains the evaluation path, but no device knob or saved setting enables it.

All five vowels/MIDI 48–96 were compared; low-register/baseline samples, pitch/phase and fresh consonants retain their tested behavior. A96 RMS improves 614→1228 but remains substantially quieter than the reference register. Existing raw U92 at velocity 127 already overloads and is not fixed here. No claim of global clip-free behavior or completed perceived-vowel improvement. More source/character/legato/effect combinations and listening remain necessary.

### Lyrics

`ly_compile()` produces caller-owned four-byte `LyUnit` arrays. Example: `mi-ku:2 _:4 |:2 a:3! / i`. Hyphens separate morae within a word; spaces retain word boundaries; `:N` sets ticks, `/` pauses, `|` creates a phrase pause, `!` requests separate onset, `_` sustains the preceding vowel without its consonant. Unsupported morae are rejected; two-pass parsing preserves output on errors.

The audition uses the actual fvoice engine and reports actual mouth/consonant state. This is not native lyric editing or saved lyric sequences. The inherited Japanese inventory still lacks yoon/gemination and broader phoneme combinations. Do not introduce another language engine or squeeze new semantics into reserved song bits without review.

### Band

Original eight chord/four bass/two drum outputs retain frozen regression hashes. New native MIXER IDs:

- Bass 4: **SUB DROP**, decaying pitch onset, stable base/sub, existing glide, saturation.
- Bass 5: **HARD ACID**, filter-envelope articulation and saturation.
- Drums 2: **PUNCH**; drums 3: **DIGITAL**, compact synthesized parameters, no samples.
- `ch_set_modern(s,0/1)` auditions **DIGITAL PLUCK/GLASS PAD** with optional detuned carrier. It stays outside the full stored eight-entry chord bank; no new native chord selection interface or fifth track was added.

Review clipping under full voice+band mixes, not just solo auditions. A/B examples are not loudness matched. Anti-aliasing, stereo unison, richer routing, backing-band effects, sidechain ducking and a timbre-editing UI are not complete. Choosing new bass/drum IDs in a newly saved song does not promise those timbres on older firmware even though the file layout remains valid.

## 7. GEN candidates and native-integration blocker

The original native GEN page still uses POP/BALLAD/DANCE/CHIP. `gen_next` is a separate host-enabled candidate system: Hyperpop, Future Bass, Jersey Club, Digicore/Glitch Pop, Electropop/Idol Pop. Candidates differ in tempo, form, harmonic tendencies, kick/hat/fill language, bass motion, recurring motifs, vocal density and existing preset choices. It is not a rename table.

Generation is deterministic from settings/seed, returns ordinary editable Song/MKS2 output, preserves locked tracks/arrangement, and supports localized phrase regeneration. It refuses context-capacity overflow transactionally. Tests cover 360 combinations. Ten seed/settings WAV/MKS examples exist locally and are reproducible.

**Do not drop the prototype into native UI unchanged:** target assembly showed approximately **5,032 bytes of stack scratch**, plus registers/callees. Replace with explicit caller-owned storage and measure stack high-water/generation time before native integration. A locked row with intrabar chord changes is currently followed using its first chord. Energy/complexity and candidates are not persisted. No final genre lineup, fine-grid/triplet extension or native control mapping was approved.

## 8. Wardrobe requirements and compact artwork

The owner wants at least ten outfits if practical, permits adjusting the count, and explicitly wants overhead retained for USB video. They stressed that **both the singer-face sheet and dancing-sprite sheet must change together**. All three original references were inspected: `reference.webp`, `faces.webp`, `stage.webp`.

Current review board: [`assets/singer/wardrobe/concepts-v3.png`](assets/singer/wardrobe/concepts-v3.png). Eleven concepts plus the original retained:

1. Encore; 2. Off Duty; 3. Retro Wave; 4. Prism; 5. Neon Punk; 6. City Beat;
7. **Signal Suit** — full-length sci-fi bodysuit. The owner's earlier “zettai” term was corrected; do not implement the discarded skirt/thigh-high interpretation.
8. **Summer Beat** — requested sporty bikini, replacing Rain Signal.
9. Snow Glow; 10. Festival; 11. **Moon Circuit** — requested witch with hat and proposed braided twin-tails.

Hair variants are allowed as concepts but require matched atlases, anchor/bounds checks and more unique storage. Keep MIKU recognizable; no arbitrary switch to generic unrelated Miku art. The catalog marks all designs unapproved with null production-atlas paths. **There are no new runtime costumes yet.** The contact sheet must not be treated as an aligned production sprite sheet.

For each chosen skin, create all 12 portrait states and 8 dance poses, match collar/shoulders/accessories/hair, validate palette/transparency/anchors/dirty rectangles, then preview both actual native pages. Switching outfits must update both views and repaint all affected pixels. Real full-body vowel mouth compositing is still unfinished.

### Compact format now native-integrated

Rows are deduplicated; each is raw 4bpp, nibble RLE, or one delta against a direct row. No recursive/multilevel deltas. `spr_decode_row()` uses a caller's 90/61-byte row buffer and leaves output untouched on invalid data. `spr_pixel()` serves the video callback. The original header/art is retained for reference; the native linker discards unreachable raw arrays.

Original portrait+dance data: **65,928 B**. Compact data/maps: **32,884 B**, preserving all **131,848 pixels**, palettes and dirty rectangles. Actual linked default firmware saving: **32,512 B** after decoder/metadata. No extra pool/static allocation. Existing odd-width 61-pixel dance rows now decode correctly.

Do not multiply that compression ratio to promise a fixed outfit count. New hats, braids, motion and palettes may compress differently; the decoder uses bounded 16-bit offsets/atlas limits. Measure each production pack and consider shared clothing/head data across outfits before allocating every remaining byte.

## 9. USB video: actual code, scope and review hazards

### Three distinct build profiles

- `bash native/build.sh`: normal instrument, compact original artwork, normal USB behavior.
- `bash native/build.sh --video`: isolated development **MIDI + UVC** app with physical synth output. UAC/CDC disabled. Not a release profile.
- `bash native/build.sh --video-bench`: offline emulator benchmark that encodes three frames under live vocal+band load then freezes the final JPEG for inspection. **Not a camera-stream test.**

The script refuses combining development video with `--release`. Its dependency checkout under `build/dinghy` is deliberately reset/cleaned to the pinned revision; do not develop permanent platform edits there. The persistent patch belongs in this repository.

### Driver/class integration

`tools/patch_dinghy_video.py` SHA-checks the exact pinned `usb.c` and fails closed if source changes. A wrapper selects the patched driver only for an opt-in non-loader app; the exact original source remains the fallback for loader/default. A first native attempt exposed loader inclusion, which was corrected and rebuilt successfully. Review that isolation, not merely the preprocessor flag name.

Configuration: 252 bytes, four interfaces, MIDI EP1 OUT/IN, video VC2/VS3, alt0 with no bandwidth, alt1 EP4 IN isochronous 512 B/1 ms. UVC1 MJPEG advertises 160×120, nominal 15 fps, 8,192 B/frame cap. Existing project test VID/PID retained; bcdDevice changes for host cache separation. It is not a new production USB-ID allocation.

Probe/Commit are bounded 26-byte controls. The core normalizes the one supported mode, requires valid Commit before alt1, rejects unsupported indices/lengths, produces FID/EOF/ERR payloads and advances ownership only after acknowledgment. The adapter copies ≤510 JPEG bytes plus a 2-byte header into aligned DMA storage.

Main publishes one immutable frame. TIMER5 alone mutates Muvc/SIE/endpoint state; a queued packet is not acknowledged until hardware TxPktRdy clears. The final frame buffer stays owned through final DMA completion. Alt0, reset, detach and observed suspension flush before release; late main publication after close is cancelled. A 40-poll/~20 ms queued-packet timeout stops the stream and requires reopening alt1. That policy needs real host testing.

### Rendering and codec

The GPL integer JPEG derives from pinned Synthesthetic code; provenance is in `firmware/usb/jpeg/NOTICE.md`. Context is caller-owned. Native allocates one **8,192 B output + 2,240 B context = 10,432 B pool**, no full framebuffer, no second-core job/stack. The video callback reads the same compact original dance atlas as LCD and centers the square scene within 160×120. No substitute browser synth or copied Synthesthetic graphics.

`mj_begin()` starts a snapshot, `mj_step(...,1)` processes at most one 16×16 MCU per main-loop pass; output is published only on successful completion and never modified while USB owns it. No JPEG/render/flash/screen operation was added to audio or USB ISR. Verify target integer arithmetic, overflow failure, buffer bounds and source snapshot stability.

### Validation limits

Host protocol tests include the actual patched driver against a fake SIE, not a rewrite of its behavior. They cover descriptors, EP0, alt1-before-Commit, short/wrong controls, 512-byte payloads, DMA/source lifetimes, close/reset/suspend, abandoned controls, late publication and the MIDI rescue command. Actual native default/video/benchmark packages build and default/video smoke-test in fm1-emu.

26 JPEG fixtures decode with libjpeg/Pillow; raw/compact rendering yields identical JPEG bytes. Scene sizes at quality 65: 3,475–5,027 B. A real 3,794 B MIKU JPEG packetizes into eight packets and reassembles identically.

The target benchmark completed three JPEGs during vocal+band playback: zero failures, recovered 3,794 B JPEG byte-identical to the host reference, maximum MCU wall time **1.403 ms**, maximum complete-frame wall time **32.206 ms**, emulator ISR share **13.1%**, 80,640 audio frames, peak 9,653, no full-scale samples. These are **emulator** measurements, not physical deadline guarantees or delivered USB fps/latency.

No real host has enumerated this branch as a camera. USB PHY/controller behavior, actual DMA/cache effects, OS-specific Probe negotiation, sustained IN scheduling, end-to-end latency, frame drops and update/recovery during/after streaming remain unverified. Do not claim simultaneous USB audio + video. The original community implementation reserved much larger buffers; this deliberately smaller design needs its own tests.

## 10. USB pedal and companion are separate unfinished work

The stereo pedal pipeline analyzes incoming samples before mute/effects, uses independent saturation/filter state, smooths wet/levels/bypass, applies optional synth/headroom mixing and clears stale input on disconnect. State is caller-owned, 44 B, blocks 1–256, no global effect buffers/heap. Unity WAV processing is exact in tests. It is not connected to USB OUT.

Do not call the existing single-instance mono-source voice `vfx` chain on arbitrary stereo input: it would discard a channel and share singer effect state. Real pedal work needs native-rate OUT packets, explicit feedback/clock control, rings/counters/priming and analysis/DSP before final output. Define capture routing deliberately to avoid a DAW monitoring feedback loop.

The 28-byte MIDI-safe performance codec and offline companion are reusable groundwork. Exported JSON/WAV/SysEx comes from the real C engine; the browser only renders recorded state and plays the matching WAV. There is no native live state sender or live WebMIDI receive path, no reconnect/queue priority implementation, and no true body mouth layer. The existing companion shows an actual mouth-code indicator. UVC and event-based visuals are alternative/parallel engineering paths, not a finalized product interface.

## 11. Actual linked budgets and minimum reserves

Limits verified from the pinned native build: image **581,564 B**, static RAM **98,304 B**, pool **344,064 B**. Package size is padded and is not the image budget. Do not add these distinct memory regions together as one interchangeable pool.

| Profile | Image used/free | Static used/free | Pool used/free |
| --- | ---: | ---: | ---: |
| Original main baseline |183,788 /397,776|35,492 /62,812|254,784 /89,280|
| Before compact feature build |185,208 /396,356|35,540 /62,764|254,784 /89,280|
| Current compact default |152,696 /428,868|35,540 /62,764|254,784 /89,280|
| Optional MIDI + video |159,080 /422,484|33,732 /64,572|265,216 /78,848|

Video static RAM is lower because this profile excludes UAC capture, not because all USB functionality became free. Runtime stack high-water is not measured by these linker figures. The audio callback remains 256 frames/44.1 kHz, approximately **5.805 ms**.

`config/resource_budget.json` enforces minimum free image **65,536 B**, static **16,384 B**, pool **32,768 B** after linked allocations. `native/build.sh` invokes `tools/check_resources.py` against the actual app and linker symbols. Keep these floors until explicit measured reallocations justify a deliberate change; never weaken them silently to squeeze in more costumes. They are planning margins, not a guarantee that future USB combinations fit.

A durable machine-readable snapshot with artifact hashes is committed at [`docs/evidence/NEXTGEN_REVIEW_SNAPSHOT.json`](docs/evidence/NEXTGEN_REVIEW_SNAPSHOT.json). Most large evidence remains generated/ignored, as described below.

## 12. Exact verification workflow for a fresh reviewer

### Checkout and inspect

```sh
git fetch origin
git switch feature/miku-nextgen-performance
git status --short
git log --oneline origin/main..HEAD
git diff --stat origin/main...HEAD
```

Confirm the target branch, expected base and any newer changes before destructive operations. Do not reset the feature branch to main or force-push away review commits.

### Generated files, then host build — sequentially

Requires a C11 compiler, CMake, Python3; the original sprite packer also needs Pillow and NumPy. The host CTest suite itself does not require a JieLi toolchain. Some existing host code/tools link libm; the device DSP does not.

```sh
python3 tools/gen_fvoice_tables.py
python3 tools/gen_fmband_tables.py
python3 -I tools/sprite_pack.py --check
python3 -I tools/sprite_compact.py --check
git diff --exit-code -- firmware/audio/fvoice/fvoice_tables.h firmware/audio/fmband/fmband_tables.c firmware/ui/singer_sprites.h firmware/ui/singer_compact.h
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
git diff --check
```

If CMake is installed as the Python wheel and not on PATH, `python3 -m cmake` is equivalent. **Do not run table regeneration concurrently with compilation:** the generators rewrite headers and a compiler may briefly read an incomplete file. During the handoff rerun that orchestration mistake caused a transient compile failure; the sequential rerun passed and generated files remained unchanged. It was not a persisted source failure.

Current full host result: **26/26 pass**, rerun 2026-11-09. Release packet tests explicitly retain assertions. Read test logic, including source-extracting native adapters; a green test is not automatically evidence of coverage for every interface or race.

### Native profiles

Verified prerequisites: JieLi Linux toolchain `jieli-linux-toolchains-20260730.1`, pinned Dinghy `f77bf03283bbb2c05294b01cdad68a30b410f93d`, AC79 SDK revision `d179b4484759423312073f5fbb232501aa491047`. Only the required SDK binary files are needed locally; their hashes passed the upstream build checks. See historical setup recipe if installing from scratch.

In the retained workspace, toolchain and SDK are under `build/tooling/`. On a new machine set these paths to its own installation; do not assume this container exists.

```sh
export JIELI_TOOLCHAIN=/path/to/jieli-linux-toolchains-20260730.1
export AC79_SDK=/path/to/ac79-sdk
bash native/build.sh
bash native/build.sh --video
bash native/build.sh --video-bench
```

Outputs under `build/native/`: `miku-fm.fwsc`, `miku-fm-video.fwsc`, `miku-fm-video-bench.fwsc` plus matching `*-app.bin`, `*-symbols.txt`, `*-resources.json`. All are development packages, not published/attested releases. Check register access, call-free `.ram_text`, image/RAM/pool gates and retained free reserves. The unchanged rescue/update loader package was 9,067 B in these builds.

Build each profile separately; the generated `build/dinghy` checkout is shared and may be reset by another build. Preserve each profile's binary/log/evidence before invoking the next one. Keep the actual source outside this disposable tree.

### Separate real-driver protocol test

After `native/build.sh` has cloned Dinghy, or using another checkout containing the pinned commit:

```sh
python3 tests/test_native_video.py build/dinghy
```

This test reads the original pinned source with `git show`, so it does not trust a previously patched worktree. It needs a host `cc`. It is intentionally separate from the normal fresh-clone CTest workflow, which has no Dinghy checkout. No actual USB hardware access occurs.

### Real package emulator gates

Build fm1-emu at `9de1ef0` with Rust/Cargo; executable is normally `build/fm1-emu/target/release/fm1-emu`. Always create an output `work/` directory or use the smoke helper, which creates it.

```sh
tools/emu_smoke.sh build/native/miku-fm.fwsc build/emu-default build/fm1-emu/target/release/fm1-emu
tools/emu_smoke.sh build/native/miku-fm-video.fwsc build/emu-video build/fm1-emu/target/release/fm1-emu
python3 tools/video_emu_bench.py
python3 tools/video_bench_report.py build/evidence/video-bench
```

Both smoke fixtures previously passed: 124,928 frames, RMS approximately 1,504, 23 colors, emulator ISR 8.6%. The target benchmark is slower in wall-clock execution than its two emulated seconds. Its report requires Pillow for independent JPEG decoding. Listing parser distinguishes numeric unknown-instruction u16 text from raw byte listings; otherwise counter endianness can be misread. The recovered target JPEG must match the host reference, not just start with SOI.

### Host video/JPEG checks

```sh
mkdir -p build/evidence/video-review
build/host/video_render build/evidence/video-review
python3 firmware/usb/jpeg/check_decode.py build/evidence/video-review
build/host/firmware/usb/video/muvc_packet_stream build/evidence/video-review/stage-bg0-frame4.jpg build/evidence/video-review/frame.uvcp build/evidence/video-review/frame-reconstructed.jpg
```

Consult the packet tool's usage for its output format and the core doc for reconstruction. Frame packetization is not a real host transaction. The original/compact callback modes were separately compiled and yielded identical fixture bytes.

## 13. Recreate listening and visual review material

Large WAVs, packages, logs and most PNGs live in ignored `build/evidence/` and `build/nextgen/`. **A GitHub checkout does not include them.** Do not assume the author's local review HTML is a remote-hosted product or that all its optional files exist on a fresh checkout. The committed concept boards and durable JSON snapshot are available in Git.

```sh
mkdir -p build/evidence build/nextgen
build/host/firmware/audio/fmband/band_demo build/nextgen
build/host/firmware/song/lyrics_render build/evidence/lyrics-phrase.wav > build/evidence/lyrics-mouth.csv
build/host/performance_preview build/evidence/stage
build/host/performance_export build/evidence/performance.json build/evidence/performance.wav build/evidence/performance.syx
cc -O2 -I. tools/voice_compare.c firmware/audio/fvoice/fvoice.c -lm -o build/voice_compare
build/voice_compare build/evidence/voice > build/evidence/voice.csv
```

Candidate style IDs 0..4 map to Hyperpop, Future Bass, Jersey Club, Digicore, Idol Pop. For each, render seed1/energy45/complexity30 and seed42/energy90/complexity85:

```sh
build/host/firmware/app/gen_next_render build/nextgen/hyperpop-1 0 1 45 30
build/host/firmware/app/gen_next_render build/nextgen/hyperpop-42 0 42 90 85
```

Repeat with IDs1..4 and meaningful prefixes. It writes editable `.mks` plus actual-engine stereo WAV. `song_render` and `mora_render` still use the real C engine for baseline/demo/mora auditions. To regenerate ANIMAL before/after, compile `tools/animal_render.c` against both the original-main and current engine in separate worktrees/build directories; do not overwrite current sources to create a baseline. The other module reports explain their historical comparisons.

For the recorded companion:

```sh
python3 -m http.server 8765 --bind 127.0.0.1
# In another terminal with Playwright + Chromium installed:
python3 companion/test_review.py
```

Open `/companion/index.html`, load matching exported JSON/WAV. Test currently assumes Chromium at `/usr/bin/chromium` and generated evidence; adapt only the local test path if needed. It does not connect to live USB. Stop the local server after review.

`python3 tools/nextgen_review.py` assembles a local evidence index after artifacts are generated. Some previews are PPM; PNG copies may need regeneration. It skips unavailable audio and is not a completeness check. The wardrobe board is committed separately and is not installed into the live website.

## 14. What still needs doing before main and before the full request is complete

### Before recommending any merge scope

- Independently review fixes and actual tests, and run the fresh-checkout host/generated/native gates for the selected scope.
- Confirm hosted CI on the **latest pushed SHA**. Existing `host.yml` runs only configure/build/CTest; it does not independently certify native hardware, UVC enumeration or all generated-art checks.
- Review the optional build patch/loader fallback, ownership/reset paths and the new memory guard. Audit default versus video differences explicitly.
- Decide whether to split stable default fixes/compact assets from the video and other experiments. Keep genuinely experimental features opt-in and documented.
- Have the owner listen to ANIMAL/voice/band/GEN comparisons and review intended behavior. Mechanical tests cannot approve music or intelligibility.
- Clarify whether optional unverified UVC may be merged as a development profile, or whether it must stay separate until actual host validation.
- Understand the deployment consequence: `.github/workflows/pages.yml` triggers on a push to **main** and can update the live site. It normally carries the latest published release rather than this development package. **A merge is not an isolated code-only action in this repository.** Do not trigger that side effect without the owner's approval.

### Before claiming finished wardrobe/visual presentation

- Produce approved, aligned **paired** portrait/dance atlases for the chosen concepts; no placeholder reuse of the original outfit for all IDs.
- Measure packed flash cost per outfit, shared pixels and decoder/display time; keep USB reserves and adjust count with the owner.
- Implement deliberate selection/persistence, paired redraw, backgrounds/frames/effects/choreography, and actual lip synchronization as reviewed product behavior.
- Keep both 240×240 native and optional computer presentation; preserve original assets and PCL attribution.

### Before claiming USB camera/pedal functionality

- Obtain explicit hardware-test/flash authorization. This branch has never been flashed to the owner's FM-1.
- On an authorized device, capture descriptor/control traces and real MJPEG data; test target OS/camera/OBS consumers, nominal versus delivered fps, stalls, latency, reconnect, MIDI traffic, synth load and thermal/runtime stability.
- Verify updater/rescue and saved-song behavior before/after using video. Do not assume mock tests cover the PHY/DMA/controller.
- Implement USB playback OUT/feedback/rings and pre-effect routing for pedal mode. Add counters and sustained tests for underruns, overruns, clock drift and audio latency.
- Only then attempt deliberate combined configurations and real host bus-budget testing. Current video disables UAC; current default does not provide computer-playback pedal input.

### Before completing remaining musical features

- Rework GEN scratch storage, refine approved styles and integrate native editing/controls without changing old song semantics silently.
- Add bounded native lyric playback/editing and a reviewed versioned persistence design.
- Address remaining voice high-note loss, baseline U92 overload and articulation/expression work with audible evidence.
- Continue instrument/mix/filter/effect work and validate singer clarity above backing. No fifth track or unrelated language engine has been approved.

## 15. Licensing, provenance and assets

Firmware code remains GPL-3.0-only; old MIT-marked prototype files retain their notices. JPEG provenance is in `firmware/usb/jpeg/NOTICE.md`, pinned Synthesthetic `589bd0f30b68b66f78dab84debb42909131f4cad`. No platform migration or foreign graphics were introduced.

The singer and all original/generated/compact character pixels are Hatsune Miku fan art under the **Piapro Character License**, not GPL/MIT. Preserve the credit and the free/non-commercial/unofficial presentation. Compressing the pixels does not change their license. The historical art prompts asking for an “original character” do not override actual established identity/licensing or the owner's continuity requirement. See `LICENSING.md` and `assets/NOTICE.md`.

## 16. Suggested first Claude review pass

1. Read sections1–3 and inspect the current remote diff against main.
2. Review native controls/engine first and run the26 host tests plus generated checks sequentially.
3. Review compact decoder/native draw paths and verify actual output against original atlases.
4. Review optional USB/JPEG/loader/ownership code and run the separate real-driver test and native builds.
5. Distinguish blockers from unfinished but properly isolated prototypes. Report a recommended merge/split scope and remaining owner choices.
6. Stop before main/release/deployment/hardware actions; let the owner decide after review.

This is the next review step, not permission to treat the entire next-generation assignment as complete.

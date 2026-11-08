# Next-generation development log

Branch: `feature/miku-nextgen-performance`, from `origin/main` `88b0ef3` (2026-11-08).
No release, website deployment, main merge or physical-device flash is authorized or performed.
The original art, four tracks and MKS2 saved-song layout remain the compatibility baseline.

## Phase 1: live performance

STAGE/DANCE SELECT now changes only `ed_mora` and requests redraw. It creates no note-on, note-off, audition deadline or pitch command. The visual wheel retains its existing main-loop animation. Live keys still post the selected mora on their next note-on. PATTERN ALGORITHM retains its separately guarded 300 ms editor audition, disabled during playback or held keys. No new audition gesture has been selected.

`tests/test_live_select.py` compiles the actual native SELECT handler against the real engine. It checks mailbox and full engine state unchanged, exact output samples idle/held/playing, key release, wraparound and next-key syllable. It is a host adapter, not a HAL/emulator test.

ANIMAL uses the same integer sample-counter helper at initial note and every subsequent articulation. Default intervals 140–220 ms (previously 70–90); speech rate 0/50/100 gives 180–260/140–220/100–180 ms. On STAGE/DANCE in ANIMAL, hold SCL/SEL and turn PRESETS; it consumes the short language tap and keeps tempo unchanged. Rate affects the next scheduled interval and does not retrigger a held note. It is session-only and resets to 50 at boot, avoiding a silent change to saved songs. Existing ANIMAL pitch-jump character remains; the held base note and release are unchanged. JP/SCAT/VOWELS keep independent timing.

Measured over a four-second held note (same engine, same initial settings): old/new 51/23 onsets, mean interval 79.31/177.91 ms, peaks 24297/22592, no full-scale samples, release gate off. Evidence `build/nextgen/animal-{before,after}.{wav,csv}`. Reproduce using `tools/animal_render.c` linked to each revision's engine. This is measured timing, not a listening judgment. Final cadence, pauses and phrasing need owner listening review.

Host CMake Release suite passed 13 tests including SELECT before later independent modules were added; final aggregate results are recorded below. Generated voice/band tables and sprite-header check passed. Native/emulator final results pending.

## Phase 2: voice and lyrics

See [voice evaluation](NEXTGEN_VOICE.md). Compensation is opt-in in `FvVoice`, baseline 0, not yet a device control or saved setting. A96 gains 6 dB but still has substantial high-note loss. Raw U92 at maximum velocity already clips; no global no-clipping claim. Text/phrase compiler and articulation evaluation are in development; final integration is recorded below.

## Phases 3–6

Backing presets, candidate generator and modular visuals are development experiments; final product lineup and visual designs require owner review. USB research is complete in [USB research](NEXTGEN_USB_RESEARCH.md); no USB stack rewrite or hardware functionality is claimed. UAC input must be available before app effects; existing mono global voice effects cannot simply be reused for a stereo pedal. Native UVC is a separate feasibility task.

## Resource baseline (actual native build)

`origin/main`: image 183788 / 581564 bytes; static RAM 35492 / 98304; pool 254784 / 344064. JieLi toolchain 20260730.1, Dinghy f77bf03, SDK binaries verified against pinned hashes. Remaining pool 89280 bytes. Host timings and emulator CPU are not physical-device ISR measurements. No hardware timing or USB throughput measured.

## Owner decisions pending

- Listen to default ANIMAL cadence and select final pacing/phrasing; approve or revise the session-rate gesture before final product integration.
- Evaluate compensation audio and choose whether/how to expose it; no replacement default selected.
- Review two examples of each of Hyperpop, Future Bass, Jersey Club, Digicore/Glitch Pop and Electropop/Idol Pop before final GEN lineup.
- Approve costumes/character concepts, backgrounds, frames and choreography before final art.
- Select performance-events companion versus further UVC work after feasibility evidence; no platform migration proposed.

## Phase 2 milestone

The reusable `lyrics.c/h` compiler now supports explicit mora words, durations, word/phrase boundaries, rests, separate onsets and vowel melismas. It uses caller-owned four-byte units, validates before writing, refuses unsupported phonemes, and leaves MKS2 unchanged. `lyrics_render` demonstrates actual voice articulation and exports actual mouth/consonant states. Native lyric editing/persistence and a live phrase-player adapter remain unfinished; see [lyrics](NEXTGEN_LYRICS.md).

## Phase 3 milestone

New native MIXER choices: bass SUB DROP/HARD ACID and drums PUNCH/DIGITAL. Optional chord profiles DIGITAL PLUCK/GLASS PAD remain an API audition because the stored eight-entry chord bank is full. Original preset audio hashes are unchanged. Same-score A/B renders and envelopes/pitch/drop/glide/release/clipping tests are documented in [band](NEXTGEN_BAND.md). No fifth track, supersaw, shared band effects or new operator topology is claimed. New presets use no pool buffers; gain differences in auditions are disclosed.

## Phase 4 milestone

`gen_next.c/h` provides five experimental candidates with distinct forms, rhythmic templates, harmony, bass/melody phrasing, density and energy/complexity. Ten WAV/MKS examples use the actual engine. 360 combinations test determinism, editable valid song output, roundtrips, locked tracks and local phrase regeneration. Existing native GEN remains unchanged until style approval and explicit scratch-storage work: the prototype uses 5032 stack bytes. No tempo/grid/song format change. See [GEN](NEXTGEN_GEN.md).

## Phase 5 milestone

`performance.c/h` separates outfit metadata, procedural scene geometry and absolute clock/beat animation state. Native DANCE uses it with original defaults; atlas storage is unchanged. The original eight-frame dance decoder now reads odd-width rows correctly, tested against every packed pixel. Three 240x240 visual concepts are generated from the actual atlas. Only ORIGINAL outfit is registered; no new costume or genre mapping finalized. See [visuals](NEXTGEN_VISUALS.md).

## Phase 6 milestone

`pedal.c/h` provides a tested integer stereo processing path with pre-effect input analysis, independent saturation/filter state, wet/dry, smoothed bypass/levels, optional synth mix/headroom and disconnect handling. The WAV tool is exact at unity; this is ready-to-feed DSP, not a USB endpoint. `performance_wire.c/h` supplies a versioned MIDI-safe packet; the actual engine exporter records audio plus sample-clocked visual state. The companion renders that recorded state with existing art and attribution. Browser playback/import validation passed. Live endpoint/feedback/ring-buffer integration, live event transmission, true full-body mouth layering and UVC are unfinished. See [pedal](NEXTGEN_PEDAL.md), [companion](NEXTGEN_COMPANION.md), [source research](NEXTGEN_USB_RESEARCH.md).

## Final validation and resource accounting

Host Release suite: 19 tests, including all new modules, before final documentation. Generated voice and band tables leave no changes; sprite header check passes (65928 pixel bytes). Final aggregate rerun recorded in VALIDATION.md.

Actual final native build: image 185208/581564 bytes (+1420); static RAM 35540/98304 (+48); pool 254784/344064 unchanged. Dinghy register checks and .ram_text gate pass. This includes default visual state, silent SELECT, ANIMAL control, voice evaluation branch and added native bass/drum presets. It does not link host-only GEN/lyrics/pedal/wire prototypes.

Phase1 real-package emulator: smoke124928 frames/RMS1504, idle SELECT exact digital silence versus baseline peak7887/RMS1917.1, held-note with SELECT bit-identical across89600 stereo frames to no-turn reference, natural release silent. Rate gesture screenshot shows66/BPM148 unchanged. Emulator ISR8.3% baseline/8.6% phase1; not a physical-device timing result. Final package smoke:124928frames/RMS1504/peak7393/no full-scale samples, emulator ISR8.6%; DANCE ISR10.3%, corrected sprites inspected; MIXER HARD ACID confirmed. Full evidence is in VALIDATION.md.

Compile-only pi32v2 prototype code sizes: lyrics638 bytes; GEN3254 bytes text+constants; pedal870 bytes; wire588 bytes after freestanding-copy correction. No prototype static/pool allocations, except caller-owned state/workspace described in individual docs. Compile-only sizes do not establish linked native budgets or real-time safety.

Physical device ISR deadlines, output-stage artifacts, buffer underruns, USB throughput/latency and UVC frame rate remain unmeasured. No physical device was modified.

## Next concrete coding tasks

1. Refine approved musical/visual examples; implement caller-owned GEN scratch before device UI integration.
2. Implement bounded live lyric phrase-player adapter and versioned persistence proposal after editing-interface review.
3. Prototype Dinghy integer UAC1 input + feedback + ring buffers, expose frames BEFORE pedal DSP, then test native/emulator transport and combined descriptors. Performance-event companion is the recommended first computer renderer; UVC requires its own measured resource prototype.

## Wardrobe follow-up (2026-11-08)

Owner requested at least ten outfits and emphasized matching singer-face sheets as well as dancing sprites. Visually inspected all three source sheets and generated ten paired body/portrait concepts under `assets/singer/wardrobe/`, preserving all original art. No designs approved or runtime skins claimed. See [wardrobe review](NEXTGEN_WARDROBE.md) for concepts, required paired animation states and flash/storage constraints.

Wardrobe revision2: owner requested replacing07 Starlight with Zettai and08 Rain Signal with a bikini (Summer Beat). Generated paired full-body/portrait concepts using the existing contact sheet, face sheet and dance sheet as references; saved revision2 while preserving revision1. No final atlas or runtime skin integration claimed.

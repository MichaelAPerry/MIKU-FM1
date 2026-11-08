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

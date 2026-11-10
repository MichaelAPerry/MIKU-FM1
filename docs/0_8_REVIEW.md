# Claude review handoff: Miku-FM 0.8, smooth and seen

Review `feature/0.8-smooth-and-seen` against `v0.7`; record the full code HEAD and
native evidence manifest's verified code SHA. This branch is a draft, not a
release. The owner permits no experimental installation until Claude review and
main integration. The agent has no hardware access; the owner alone handles later
approved device testing. Do not push main, create release tags, deploy, or flash hardware.

This draft fixes DX7 playback animation and completes the isolated MIDI+UVC
prototype for review. Inspect these concrete video changes:

- `tools/patch_dinghy_video.py` and `firmware/usb/video/uvc.c`: UVC camera-terminal
  control bitmap length now matches the 18-byte descriptor. Inspect the full
  MIDI/video IAD/interface/endpoint tree, Probe/Commit and alternate settings.
  The camera advertises `MIKU-FM VIDEO` and descriptor revision 4.11, while the
  default and rescue USB descriptors retain the exact pinned implementation.
- `native/video_usb.inc`: retain the selected camera across suspend/resume,
  recover stalled endpoint transactions, reject stale JPEGs across stream
  generations, and retain immutable DMA ownership. Check each possible preemption
  between publication fields and owner transfer, alt0, reset and resume.
- `firmware/usb/jpeg/performance_frame.*`: twelve fixed decoded rows replace
  repeated per-pixel decode; verify byte-identical JPEGs for all 720 wardrobe/
  scene/effect fixtures and bounded pool use, no framebuffer or heap.
- `native/dinghy.c`: encoding stays in main context, one MCU per pass, including LCD DMA
  waits. DANCE uses two strips within the existing canvas to overlap rendering
  and transfer; the DMA-source immutability test covers every outfit. Fixed
  decoded-effect rows avoid per-pixel atlas traversal, and status-strip work is
  deferred during the final 20 ms before a DANCE slot. The
  video profile retains MIDI and analog audio, disabling USB audio/CDC because
  video and UAC both use EP4. Do not claim simultaneous UAC+UVC support.
- Two fixed8 KiB compressed buffers decouple encoding from USB output. TIMER5
  swaps the cached pointer only between class frames and returns the old pointer
  through `video_usb_take_free()`. Main never overwrites an owned buffer. Inspect
  the one-slot release invariant, queued publication/EOF ordering, generation
  invalidation and fresh/repeated counters. TIMER5 outranks audio and performs no
  JPEG-sized copy or encoding. The second150-frame driver test pauses encoding
  and verifies exact repeated payloads/cadence while the other buffer is reused.
  Both driver tests are fake-controller evidence, not hardware qualification.

Playback and small-item source map:

| Requirement | Implementation / evidence |
| --- | --- |
| Stop per-block patch CRC | `en_refresh_patches` at locked edit boundary, `dx_loaded` invalidation, actual-renderer regression test |
| Smooth display | row reuse and alternating canvas strips in native DANCE, phase-preserving cadence, existing musical-clock animation, draw/audio telemetry |
| Sprite size jumps | upright outfit pose normalization before packing, bow preserved, original outfit untouched, source/companion generated together |
| BANK/LIBRARY layout | actual page text checked in 729 states, both tracks/all 11 bank pages, pending hints, long names, playing/stopped library |
| Sustain | classic chords/bass/singer note releases deferred under CC64; retrigger/pedal-up/panic and same MIDI-batch ordering tests |
| User-slot name editing | USER browse → EDIT, SELECT letter, K1 character, REC save/GLO cancel; storage failures retain the edit |
| Budgets/compatibility | unchanged native resource floors, both flash-ID assertions, unchanged MKS2 v4/v5 serialization |

Run all host tests and the pinned real-driver test. Native development CI builds
baseline/default/video/video-benchmark packages with the authentic toolchain and
pinned SDK, checks linked resources, boots default and video packages, runs
baseline/candidate complete-song profiles, runs JPEG encoding throughout the
arrangement with an end cutoff that freezes the final JPEG, and captures all requested bank pages
plus library states. Timing probes are identical in baseline and candidate.
Measurements marked incomplete or associated with another code SHA are not proof
for the current HEAD. Review the comparator's 10 ms DANCE spread target and
worst-callback/no-overrun checks against the underlying counters.

The initial native run at `22d3a68` passed build/budget/smoke checks. Later baseline
instrumentation initially tripped the strict HAL checker on a state-mask literal;
the state read was corrected to bit extraction, and the original checker is
retained. Later timed runs exposed real DANCE and encoder scheduling failures;
those diagnostics are retained separately. Read the latest successful manifest
instead of relying on a historical pass or incomplete measurement.

Remaining owner-dependent evidence:

- The latest Claude review text was requested but is not available in this chat
  or checkout. Apply its specific findings when supplied; do not invent them.
- CLUB STAB, DIGI GROWL and DIGI PLUCK trims require actual listening decisions;
  no approved trim values have been supplied and their factory bytes are retained.
- Real macOS/Windows/Linux webcam enumeration, sustained physical USB throughput
  and device audio deadlines are unverified. The owner forbids experimental
  installation before review/main integration; fake-controller and CPU-emulator
  results must retain their labels.
- GitHub rejected draft creation because Actions cannot create/approve PRs in
  this repository. Enable the repository setting or open the draft through an
  authenticated user. This branch must never be merged automatically.

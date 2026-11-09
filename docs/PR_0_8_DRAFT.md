DX7 playback in 0.7 recomputes patch checksums in each audio block, and the isolated USB-camera prototype does not recover cleanly from suspend or endpoint stalls. This draft moves patch invalidation to locked edits, reduces sprite decode work, stabilizes upright pose sizing, and completes the MIDI+UVC stream lifecycle without adding heap allocation or floating point to the audio path.

USB video now has corrected UVC camera-terminal control-bitmap geometry, immutable session-stamped JPEG publication, suspend/resume and endpoint-timeout recovery, deadline-based packet pacing, bounded decoded-row caching, and transmitted-frame/size/interval counters. It stays a separate 160x120 MJPEG/MIDI profile; USB-audio sharing is excluded because both profiles use EP4.

Also included: BANK/LIBRARY hint layouts, checked user-bank slot naming, and CC64 for classic chords, bass and singer notes. Song format, both flash IDs and resource limits are retained.

Validation:
- 52/52 host CTest checks pass, including 729 layout states and cached/reference JPEG byte equality across the wardrobe/effect fixtures.
- The real patched USB driver passes a 150-frame fake-controller transport run with 66–67 ms simulated frame intervals, exact reconstruction, FID/EOF checks and suspend/stall/restart cases. This is not OS or hardware enumeration evidence.
- Native development CI installs the authentic toolchain and pinned SDK, builds default/video/benchmark profiles with resource checks, and runs the pinned CPU emulator. The first UVC implementation commit passed all these steps; the latest run adds full-song baseline/candidate measurements and must be checked before approval.

[Native validation runs](https://github.com/MichaelAPerry/MIKU-FM1/actions/workflows/native-development.yml) · [validation record](https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/docs/VALIDATION.md) · [before/after host images and sprites](https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/docs/evidence/smooth-and-seen/README.md)

Review gates: native frame/callback measurements and emulator page artifacts; owner-approved listening trims for CLUB STAB/DIGI GROWL/DIGI PLUCK; Claude review before main integration. OS webcam enumeration, sustained physical USB throughput and real-device audio deadlines remain unverified. The owner does not permit experimental firmware on their FM-1 before Claude review and main integration. No main push, release tag, deployment or hardware access is authorized by this PR.

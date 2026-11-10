DX7 playback in 0.7 recomputes patch checksums in every audio block, and DANCE serializes rendering with LCD transfers. This draft moves patch invalidation to locked edits, overlaps rendering and transfer within the existing canvas, caches decoded body/effect rows, and protects musical-clock frame deadlines. Added outfits now keep consistent upright sprite heights.

USB video includes corrected UVC descriptors, generation-checked immutable JPEG ownership, suspend/resume and stalled-endpoint recovery, packet pacing, and completed-transfer counters. It stays a separate 160×120 MJPEG + MIDI profile because UVC and USB audio share EP4. Also included: BANK/LIBRARY layout fixes, checked user-bank name editing, and CC64 for classic chords/bass and the singer. MKS2 v4/v5 and both flash chips remain supported; no heap or libm was added to the audio path.

Verified source: `a6ad1ec278cc1c859d442052ea6a4a1f042266f0`. CPU-emulator measurements use timed 12 MHz LCD transfers and full-band DX7 playback:

- 56/56 host CTest checks pass, including 729 text-layout states and 720 cached/reference JPEG fixtures. Default/video/benchmark native builds, resource checks and default/video emulator smoke pass.
- DANCE draw mean 75.494 ms; frame intervals 98.508–102.669 ms. The unchanged 10 ms interval-spread check passes.
- Worst audio callback: STAGE 2271.792 µs and DANCE 2271.833 µs, both no higher than the instrumented 0.7 baseline; zero overruns.
- Full-song JPEG run: 639 frames over 30.532 s, 20.896 encoded fps, 4329–4822 bytes/JPEG. Mean/max frame time 47.593/110.371 ms; zero encoder failures/audio overruns. These are encoder measurements, not physical USB throughput.
- The real USB driver separately reconstructs 150 fresh frames, then 150 frames during an encoder pause (1 fresh/149 repeated), with 66–67 ms intervals, exact payloads and recovery checks. A separate compressed snapshot keeps the camera cadence independent of encoding; fresh/repeated counters remain distinct. This is protocol evidence, not OS enumeration.
- Main and USB swap ownership of two fixed compressed buffers between class frames. TIMER5 performs no JPEG-sized copy or encoding; physical USB-ISR cost remains unverified.

| Profile | Image bytes (≤581564) | Static free (≥16384) | Pool free (≥32768) |
| --- | ---: | ---: | ---: |
| miku-fm | 544,976 | 46,688 | 80,240 |
| miku-fm-video | 551,584 | 48,336 | 60,740 |

[Before/after emulator screenshots, all BANK pages and both tracks](https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/docs/evidence/smooth-and-seen/native-ci/README.md) · [validation and limitations](https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/docs/VALIDATION.md) · [review handoff](https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/docs/0_8_REVIEW.md)

Remaining owner gates: macOS/Windows/Linux webcam enumeration and physical sustained USB/audio timing are unverified. CLUB STAB, DIGI GROWL and DIGI PLUCK trims await listening decisions; their bytes are unchanged. Claude review and main integration must precede experimental installation on the owner's FM-1. No main push, release tag, deployment or device access occurs in this work.

# Performance events and computer review prototype (2026-11-08)

This implements a transport-independent event codec and a recorded-performance computer view. It does **not** implement
live USB MIDI sending, USB audio, UVC, a new synthesizer, or a USB stack rewrite. Dinghy and the native firmware remain
unchanged. The visual view has no JavaScript synthesis: its WAV and all vocal/mouth/performance states come from the actual
C firmware engine. This is an engineering review tool, not the final approved computer interface.

## Codec

`firmware/app/performance_wire.c,h` encode/decode a fixed 28-byte SysEx record, entirely integer, without I/O, libc,
allocation or shared buffers. The caller supplies a snapshot and a destination. Invalid bounds, capacity, length,
header, checksum or state values fail without mutating the destination. No transport is called from the audio ISR.

| Bytes | Content |
| --- | --- |
| 0–4 | F0, 7D (educational/non-commercial ID), ASCII M F, version 1 |
| 5–6 | 14-bit sequence, least significant 7-bit group first |
| 7–11 | 32-bit rendered sample clock, five 7-bit groups (last <=15) |
| 12–19 | mouth, vocal level, mora, section, row, step, playing, sprite |
| 20–25 | outfit, background, frame, palette, audio level, transient |
| 26 | XOR of bytes 1–25 |
| 27 | F7 |

All interior bytes are MIDI-safe (<128). Levels are 0–127; mouth is actual `fv_mouth`/published engine mouth 0–5;
mora 0–69; section 0–5; row 0–63; step 0–15; sprite 0–7. Outfit/background/frame/palette are extensible 7-bit IDs;
a receiver must reject or substitute unavailable visual IDs according to its own registry. Sequence and sample clock
wrap naturally; the future live receiver must handle wrap, missing packets, timestamp drift and stale/disconnected state.
The XOR detects single-bit corruption; it is not a cryptographic integrity check.

A future main-loop sender should snapshot ISR-published words, rate-limit to approximately 30–50 Hz, and enqueue
without blocking. At 50 Hz this payload uses 1400 bytes/s before USB-MIDI packet overhead. It must yield to note/control
traffic and discard obsolete visual snapshots on a full queue. USB-MIDI event packet framing and endpoint integration are
still required. No simultaneous USB audio/MIDI/UVC capability is implied. The codec itself does not allocate pool RAM.

## Reproduce

The parent build adds `performance_export` and `performance_wire_tests` targets. Direct host build:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I. tools/performance_export.c \
  firmware/app/performance_wire.c firmware/ui/performance.c \
  build/host/firmware/app/libmiku_engine.a build/host/firmware/song/libmiku_song.a \
  build/host/firmware/audio/fmband/libfmband.a build/host/libmiku_core.a -lm -o build/performance_export
build/performance_export build/evidence/performance.json build/evidence/performance.wav build/evidence/performance.syx
python3 -m http.server 8765 --bind 127.0.0.1
```

Open `http://127.0.0.1:8765/companion/`, load the JSON and matching WAV, and press play. The generated example is
12 seconds from demo row 3, 529200 stereo frames at 44.1 kHz, with 2068 block-end records. The `.syx` file contains those
same snapshots encoded through the codec. Export at audio-block frequency is for offline inspection; it is deliberately
not the proposed live USB send rate. The state clock follows the rendered WAV, and the browser selects the latest record
at or before the audio element's current sample position. State latency is bounded by one 256-frame block (~5.8 ms), plus
browser animation scheduling; this is not measured USB latency.

The companion references the existing original `site/media/dancer.png` atlas and preserves its PCL credit. No new
costume or character design is introduced and no existing artwork is overwritten. The original full-body frames follow
`PerfState.sprite`; lyric, musical section and the actual mouth code appear below the performer. Full-body mouth-pixel
compositing is unfinished: the mouth text is an engine-state indicator, not a claim of completed visual lip synchronization.
Audio-reactive tint is a procedural review effect pending owner approval. It does not select a final stage design.

## Validation

The codec test covers maximum clocks/sequence/fields, 7-bit payload, decode roundtrip, every single-byte one-bit corruption,
truncated messages, invalid mora, insufficient output capacity and transactional failures. Assertions explicitly remain
enabled in Release builds. Direct `cc -O2 -Wall -Wextra -Wpedantic` tests passed without warnings.
Host freestanding `-Os` object: 716 bytes text/constants, zero data and BSS. Native link/ISR/USB throughput measurements
are pending because the module is not yet wired into the native sender. The host exporter uses normal file I/O outside
firmware; it must never be compiled into the audio ISR.

`companion/test_review.py` (Playwright + Chromium, with the local server running) loads the real export files, seeks to
2 seconds, checks sample-clocked VERSE/mouth state, confirms the original sprite atlas loaded, rejects an invalid state
file and captures `build/evidence/companion.png`. This browser check passed with no page errors; the screenshot was
visually inspected. The managed browser blocks file URLs, so HTTP loopback was used. The test requires generated evidence
and is intentionally separate from normal CTest. No website was deployed.

Next concrete task: connect a bounded, rate-limited native main-loop sender to Dinghy's existing MIDI endpoint and test
computer receive/disconnect behavior with emulator endpoint support or authorized physical hardware. USB audio input
routing remains a separate task described in `NEXTGEN_USB_RESEARCH.md`.

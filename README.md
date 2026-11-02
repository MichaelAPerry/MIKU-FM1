# MIKU-FM1

An experimental **four-track, retro virtual-idol-inspired songwriting workstation** for the M-VAVE FM-1.

**Status: host-only functional audio prototype.** Not a flashable FM-1 firmware.

## Build and render an editable eight-bar song

Requires a C11 compiler and CMake 3.16+.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/miku_demo demo.wav demo.mik
```

The demo composes four patterns in C: vowel-melody, three-note FM-like chords, FM bass, and kick/snare/hat rhythm. It serializes the song to versioned CRC-checked `MIK1` bytes, decodes back to editable notes, and **renders from the decoded song** into 16-bit mono 22,050 Hz WAV.

`demo.mik` is a structured editable event file, **not** rendered audio. The demo is fixed eight bars (120 BPM); the UI, arrangement chaining, Japanese consonants, lyrics editing, and physical instrument support remain future work.

## Architecture

- `shared/song.h` — editable fixed-capacity 4-track event model
- `shared/song_io.c` — versioned little-endian binary codec and CRC32
- `firmware/sequencer/` — precomputed sorted sample-time event queues
- `firmware/audio/vocal/` — independently written five-vowel formant synthesizer
- `firmware/audio/band/` — original polyphonic chord FM, monophonic FM bass, electronic drums
- `tools/demo.c` — independent host harness rendering an eight-bar song
- `tests/` — unit, serialization, and host-demo smoke tests

The current instruments use simplified synthesis and no effects, and there is no claim of measured audio or CPU quality on FM-1 hardware. See `docs/RESEARCH.md`, `docs/SONG_FORMAT.md`.

MIT licensed original code. MIKU-FM is an unofficial working title and has no relationship to Crypton, Yamaha or VOCALOID. No voicebanks or proprietary assets are included.

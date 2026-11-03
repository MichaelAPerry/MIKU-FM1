# Miku-FM

An experimental **four-track, retro virtual-idol-inspired songwriting workstation** for the M-VAVE FM-1.

**Status: native FM-1 firmware (Dinghy) with an editable 16-step melody; emulator-tested; not yet tested on hardware.** The host engine below is a separate MIT-licensed prototype.

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

Host-engine code is MIT licensed; the firmware package is GPL-3.0-only (see Licence). MIKU-FM is an unofficial working title and has no relationship to Crypton, Yamaha or VOCALOID. No voicebanks or proprietary assets are included.

## Other firmwares' data

Miku-FM does not read other firmwares' projects or samples. It stores its melody in Dinghy's settings sector (FM-1 flash 0xFC000), which other Felucca-family firmwares also use, so saving there replaces their settings. Back up first.

## Licence

The firmware package (`native/`, built on [Dinghy](https://github.com/zvenson/dinghy)) is GPL-3.0-only; see `LICENSE`. Files marked `SPDX-License-Identifier: MIT` (the host engine) remain MIT; see `LICENSES/MIT.txt`.

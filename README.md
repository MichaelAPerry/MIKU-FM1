# MIKU-FM1

An experimental, original **four-track songwriting workstation** targeting the M-VAVE FM-1, with a retro digital-idol-inspired synthetic singer.

**Current status: host prototype only.** No FM-1 firmware image, device validation, or hardware audio integration exists yet.

## Host prototype

Requires a C11 compiler, CMake 3.16+, and Python 3.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/miku_demo demo.wav
```

The demo renders a manually authored eight-note vowel melody as 16-bit mono PCM WAV using a deterministic tick-based sequencer and a three-formant synthetic voice. No proprietary voicebank, artwork, or external assets are included.

## Modules

- `shared/song.h`: fixed-capacity editable note and pattern data, four track roles
- `firmware/sequencer/`: musical time to sample time, independent of audio block size
- `firmware/audio/vocal/`: original lightweight source-filter vowel synthesizer
- `tools/demo.c`: command-line song-to-WAV host harness
- `tests/`: C unit tests and WAV smoke testing

The host engine intentionally makes no assumptions about verified FM-1 storage, processor capacity, display driver, or firmware layout. See `docs/RESEARCH.md` and `docs/ROADMAP.md`.

## Legal

Project-original code is MIT licensed. MIKU-FM is a working title and is not affiliated with Crypton Future Media, Yamaha, or VOCALOID. No third-party DSP or artwork was copied.

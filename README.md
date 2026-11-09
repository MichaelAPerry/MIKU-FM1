# Miku-FM

**A singing songwriter firmware for the M-VAVE FM-1.** A tiny pixel singer sings your songs in Japanese, consonants and
all, over FM chords, bass and drums. Shape her voice, write patterns or let her generate a whole song, and watch her dance.

<p>
<img src="site/media/screens/stage.png" width="200" alt="STAGE: her face and the syllable dial">
<img src="site/media/screens/voice.png" width="200" alt="STAGE: a knob's picture (reverb)">
<img src="site/media/screens/dance.png" width="200" alt="DANCE: the singer dancing">
<img src="site/media/screens/song.png" width="200" alt="SONG: the arrangement">
<img src="site/media/screens/pattern.png" width="200" alt="PATTERN: the piano roll">
<img src="site/media/screens/gen.png" width="200" alt="GEN: the song generator">
</p>

**Current release: [Miku-FM 0.7](https://github.com/MichaelAPerry/MIKU-FM1/releases/tag/v0.7)**: a six-operator DX7 engine with ten
original voices, DX7 cartridge import, MIDI keyboard input, a 10-song library, lyrics editing and 12 stage outfits.

**Try it in your browser, watch the trailer, install it:** https://michaelaperry.github.io/MIKU-FM1/ (the emulator runs the
real firmware package; the installer works in Chrome or Edge over USB).

### Quick start

1. Open the [website](https://michaelaperry.github.io/MIKU-FM1/), connect the FM-1 by USB and press **Install 0.7** (Chrome or Edge).
2. Press **PLAY**: she sings. **HOME** steps through STAGE, DANCE, SONG, PATTERN, MIXER, GEN, LYRICS, LOOK, LIBRARY and DX7 BANK.
3. Load your own DX7 sounds: open the [DX7 bank tool](https://michaelaperry.github.io/MIKU-FM1/bank.html), pick a `.syx`, send it,
   and press **REC** on the FM-1 within 10 seconds. Sending a cartridge from Dexed over USB works too.
4. The [guide](https://michaelaperry.github.io/MIKU-FM1/guide.html) explains every page and control.

- **Her voice:** a formant voice synthesised live (no samples): 73 Japanese mora selections with their consonants, five characters
  from DEEP to TINY, languages (Japanese, animal babble, scat, vowels), tension, fry, whisper, a harmony voice, vowels on
  the beat; chorus, delay, reverb, drive, tremolo. Every knob shows a picture of what it does.
- **The song:** four tracks (vocal, chords, bass, drums), 16 patterns per track, 64 rows with sections, mixer, swing,
  undo. **GEN** writes a whole song in nine styles: HYPERPOP, FUTURE BASS, JERSEY CLUB, DIGICORE and IDOL POP (new in 0.7, each with
  its own DX7 chords and bass), and POP, BALLAD, DANCE and CHIP, with track locks, new phrases, energy and complexity.
- **DX7 voices (0.7):** a six-operator FM engine (all 32 algorithms, feedback, envelopes, LFO, pitch bend, sustain, bass glide)
  with ten original voices, every operator editable on the device and saved inside the song. A 32-voice user bank: import a DX7
  cartridge or a single voice from Dexed or the website's [DX7 bank tool](https://michaelaperry.github.io/MIKU-FM1/bank.html) over
  USB, store your own patches, export the bank as a .syx.
- **MIDI keyboard (0.7):** USB or TRS notes play the current track; sustain pedal and pitch bend work on DX7 voices.
- **Songs and stage (0.7):** 10 song slots with a LIBRARY page and four Japanese folk songs ready in slots 1 to 4; a LYRICS page to
  edit the sung syllables; per-song LOOK with 12 outfits, backgrounds, frames and stage effects. Her face mouths every vowel on
  STAGE; on DANCE she dances to the song. USB video output is not in 0.7.
- **Guide:** [docs/MANUAL.md](docs/MANUAL.md) (also the website's [guide page](https://michaelaperry.github.io/MIKU-FM1/guide.html)),
  every control: [docs/CONTROLS.md](docs/CONTROLS.md), what is measured: [docs/VALIDATION.md](docs/VALIDATION.md).
- **Status:** 0.7 is a release built and attested on GitHub and booted in the fm1-emu emulator. Earlier versions run on real
  FM-1s. Not yet tried on a real FM-1: DX7 SysEx over USB with Dexed and the bank tool, and MIDI keyboard input.
- **Other firmwares' data:** Miku-FM does not read other firmwares' projects or samples (see below). Back up first.
- **Licence:** code GPL-3.0-only; the singer is Hatsune Miku fan art under Crypton's Piapro Character License (free and
  non-commercial only): [LICENSING.md](LICENSING.md). The ten DX7 voices are original project data.

## Building

The firmware is `native/dinghy.c` on [Dinghy](https://github.com/zvenson/dinghy), with the modules in `firmware/`: the song model
and MKS2 format (`firmware/song/`), the voice (`firmware/audio/fvoice/`), the classic FM band (`firmware/audio/fmband/`), the DX7
renderer (`firmware/audio/dx7/`) and the engine that runs in the audio interrupt (`firmware/app/engine.c`).

Host build and tests (C11, CMake 3.16+, Python 3 with `tools/requirements-dev.txt`):

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
```

Firmware package: the JieLi toolchain and AC79 SDK (see Dinghy's README), then `bash native/build.sh`. Releases are built,
tested, attested and booted in the emulator by GitHub Actions when a `vX.Y` release is published.
Song format: [docs/SONG_FORMAT.md](docs/SONG_FORMAT.md). Measurements and test levels: [docs/VALIDATION.md](docs/VALIDATION.md).

## Other firmwares' data

Miku-FM does not read other firmwares' projects or samples. It stores its songs in Dinghy's 10 slots (FM-1 flash 0x97000...) and a small settings struct in the settings sector (0xFC000), which other Felucca-family firmwares also use, so saving there replaces their data. Back up first.

## Licence

The code is GPL-3.0-only (`LICENSE`; the firmware is built on [Dinghy](https://github.com/zvenson/dinghy)); files marked
`SPDX-License-Identifier: MIT` are MIT (`LICENSES/MIT.txt`). The singer's art is **fan art of Hatsune Miku under Crypton's
Piapro Character License**: free and non-commercial only, not under the GPL. Details, the credit and the list of art
files: [LICENSING.md](LICENSING.md).

この作品はピアプロ・キャラクター・ライセンスに基づいてクリプトン・フューチャー・メディア株式会社のキャラクター「初音ミク」を描いたものです。
© Crypton Future Media, INC. www.piapro.net — Miku-FM is an unofficial fan project, not associated with Crypton Future
Media, Yamaha or M-VAVE. 「初音ミク」 is a registered trademark of Crypton Future Media, INC.; VOCALOID of Yamaha Corporation.

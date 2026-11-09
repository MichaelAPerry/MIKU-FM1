# Native integration review evidence

These screenshots come from the actual native firmware in fm1-emu, or the
explicitly labelled computer companion. They are not concept images presented
as working features. `paired-outfit-review.png` is a contact sheet exported from
the actual packed firmware pixel rows, pairing each costume's neutral portrait
and first dance pose. All eight dance and twelve face frames are in the source
atlases and covered by decoding/dirty-rectangle tests.

Character artwork, including these derivative screenshots, remains PCL fan art:
この作品はピアプロ・キャラクター・ライセンスに基づいてクリプトン・フューチャー・メディア株式会社の
キャラクター「初音ミク」を描いたものです。 / This work depicts Crypton Future
Media, INC.'s character “Hatsune Miku” under the Piapro Character License,
https://piapro.jp/license/pcl/summary . © Crypton Future Media, INC.
www.piapro.net. Free/noncommercial only. See `LICENSING.md` at repository root.

## Files

- `song-library.png`: ten-slot library's first four preloaded entries.
- `slot-ten-reboot.png`: persisted slot 10 loaded and playing after a fresh boot.
- `native-lyrics.png`: editing real vocal units/durations/pitches.
- `native-gen.png`: native Hyperpop generation and energy control.
- `native-witch-portrait.png`, `native-fireworks.png`: matching native portrait,
  body, background, corners and fireworks.
- `companion-wardrobe-effects.png`: browser with matching body/vocal portrait and
  shared firmware pixel assets. This is recorded real-engine state, not live USB.
- `stage-effects-review.gif`: shared renderer's four animation phases.
- `video-measurements.json`: offline native target JPEG benchmark during real
  vocal/band playback with the witch outfit and effects. This is not delivered
  USB FPS, physical timing, or a test of combined UAC/MIDI/UVC.
- `wardrobe-measurements.json`: all eleven additional packed outfit costs.
- `slot-ten-save.json`: actual native slot-10 address, length and CRC extraction.
- `song-measurements.json`: uncompressed WAV peaks, RMS, hashes and full-scale
  checks. `.mp3` files in `songs/` are compressed listening copies of those full
  first-verse arrangements; measurements refer to WAVs, not MP3 decoding.
- `songs/*.mks`: exactly the editable native factory songs, not sampled audio.

Reproduction commands, toolchain pins, input scripts and limitations are in
`CLAUDE_HANDOFF.md` and `docs/NEXTGEN_NATIVE_INTEGRATION.md`. The original WAVs,
full emulator flash images and build logs remain ignored build artifacts.

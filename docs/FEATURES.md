# Features

Status words: **emulator** = checked in fm1-emu on the real package; **hardware** = checked on a physical FM-1.

| Feature | Status |
|---|---|
| 16-step monophonic melody, note per key, rests | emulator |
| Five vowel colours per step (harmonic recipes, not formants) | emulator |
| Tempo 40–240 BPM | emulator |
| Play from cursor, pause, stop | emulator |
| Save melody and tempo to flash, reload at power-on | emulator (flash dump and restore) |
| Own identity FM-1_39XXYYZZ, own store magic | built |
| Dinghy platform: USB/TRS MIDI, USB audio, USB rescue, update loader | from Dinghy; not checked with Miku-FM |

Host engine (MIT, `shared/`, `firmware/`; not on the device yet): four tracks (vocal formant voice, FM chords, FM
bass, drums), 96 PPQN event queue, MIK1 song file with CRC, WAV render. See [ROADMAP](ROADMAP.md).

# Features

Status words: **emulator** = checked in fm1-emu on the real package; **hardware** = checked on a physical FM-1.

| Feature | Status |
|---|---|
| 16-step monophonic melody (eighth notes), note per key, rests | emulator |
| Integer formant voice (fvoice): five vowels, vibrato, portamento, breath, brightness; no float | emulator |
| Live keys with legato, KNOB 1-4 (vibrato, portamento, breath, brightness), saved with the melody | emulator |
| STAGE page: singer face (mouth follows the voice, blink), syllable, step strip, CPU% | emulator |
| EDIT page: step grid and knob values (HOME switches) | emulator |
| Tempo 40–240 BPM | emulator |
| Play from cursor, pause, stop | emulator |
| Save melody and tempo to flash, reload at power-on | emulator (flash dump and restore) |
| Own identity FM-1_39XXYYZZ, own store magic | built |
| Dinghy platform: USB/TRS MIDI, USB audio, USB rescue, update loader | from Dinghy; not checked with Miku-FM |

Host engine (MIT, `shared/`, `firmware/`; not on the device yet): four tracks (vocal formant voice, FM chords, FM
bass, drums), 96 PPQN event queue, MIK1 song file with CRC, WAV render. See [ROADMAP](ROADMAP.md).

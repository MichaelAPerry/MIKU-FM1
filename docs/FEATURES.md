# Features

Status words: **emulator** = checked in fm1-emu on the real package; **hardware** = checked on a physical FM-1.

| Feature | Status |
|---|---|
| Song model: 4 tracks, 16 patterns of 16 steps each, 64-row arrangement with sections, swing, undo (8 steps) | emulator |
| Sequencer in the audio interrupt: sample-accurate, tempo change during play | emulator |
| Integer formant voice (fvoice): five vowels, vibrato, portamento, breath, brightness; no float | emulator |
| Vocal sings moras (70-entry kana table) with their consonants: plosives, fricatives, affricates, nasals, y/w glides, r flap, voiced and unvoiced; moraic n is a hum (consonants are synthesised, no samples; not listened to on hardware) | emulator |
| FM chords (8 presets, 4 voices, block/arp/strum modes), FM bass (4 presets, accent, slide), drum kit (6 sounds, 2 kits) | emulator |
| Mixer: level, pan, preset, mute, solo per track; swing, tempo, master | emulator |
| STAGE page, the voice's home: singer face, syllable wheel (SELECT), languages (JAPANESE, ANIMAL babble, SCAT, VOWELS), character DEEP..TINY, knob banks VOICE / FX / ENV / LFO, saved with the song | emulator |
| Voice effects: chorus, tempo-synced delay, reverb, drive, tremolo (integer, in the audio interrupt) | emulator |
| Hold-a-button layers on STAGE: SOURCE (tension, fry, whisper, formant), FX DEEP, ARTICULATION (incl. vowels on the beat), HARMONY (a second voice at an interval) | emulator |
| DANCE page: the singer full-body (8 pixel-art frames at 3x) dancing on the beat, singing, arms up in the chorus, bowing at the end | emulator |
| SONG, PATTERN (piano roll, drum grid), MIXER pages; one control map ([CONTROLS](CONTROLS.md)) | emulator |
| Live keys play the current track's instrument; keys write steps on PATTERN | emulator |
| Copy / paste steps, duplicate pattern, insert / duplicate / delete rows, undo | emulator |
| 8 song slots (MKS2, CRC-checked), load / new / demo with an unsaved-changes guard | emulator (flash dump and restore) |
| Demo song 「おとのはこ」 (intro, verse, chorus, outro) | emulator |
| GEN page: song generator (4 styles, key, major/minor, seed, track locks, new phrase, keep/drop a take); writes ordinary editable patterns | emulator |
| Host render of the demo through the same engine code (`song_render`) | host |
| Own identity FM-1_39XXYYZZ, own store magic | built |
| Dinghy platform: USB/TRS MIDI, USB audio, USB rescue, update loader | from Dinghy; not checked with Miku-FM |

Not there yet: song rename on the device, per-song pan, lyrics editing, MIDI sync, WAV export from the device.

Older host prototype (MIT, `shared/`, `firmware/sequencer`, `firmware/audio/band`, `firmware/audio/vocal`): 96 PPQN event queue,
MIK1 song file; superseded by the MKS2 song format on the device. See [ROADMAP](ROADMAP.md).

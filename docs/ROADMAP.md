# Progress and technical debt

Implemented: host C11 synth; 96 PPQN event queue; four sequenced track identities; simple FM-like polyphonic chords, bass, kick/snare/hat; eight-bar WAV from re-decoded MIK1 editable notes; little-endian versioned CRC32 song codec; GitHub CI tests.

Limitations in current slice: 128-note maximum per pattern; the 8-bar demo uses 32 vocal, 24 chord, 32 bass and 64 drum notes. Drum engine currently single-voice (overlapping hats and kicks may choke), and the chord voice allocator supports four simultaneous notes. The vocal synth has abrupt releases and lacks consonants. No pattern chaining, song sections, external disk APIs, track solo, live editor, transport pause, MIDI or device integration.

Next:
1. Polyphonic percussion and per-note ADSR releases; fix overlapping vocal note-off identity.
2. Pattern/section arrangement references, track editing commands, undo and actual multi-song persistence.
3. Introduce phoneme/consonant envelope and syllable editor.
4. Add browser editor to author MIK1 files and preview rendered events.
5. Measure on JieLi FM-1 SDK, implement hardware callback/HAL safely, then run emulator/hardware verification.

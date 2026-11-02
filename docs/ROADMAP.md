# Engineering backlog

Completed: C11 host build, fixed-capacity song and note representation, 96 PPQN clock, precomputed sorted event queue, five vowels, song binary codec with validation and CRC32, host WAV demo, basic tests.

Next critical milestones:
1. Add arrangement references and editing API with pattern duplication, removal and track locks.
2. Four-track audio playback with basic FM chords, bass and synthesized drums.
3. Consonant events, smooth formant transitions, anti-aliased excitation and envelope/release.
4. Build an eight-bar composed four-track song that roundtrips through the file codec.
5. Profile CPU/RAM/flash with actual FM-1 SDK and create platform adapter.
6. Browser composer and matching codec importer/exporter.

Not working: actual FM-1 firmware, file IO on device, Japanese lyrics beyond vowel index, full arranger, generative tools, UI, hardware simulator.

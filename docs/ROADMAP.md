# Engineering backlog

Completed in first host slice: fixed-capacity pattern and note representation, sample-time musical transport, five vowels, deterministic demo WAV generator, basic host tests.

Next:
1. Replace current O(N^2) transport scan with precomputed bounded event queue and explicit overlapping-note policy.
2. Add versioned binary song serialization with header, CRC and rigorous bounds checks, plus multi-pattern arrangement.
3. Integrate consonant event/timing model and smooth formant morphing; add anti-aliased excitation and envelope/release.
4. Add chords, bass, drums and a four-track mix engine to render an editable eight-bar composition.
5. Profile CPU/RAM/flash against measured FM-1 toolchain and define board adapter.
6. Implement local browser composer and matching import/export tests.
7. Add hardware simulator and physical verification once available.

**Not yet implemented:** device firmware, saving/reloading, lyrics/consonants, chords/bass/drums, generators, UI, or browser composer. No claims of audio quality or performance on the hardware are made.

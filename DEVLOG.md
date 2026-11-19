# DEVLOG

Who did what, with which model. Newest first.

## 2026-11-19: fvoice English (0.9 step 3), model = claude
- `tools/gen_fvoice_tables.py`: 12 English shapes (IH EH AE AH AX AO UH UW ER, L, English R, NG); F1/F2 from CHIMERA's
  calibrated female vowels, F3 from Hillenbrand 1995, scaled by 1.08. 8 English onset classes, 21 coda classes, a TH
  noise band, phoneme -> class/shape/viseme maps. Phase rows widened 5 -> 6 bytes (full shape byte).
- `fvoice.c/h`: `fv_note_on_syl` (onset clusters queued), diphthong glide (150 ms or at the coda), `fv_note_off_syl`
  (coda clusters, release from the coda's held end), `fv_syl_onset_ms`, `fv_syl_coda_ms`, `fv_viseme`.
- Japanese output is bit-identical to 0.8 (25 s randomized render, same FNV hash with the old and new engine).
- New: `tests/test_fvoice_english.c` (all vowels x consonants bounded/sounding/ending, clusters, codas, visemes,
  determinism, legato), `tools/english_voice_render`. Host ctest: 57/57.
- Baseline native build reproduced locally: 544980 B (= v0.8 release), 36584 B free.

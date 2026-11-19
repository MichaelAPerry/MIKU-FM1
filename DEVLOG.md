# DEVLOG

Who did what, with which model. Newest first.

## 2026-11-19: English front end (0.9 step 2), model = codex
- Ported CHIMERA's English front end (via Shan-Sherif's C port, MIT) to integer C: `firmware/song/english.[ch]`,
  packed 477-word dictionary `english_dict.h` (CMUdict-derived), FM-1 letter-to-sound rules, weak forms, numbers,
  maximal-onset syllabification. Golden test vs CHIMERA's own C front end: 477 dictionary + 535 fallback words + 30
  sentences (`tools/english_golden.c`, `firmware/song/tests/english_golden.txt`), 12 hand-checked splits, buffer limits;
  ASan/UBSan clean. Host -Os size 12.8 KB (5.8 KB dictionary).
- Divergences: schwa and dark L fold into AH and L (fvoice maps unstressed AH to AX); flaps keep T/D.
- Reviewed and merged by claude (the sandbox could not commit in the worktree).

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

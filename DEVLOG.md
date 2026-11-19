# DEVLOG

Who did what, with which model. Newest first.

## 2026-11-19 — Shared mouth overlay approval sheet, model = codex

- Built 12 visemes from the original portrait pixels in a 17×10 box with nine RGB565 palette entries,
  including transparency. The generator detects mouth changes inside hand-checked search windows and
  emits 12 anchors in `WD_OUTFITS` order (ORIGINAL plus the 11 outfit sheets present in this worktree).
  A sparse table replaces each neutral lip mark with its outfit's skin index before compositing.
- The compact atlas arrays use 692 bytes. The full data accounts for 861 bytes on the 32-bit target,
  including the palette, anchors, neutral lip table, and atlas descriptor. The generated header is
  4,108 bytes. The indexed source sheet is 3,705 bytes and the 2× approval sheet is 310,112 bytes.
- Reviewed each combined face at 2× and shortened the tall open vowels to clear the smallest chins.
  The host test uses `spr_decode_row` to compare all 12 frames against both scales of the indexed PNG.
- Owner review is open. Does FV need a stronger lower-lip bite to distinguish it from EE, and should OO
  be more pursed to distinguish it from UH? Does R read as a forward lip shape at 2×? Should REST add
  the shared mouth line to Encore and Prism, whose neutral source faces have no visible lip mark?
- Merged and wired in by claude: `face_draw` draws NEUTRAL + the mouth picture for English (face id FACE_MOUTH|viseme),
  dirty box = the mouth rect, m/b/p closures latched 70 ms like the Japanese ones. Japanese keeps its own frames until
  the owner approves the sheet. Tests: every outfit x viseme pixel-exact (test_sprite_decode), English face clock.
  Native: 563244 B (18320 free). Emulator smoke passes.


## 2026-11-19: MKS2 v6 and English in the engine (0.9 step 5), model = claude
- `song.h/.c/song_io.c`: per-song table of up to 120 English syllables (phonemes + spelling); VOCAL mora bytes
  128..247 name them, 248..255 are the engine's live slots. v6 = v5 layout + table, written only when a step uses
  English (songs without English still write v4/v5 byte-for-byte). `sg_syl_add/sg_syl/sg_syl_compact`.
  Decoder validates phoneme codes, spelling, references and length; the encoder refuses dangling references.
- `engine.c`: English steps sing via `fv_note_on_syl` (harmony too), HOLD keeps the vowel, legato carries the
  previous coda into the next onset, on-beat timing uses the syllable's onset/coda length, VOWELS strips consonants,
  live slots (`en_set_live`), viseme published (`en.vis`).
- `firmware/song/tests/test_song_english.c`. Host ctest 59/59.

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

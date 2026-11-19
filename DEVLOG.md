# DEVLOG

Who did what, with which model. Newest first.

## 2026-11-19: merged the mouth-frame retirement, model = claude
- Reviewed codex's change (Japanese lip-sync on the shared overlay; frames 2..7 of every face set = NEUTRAL at pack
  time so the row dedupe stores nothing for them; identity test for every other frame). Native build: 512340 B used,
  69224 B free (was 3596). Host ctest 65/65, emulator smoke ok, Japanese lip-sync checked in fm1-emu
  (`docs/evidence/mouths/emu_japanese_lipsync.png`). Owner note: the old wavy closed mouth (N) is now the plain MBP line.

## 2026-11-19 — Japanese shared mouth overlay, model = codex

- STAGE now maps Japanese A/I/U/E/O to AH/EE/UH/EH/OH on the approved shared overlay. The 70 ms
  closure latch and quiet-vowel closure use MBP. The companion review exports the same six
  composites from the firmware atlas. USB JPEG rendering uses DANCE frames only.
- Both compact packers alias face slots 2–7 to NEUTRAL and zero their dirty rectangles. The original
  art and raw header are intact. The previous native symbol map contains the compact face data
  and no raw face arrays. The mouth generator still reads the original expression art.
- The identity test compares decoded retained portraits and DANCE frames against the pre-change
  headers, and checks palette bytes, retained dirty rectangles, and packed DANCE arrays.
  The table counts generated face arrays, including row IDs, offsets, payload, face palette,
  and dirty rectangles. The native build is pending owner verification.
- The [Japanese before/after sheet](docs/evidence/mouths/japanese_before_after.png) shows ORIGINAL,
  ENCORE, PRISM, and NEON PUNK at 2×, including the closed mouth.

| Face set | Before (B) | After (B) | Saved (B) |
| --- | ---: | ---: | ---: |
| Original | 18,521 | 14,375 | 4,146 |
| Encore | 19,021 | 13,789 | 5,232 |
| Off Duty | 19,037 | 13,714 | 5,323 |
| Retro Wave | 20,714 | 14,829 | 5,885 |
| Prism | 18,535 | 13,252 | 5,283 |
| Neon Punk | 19,963 | 14,481 | 5,482 |
| City Beat | 17,653 | 11,932 | 5,721 |
| Signal Suit | 18,890 | 13,834 | 5,056 |
| Summer Beat | 17,006 | 11,283 | 5,723 |
| Snow Glow | 17,730 | 11,949 | 5,781 |
| Festival | 19,146 | 13,119 | 6,027 |
| Moon Circuit | 18,923 | 12,974 | 5,949 |
| **Total** | **225,139** | **159,531** | **65,608** |

## 2026-11-19: PHRASE page, USB glue, engine test (0.9 step 6), model = claude
- `native/phrase_ui.inc`: the PHRASE page (bank of 64 at PHRASES_AT, live syllable per key/MIDI note through the
  engine's live slots, REC places into up to 4 empty patterns with exact undo snapshots, typing editor: A-Z + space
  on the keys, ALGORITHM for digits/punctuation/romaji marks, K1 EN/JP, EDIT compiles and stores while stopped).
  Miku SysEx 0x7D glue: phrase get/put/list on flash, song upload held until REC; buffers in the pool.
- Merged codex's phrase/mkx modules (resolved an english.c overlap in favour of codex's initializer).
- `tests/test_english_engine.c`: song syllables, codas, legato coda carry, live slots, harmony, visemes.
- CONTROLS.md: PHRASE section (drafted by the local model qwen3.5:9b, corrected by claude).
- Native: 577968 B of 581564 (3596 free); static RAM 38544 free; pool 59412 free. Host ctest 64/64. Emulator smoke ok.

## 2026-11-19 — Phrase records and Miku SysEx, model = codex

- Added the 1024-byte MPH1 codec, English and romaji compilation, and transactional VOCAL placement. English streams
  through the sung front end to keep compiler scratch small. Japanese rests, holds and separate attacks use spelling
  markers within the existing layout.
- Added 7-in-8 MKX framing and command codecs, a callback dispatcher, and ordered staging and validation of MKS2
  uploads. Added the browser codec and checked its bytes against C-generated vectors.
- Host ctest passed 62/62. JieLi pi32v2 `-Os` object sizes from `size` were 2,785 B for `phrase.o`, 1,947 B for
  `mkx.o`, and 12,500 B for `english.o` (+118 B). The three files compiled together with warnings treated as errors
  and had no libc references.

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

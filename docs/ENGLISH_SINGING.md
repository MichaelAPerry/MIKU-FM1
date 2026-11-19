# English singing, the phrase bank and the web editor: design (0.9)

Status: **approved direction, 2026-11-19**. Branch `feature/english-singing`. This document is the plan of record;
change it when a decision changes.

## Goal

Miku sings English as well as Japanese, live and in songs, in her own formant voice, on one core, over her band.
Words come from a phrase bank on the FM-1, typed on the device or prepared in an offline web editor and sent over USB.
Any note (keys, MIDI from a controller or a DAW, the song) can sing the next syllable of a phrase: a Vocaloid-style
live mode.

Not in this plan: CHIMERA's airway physics on the FM-1 (it needs the second core and ~300 KB), cutting outfits, the
X-ray head view, and USB audio streaming from the web app. See "Later" at the end.

## Constraints (measured, not assumed)

| Resource | Fact | Source |
|---|---|---|
| App flash | 0x04000–0x93000, about 572 KB; 0.8 uses about 545 KB, **~27 KB free** | Dinghy `fm1_flash_map.h`, v0.8 `app.bin` |
| Data flash | Songs 0x97000–0xBF000 (10 × 16 KB); **0xBF000–0xE0000 (~132 KB) free** | Dinghy `store.h`, `tools/patch_dinghy_slots.py` |
| CPU | One core; 256-sample / 5.8 ms audio callback; emulator worst case 2.7 ms | `docs/HARDWARE_AND_DX7_REVIEW.md` |
| Arithmetic | **No float, no double, no libgcc** (no FPU) | Dinghy `docs/RULES.md` |
| Art | 12 outfits = 339 KB, each with its own 12-frame face set | `firmware/ui/wardrobe_sprites.h` |

The 27 KB of app flash is the tightest budget. Everything new must fit in it, or pay for itself (see "Flash plan").

## Architecture

```
 text ("hello world")                       web editor (offline HTML)
   │                                          full CMUdict, piano roll, preview (WASM of the same C)
   ▼                                          │  USB SysEx (0x7D)
 English front end  (firmware/song/english.c) ◄┘
   CHIMERA's G2P and syllabification, integer C, small built-in dictionary + letter-to-sound
   ▼
 syllables: onset (≤3) + vowel (+ glide) + coda (≤4) phonemes, spelling for display
   ▼                                   ▼
 phrase bank (data flash)          song: VOCAL steps reference a per-song syllable table (MKS2 v6)
   ▼                                   ▼
 live: next syllable per note-on   sequencer: syllable per step, onset early, coda before note end
   ▼
 fvoice: English vowel shapes, consonant classes, onset queue, coda on release
   ▼
 mouth overlay (visemes shared by all outfits) + face
```

### 1. English front end: `firmware/song/english.c`

A port of the **text-to-phoneme and syllable parts** of CHIMERA's English front end, from Shan-Sherif's C port
(`engine/ch_speech.c`, MIT; CHIMERA by Jameson Hodge, MIT). Only these parts:

- the dictionary lookup with a pluggable lookup function, the built-in common-word list (`ch_dict_data.h`, ~480 words);
- letter-to-sound rules, including the FM-1 device additions;
- ARPAbet parsing, stress, weak forms of function words, numbers spelled out;
- English syllabification (onset / nucleus / coda, maximal onset).

**Left out:** CHIMERA's Klatt durations, gestural planner, intonation and articulator targets (all floating point and
all aimed at the airway model). Timing comes from the note (live) or the step grid (songs).

Rules: integer only, no allocation, fixed buffers, no libc beyond what the firmware already has. The output is our own
phoneme enum (ARPAbet's 39 phonemes), not IPA strings. Host tests compare its phonemes and syllable splits against
CHIMERA's port for a word list (CHIMERA's front end compiled on the host, floats allowed there) and against CMUdict.
Licence: MIT files keep their header and notice; LICENSING.md and the credits name CHIMERA and the FM-1 port.

### 2. English in fvoice

The engine stays as it is (six parallel resonators, table-driven consonant phases). Additions, all in
`tools/gen_fvoice_tables.py` and `fvoice.c`:

- **Vowel shapes.** English monophthongs AA AE AH AO EH ER IH IY UH UW. IY, EH, AA, AO reuse or tune the Japanese I E A
  O. UW is new (rounded, unlike Japanese u). F1/F2 come from CHIMERA's calibrated vowel table (female body); F3+ from the
  published Hillenbrand values, scaled for Miku's character like the existing table. ER is a low-F3 shape.
- **Diphthongs** AW AY EY OW OY: two shapes, the glide to the second one starting at a fixed share of the expected note
  length (live: a fixed time, ~150 ms, or the coda, whichever comes first).
- **Consonant classes** added to the existing 21: TH, DH, V, ZH, NG, English R (approximant, low F3, not the Japanese
  flap), L (lateral shape), plus English W/Y using the new shapes. JH, CH, SH, S, Z, F, HH, K, G, T, D, P, B, M, N reuse
  the existing classes.
- **Onset clusters:** a queue of up to 3 consonant classes before the vowel ("str", "spl"); each runs its phases, then
  the next. An /s/ before a stop removes the stop's aspiration (English rule).
- **Codas:** `fv_note_off_coda()`: the vowel closes into up to 4 consonants (a stop's closure and release burst, a
  fricative's hiss, a nasal hum, L/R glides), then the release envelope. A sequencer starts the coda
  `fv_coda_ms()` before the note end so the word ends on time. Live, the coda plays on key release.
- **API**, existing calls unchanged (Japanese is untouched):
  `fv_note_on_syl(v, note, const FvSyl *s, vel)`, `fv_note_off_syl(v, const FvSyl *s)`, `fv_syl_onset_ms(s)`,
  `fv_syl_coda_ms(s)`, `fv_viseme(v)`.

Cost target: under 8 KB of tables and code, no extra per-sample work (all in the per-block control path, as now).

### 3. Data

**Phoneme codes:** 1 byte, ARPAbet order, 0 = none. A syllable `FvSyl` = 8 bytes: onset[3], vowel, coda[4].

**Songs (MKS2 v6):** a per-song syllable table of 128 entries × 16 bytes (8 phoneme bytes + 8 spelling characters) =
2 KB, appended to the record. A VOCAL step's mora byte: 0..72 as now (Japanese), **128..255 = syllable table entry**.
v1–v5 songs read as before (no table); a v6 song with no English writes the table as zero-length. The 16 KB song slot
has room. Undo keeps working on steps; the table is append-only per song (deleting unused entries on save).

**Phrase bank:** 64 phrases, each with a name, source text, compiled syllables and optional melody. The device compiles
typed text itself. The web editor sends compiled records, preserving its pronunciations. See section 3b for the wire
and flash layout.

### 4. Playing

- **Live (syllable per note):** with a phrase armed, each note-on (key, MIDI in, or the song's VOCAL track in FOLLOW)
  sings the next syllable at that note's pitch; note-off plays the coda. A held key plus a new key glides (legato)
  into the next syllable without a new attack, as now. Wraps at the end; SEQ rewinds.
- **Into a song:** "place phrase" writes the phrase's syllables into the cursor VOCAL pattern(s) with its stored notes
  and lengths (or the existing notes, one syllable per note).
- **Japanese phrases** in the bank use the existing mora path, so the bank holds both languages.

### 5. Screens

- **PHRASE (new page, after LYRICS):** the phrase list (SELECT), the armed phrase as karaoke text (current word
  highlighted, the current syllable large, the next two small), mode (KEYS / MIDI / FOLLOW), BPM. EDIT opens the
  typing editor (a letter per key, two QWERTY rows, as CHIMERA's FM-1 port does), REC deletes, GLO undoes, SAVE stores
  the bank. PLAY auditions the phrase on its stored melody (or a monotone at its BPM).
- **LYRICS:** shows English syllables by their spelling; ALGORITHM can pick a syllable from the song's table; the
  marks work as now.
- **STAGE:** language gains ENGLISH (the syllable dial then lists the armed phrase's syllables).
- Other pages: unchanged. Exact control assignments are fixed in CONTROLS.md when the page is built.

### 6. Mouth overlay

One set of mouth images drawn over every outfit's face, so English lip-sync needs no per-outfit art:
REST, MBP (closed), FV (lip on teeth), TH (tongue tip), EE, EH, AH (open), OH, OO/W (round), UH, L, R. Japanese A I U E
O N map onto these. `fv_viseme()` gives the current one from the sounding phoneme. Codex draws a test sheet first,
generated from the existing faces' mouth region so the style matches; the owner approves it before the full set.
When the overlay is approved, the outfits' own A I U E O N face frames become redundant (about 6 of 12 frames each):
removing them later is the flash reserve for this whole plan (estimated 60–90 KB), done only on approval.

### 3b. Phrase record (exact layout, little-endian, 1024 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic "MPH1" |
| 4 | 1 | format version (1) |
| 5 | 1 | language: 0 English, 1 Japanese |
| 6 | 1 | BPM 40..240 |
| 7 | 1 | syllable count n, 0..48 |
| 8 | 1 | default length in sixteenths 1..16 (for syllables with no length) |
| 9 | 1 | flags: bit0 has melody |
| 10 | 2 | reserved 0 |
| 12 | 12 | name, A-Z 0-9 space, padded with spaces |
| 24 | 8 | reserved 0 |
| 32 | 128 | text, printable ASCII, NUL-padded (the words as typed, for display and re-editing) |
| 160 | 48 × 16 | syllables: phonemes[8] (English: EP_* onset[3] vowel coda[4], 40 = schwa; Japanese: ph[0] = mora index, rest 0), spelling[6] (NUL-padded), note (MIDI 0 = none, else 1..127), length in sixteenths (0 = default) with bit 7 = word end |
| 928 | 92 | reserved 0 |
| 1020 | 4 | CRC32 (IEEE, as sg_crc32) of bytes 0..1019 |

The encoder zero-fills unused syllable slots, and readers require that padding to be zero. Text can fill all 128 bytes.
Shorter text has a zero-filled tail. Spelling always ends with a NUL.

Japanese `/` and `|` are timed entries with mora zero and the literal token in the spelling field. `|` also marks a
phrase break. A hold uses the preceding mora with spelling `_`. A separated onset uses its mora spelling with an
appended `!`. All seven other phoneme bytes stay zero. The compiler accepts printable, space-separated romaji
with the `ly_compile` grammar, including durations through 127 sixteenths. Recompiling keeps the name, BPM and
any valid default length, with a one-sixteenth default otherwise. It clears the melody.

Placement replaces the touched VOCAL patterns. It uses a continuous sixteenth-note grid with the song's 64-sixteenth
event limit. A missing melody pitch uses the old pitch at that onset, then the supplied fallback. English notes
reattack. Japanese uses the lyric editor's explicit rests, legato and sustain flags.

The bank: 64 records, 4 per 4 KB sector, in four extra 16 KB store slots after the songs (0xBF000–0xCF000). A record
that fails its magic or CRC is an empty phrase (the area may hold another firmware's old data).

### 7. USB protocol (SysEx 0x7D)

Frame: `F0 7D 4D 4B <proto=1> <cmd> <seq> <packed payload> F7`. The payload (raw bytes, then its CRC32, little-endian) is
packed 7-in-8: each group of up to 7 raw bytes becomes one byte of their high bits (bit 0 = the first byte's bit 7) and the
7 low-bit bytes. `seq` 0..127 is echoed in the reply. Through the app SysEx hook (`tools/patch_dinghy_sysex.py` widened
from 0x43 to 0x43 and 0x7D); the frame buffer is 4104 bytes, so payloads stay ≤ 3000 raw bytes.

| cmd | Direction | Payload | Reply |
|---|---|---|---|
| 0x01 HELLO | host → FM-1 | none | 0x41 IDENT: proto u8, phrase count u8, song slots u8, firmware version (ASCII, NUL) |
| 0x02 PHRASE_GET | host → FM-1 | index u8 | 0x42 PHRASE: index u8, record[1024] (an empty slot: all zero) |
| 0x03 PHRASE_PUT | host → FM-1 | index u8, record[1024] | 0x7F ACK: cmd u8, status u8 (0 ok, 1 bad CRC/frame, 2 bad record, 3 busy, 4 refused) |
| 0x04 BANK_LIST | host → FM-1 | none | 0x44 LIST: 64 × (valid u8, language u8, name[12]) |
| 0x05 SONG_BEGIN | host → FM-1 | total length u16 | ACK |
| 0x06 SONG_CHUNK | host → FM-1 | offset u16, up to 2048 bytes | ACK |
| 0x07 SONG_END | host → FM-1 | none (the record's own CRC is checked by song_decode) | ACK |

A PHRASE_PUT is written to the bank when the FM-1 is stopped (a few 50 ms sector erases); while playing the reply is 3
(busy). A received song is held and shown on the FM-1 and replaces the current song only when the player presses REC
there (as the DX7 bank import does); it is stored with SAVE as usual.

The dispatcher applies the busy flag to phrase writes. A song upload stages contiguous, nonempty chunks starting at
offset zero. A refused chunk leaves the accepted offset available for retry. An incomplete end is refused and can
be retried after the missing bytes arrive. A complete end consumes the upload and reports a bad record if
`song_decode` fails. The staging song is updated only on a successful end.

## Phase 2: the web editor (`site/editor.html`)

A single self-contained HTML file that works offline (saved to disk, it still works; nothing loaded from the network
except the page itself). Chrome or Edge for USB (Web MIDI SysEx); other browsers can still edit, preview and save
files.

- Piano roll with a lyric under each note; tempo; English or Japanese (romaji) per phrase.
- Full CMUdict in the page (compressed, ~1 MB); unknown words fall back to the same letter-to-sound C as the device.
  Pronunciations can be edited per word.
- Preview with Miku's real voice: fvoice and the English front end compiled to WebAssembly from the same C files.
- Send / receive the phrase bank, send a song; save and load `.mikuphr` files (JSON).
- Import MIDI and karaoke (.kar) files with lyrics.

## Flash plan

| Item | Estimate |
|---|---|
| English front end code | 8–12 KB |
| Built-in dictionary (~480 words) | 6–8 KB |
| fvoice English tables + code | 5–8 KB |
| PHRASE page, typing editor, SysEx, phrase store | 6–10 KB |
| Mouth overlay (12 shapes) | 4–8 KB |
| **Total** | **29–46 KB** against ~27 KB free |

So the plan needs room. In order: (1) measure the real map first (build with `-Wl,--print-memory-usage` and a size
report); (2) the overlay retires the outfits' mouth frames (60–90 KB, on owner approval); (3) if still short, trim the
built-in dictionary (the web editor carries the full one). Outfits stay.

## Work split and order

| Step | What | Who |
|---|---|---|
| 1 | Toolchain + emulator locally, real flash/RAM size report | Claude |
| 2 | English front end port + host tests vs CHIMERA | Codex (brief), Claude reviews |
| 3 | fvoice English: tables, onset queue, coda, visemes; renders for listening | Claude |
| 4 | Mouth overlay test sheet → owner approval → full set | Codex draws, Claude wires in |
| 5 | MKS2 v6 syllable table, sequencer onset/coda timing | Claude, Codex writes tests |
| 6 | Phrase store, SysEx 0x7D, PHRASE page, typing editor, LYRICS/STAGE changes | Claude; local model for mechanical edits |
| 7 | Emulator validation: builds, CPU deadline, recordings, screenshots | Claude |
| 8 | Owner hardware test | Owner |
| 9 | Web editor | Codex, Claude reviews protocol and WASM build |

Each step is committed separately, logged in DEVLOG.md, tested on the host and, where it touches the firmware, built
and run in the emulator. Nothing is flashed or released without the owner.

## Later

X-ray side-view head on STAGE (our own drawing), GEN writing English lyrics, the web editor streaming audio into the
FM-1 over USB, CHIMERA's real airway on the device, Korean and Russian through the same front end.

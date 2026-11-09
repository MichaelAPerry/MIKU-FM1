# Reusable lyric phrases and native pattern adapter (2026-11-09)

`firmware/song/lyrics.c,h` adds a freestanding text compiler over the existing mora table. It produces caller-owned,
editable `LyUnit` arrays, not a new synthesizer or a replacement song format. A word contains multiple morae;
spaces mark word boundaries; explicit rests and phrase boundaries carry durations. A caller maps units to its own
melody and tick clock. The same compiled sequence can be reused with another melody or tempo.

Example: `mi-ku:2 _:4 |:2 a:3! / i`

| Syntax | Meaning |
| --- | --- |
| `mi-ku` | two morae in one word; separate tokens retain their individual consonants |
| `:2` | duration of two caller-defined ticks (default supplied to compiler) |
| space | end of a word, retained as `LY_WORD_END` |
| `_` | previous vowel on a new melody note, no consonant: a melisma; repeated `_` allowed |
| `/` | pause/rest; default duration or explicit `:N` |
| `\|` | phrase boundary with a pause, retained as `LY_PHRASE_END` |
| `!` | separate onset requested; ordinary mora transitions are legato |

Tokens are case-insensitive and explicit: `mi-ku`, not the ambiguous unsegmented string `miku`. All durations are
1..65535 ticks. The compiler validates in a first pass and only writes after syntax and capacity checks, so errors
leave both the destination and its prior count untouched. Errors identify the byte offset. A null destination with
zero capacity validates and reports the required unit count. The source must stay stable across both passes.

A caller renders an ordinary unit through `fv_note_on_mora(note, ly_consonant(unit), ly_vowel(unit), velocity)`.
For a rest it calls `fv_note_off`; for `LY_SEPARATE` it calls note-off before note-on. Sustained units use `MC_NONE`
with the preceding vowel, retaining the existing voice's legato envelope. The caller publishes the voice's actual
`fv_mouth` and `fv_cons` state for lip synchronization; the compiler does not guess mouth timings from text.
The audition tool demonstrates these rules using the real device voice source.

## Validation and evidence

Build and run:

```sh
cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host -R lyrics --output-on-failure
build/host/firmware/song/lyrics_render build/evidence/lyrics-phrase.wav > build/evidence/lyrics-mouth.csv
```

`lyrics` passes: duration and capacity bounds, malformed and unsupported tokens, case folding, word and phrase
boundaries, melisma and invalid melisma after a rest or moraic n, failure transactionality, and actual voice articulation.
The voice test verifies a consonant on a legato transition without resetting the envelope, a sustained U with no
consonant and actual U mouth, and complete release after a rest.

Default audition: mono 44.1 kHz, 96138 frames (2.18 seconds), peak 16909, RMS 3718, zero full-scale samples.
`lyrics-mouth.csv` contains sample-indexed actual voice mouth/consonant states every 16 frames. These are generated
evidence, not a claim that pronunciation or phrase interpretation has passed an owner listening test.

Each `LyUnit` uses four bytes; 128 units require 512 bytes in caller storage. The module has no static data or buffers.
Host `cc -Os -ffreestanding -fno-builtin` object: 1142 bytes text, zero data/BSS; unresolved references are only
`mora_find`, `mora_cons`, `mora_vowel`. This is an x86 host object measurement, not device flash or ISR timing.
The compiler is intended for editor/control time, never the audio ISR. These initial prototype object measurements
precede the native pattern adapter described below; they are not current complete-firmware measurements.

## Limits and next integration

The Japanese mora model still lacks contracted yoon combinations (`kya`, `shu`, etc.), gemination/sokuon, long-vowel
spelling rules, alternate pronunciation and non-Japanese phonemes. Unknown tokens are refused instead of substituted.
Moraic n cannot supply a vowel for `_`. A diphthong is two explicit vowel units. This adds phrase construction and
articulation control; it does not claim that the existing phoneme inventory is linguistically complete.

## Native editing and saved articulation (2026-11-09)

`lyric_edit.c,h` connects reusable `LyUnit` sequences to the actual existing vocal pattern bank. `ly_apply()` validates
all units and builds a temporary 16-step pattern before committing anything. `ly_read()` reconstructs editable units,
including gaps, and their pitches. Both run in editor/control time with caller-owned buffers; the native caller uses
the existing song edit lock and pattern undo. No fifth track, heap or global phrase bank is introduced.

One tick is one sixteenth on the current 16-step grid. Durations and pauses must fit one bar. A lyric unit can carry
word end, phrase end, consonant-free vowel sustain and separated/legato intent. Notes may be supplied per unit; if
omitted, existing destination pitches at unit onsets are retained and blank onsets use a supplied fallback. Extracting
a phrase and applying it to another pattern therefore reuses the lyrics with another melody. Existing row references
reuse the resulting patterns normally. This is pattern-based reuse, not an independent long-lyric library.

The four formerly reserved vocal flag bits now have explicit MKS2 **version 4** meaning:

| Bit | Name | Meaning |
| --- | --- | --- |
| 0x10 | `VF_LEGATO` | hold this note to the next unit, retaining the voice envelope |
| 0x20 | `VF_SUSTAIN` | this unit continues its preceding vowel without a consonant, including another pitch |
| 0x40 | `VF_WORD_END` | word boundary |
| 0x80 | `VF_PHRASE_END` | phrase boundary; ends outgoing legato |

A rest has note zero, duration, mora zero and at least WORD_END. This distinguishes an explicit lyric pause from
ignored nonzero bytes in legacy empty steps. A separate onset is represented by the preceding note not carrying
LEGATO. Redundant separate markers on the first note or after a rest normalize away: those already start a new attack.
An implicit trailing gap becomes a rest on extraction, so an editable extracted sequence spans all sixteen ticks.
Sustain cannot start a phrase, follow a rest or phrase end, coexist with separate articulation, or continue moraic n.
It must retain the preceding vowel. Overlapping onsets, notes crossing a bar, durations outside the grid, capacity
errors and invalid flags are refused transactionally rather than truncated or silently discarded. Such legacy patterns
remain playable; this bounded editor cannot normalize them without an explicit owner-directed edit.

Version 4 keeps the exact 4508-byte layout and CRC positions of version 3; `Song`, undo and slot buffer sizes do not
grow. The writer marks v4 to avoid older firmware silently interpreting new articulation semantics. Versions 1, 2 and
3 still load with their original voice defaults and sound: historically accepted but ignored vocal high bits are cleared
on import. Source inspection and the old randomized test show that the old decoder accepted these high bits; they
were never a legacy rejection condition. All other field, size, CRC and transactional validation remains unchanged.
Old firmware rejects v4, so users must export/back up projects before downgrading; no unsupported old-version write
is offered.

`lyric_edit` tests cover the full compiled phrase, word and phrase markers, durations, pitches, sustain, pauses, v4
round-trip, v1/v2/v3 high-bit normalization, rejected edits leaving the song untouched, rejected extraction leaving
buffers/count untouched, explicit versus legacy rest discrimination, reuse onto a different melody and undo of all
metadata. The unchanged complete host suite passed 27/27 after the adapter and format changes, before subsequent
native sequencer/UI integration work. Native firmware memory and emulator evidence belong to the final integrated
validation report, not the earlier standalone compiler figures.

`lyric_song_render OUT.wav OUT.csv [--baseline]` provides the complete device-engine evidence path:
text compiler -> editable vocal pattern -> MKS2 encode/decode -> actual sequencer and `en_render()`. Its CSV records
actual mouth, consonant, level, gate, mora and playhead every audio block. The baseline option clears new articulation
bits while retaining the same pitches, morae and grid, allowing direct comparison of the new native articulation.
This differs from `lyrics_render`, which auditions the voice directly and therefore cannot validate native sequencing.

Next validation gate: integrated native LYRICS editing, engine articulation and on-beat event handling, actual firmware
build, emulator edit/save/load/play evidence, and owner listening review. No unrelated language engine is introduced.

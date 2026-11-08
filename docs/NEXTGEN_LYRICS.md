# Reusable lyric phrase prototype (2026-11-08)

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
The compiler is intended for editor/control time, never the audio ISR. Native device UI inclusion has not been done,
so this prototype currently adds no linked native firmware code, static RAM or pool usage.

## Limits and next integration

The Japanese mora model still lacks contracted yoon combinations (`kya`, `shu`, etc.), gemination/sokuon, long-vowel
spelling rules, alternate pronunciation and non-Japanese phonemes. Unknown tokens are refused instead of substituted.
Moraic n cannot supply a vowel for `_`. A diphthong is two explicit vowel units. This adds phrase construction and
articulation control; it does not claim that the existing phoneme inventory is linguistically complete.

MKS2 remains unchanged: the compiler's word/phrase metadata, separate-onset flags and melisma intent are not saved
inside old vocal steps. Silently squeezing these fields into reserved bits would break the documented contract.
The existing step grid also cannot represent arbitrary sub-step gaps. A future device editor/playback adapter needs
owner review of its interface and a measured, versioned persistence proposal. No additional language engine or
physical-device behavior has been selected. Next concrete integration task: drive these units from a bounded live
phrase player with an explicit melody/tick source and mailbox commands, then design device editing and persistence.

# Song model and the MKS2 file format

## DX7 extension: MKS2 v5

Projects using DX7 on chords/bass use4764-byte v5 records. The v4 prefix is
unchanged; two128-byte standard packed DX7 snapshots append after the32-byte
voice block and before the final CRC. Chord/bass preset64–71 identifies factory
origins;72 marks an edited patch. Playback uses the embedded data, not a future
factory-bank lookup. Classic projects still write4508-byte v4. Readers continue
to accept v1–v4; old firmware rejects v5. Ten16 KiB slot strides/addresses are
unchanged. CRC and all active patch fields validate before mutating a song.
See [DX7 implementation and compatibility](DX7_BANK_HANDOFF.md). The v4 tables
below describe the unchanged classic prefix.


Miku-FM songs are pattern based, like a tracker. The model lives in `firmware/song/song.h`; the file format is
`firmware/song/song_io.c`. Everything has a fixed capacity (no heap): `sizeof(Song)` is 4494 bytes, an encoded song
4508 bytes (version 3; one Dinghy slot holds 16384), the undo ring 2594 bytes.

## Current feature-branch format: MKS2 v4

The encoded size remains **4508 bytes**. Native save slots now number ten,
with the original eight addresses and 16 KiB strides preserved. See
[NEXTGEN_NATIVE_INTEGRATION.md](NEXTGEN_NATIVE_INTEGRATION.md) for the storage map.

Vocal flag bits now mean low nibble velocity, `0x10` outgoing legato,
`0x20` incoming sustained vowel, `0x40` word boundary and `0x80` phrase boundary.
A phrase boundary suppresses outgoing legato. Explicit lyric rests have note 0,
a positive duration and a word/phrase boundary flag; ordinary legacy empty cells
remain empty. Sustain suppresses the incoming consonant and follows the actual
preceding gated vowel when available. The appended mora IDs 70–72 are sha, ja
and the existing R→A approximation of rya; IDs 0–69 are unchanged.

Versions 1, 2 and 3 remain readable. Their previously ignored high vocal bits
are cleared on import so old songs keep their previous sound. Older firmware
does not understand version 4. The historical layout tables below still describe
the same byte offsets; their high-bits-reserved wording applies to v1–v3 only.

## Model

- 4 tracks: VOCAL 0, CHORDS 1, BASS 2, DRUMS 3. Each has a bank of **16 patterns** (`NPAT`); a pattern is **one 4/4 bar of
  16 sixteenth steps** (`NSTEP`); a step is 4 bytes:

| Track | byte 0 | byte 1 | byte 2 | byte 3 |
| --- | --- | --- | --- | --- |
| VOCAL | note, MIDI 1..127 (0 = none) | len 1..64 steps (may run past the bar) | mora index (`mora.h`; 0 = vowel A) | flags: velocity 0..15 in the low nibble, high bits reserved |
| CHORDS | root MIDI (0 = none) | type: low nibble quality, high nibble mode | len 1..64 | vel 0..15 |
| BASS | note (0 = none) | len 1..64 | flags: 1 accent, 2 slide into the next note | vel 0..15 |
| DRUMS | mask: 1 kick, 2 snare, 4 clap, 8 closed hat, 16 open hat, 32 perc | accent mask (same bits) | 0 | 0 |

  Chord quality: maj, min, 7, maj7, min7, sus4, sus2, dim, aug, add9 (0..9). Chord mode: 0 block, 1 arp up, 2 arp down,
  3 arp up-down (sixteenths), 4 strum (about 15 ms per note), 5..7 the same arps in thirty-seconds. The sequencer
  voices chords near the previous chord (nearest inversion) inside root-6 .. root+16.
- The vowel of a sung note always comes from its mora (the table is in `mora.c`: gojuon, dakuten, handakuten, n;
  70 entries; each has romaji, vowel A I U E O or N, and a consonant class for the later consonant engine).
- **Song**: 8-char name (A-Z 0-9 space), `bpm` 40..240, `swing` 0..75 (% of a sixteenth by which odd sixteenths are
  delayed; about 33 is a triplet feel), `master` 0..127, `transpose` -24..24 (VOCAL, CHORDS, BASS), `loop`, `nrows`.
  Per track: `level` 0..127, `mute`, `solo`, `preset` 0..7, and the voice parameters `vib`, `porta`, `breath`, `bright`
  0..100 (as the knobs of the first firmware).
- **Arrangement**: 64 rows (`NROW`), each four pattern indices (0..15, or 0xFF = track silent in that row) and a
  section label (none, INTRO, VERSE, CHORUS, BRIDGE, OUTRO). Patterns are reused across rows, that is the point.
- Editing helpers (`sg_*`, all bounds-checked, return success and change nothing on failure): set/clear step, copy and
  paste a step range, clear/duplicate a pattern to the first free slot (empty and unused), transpose a pattern, insert,
  delete and duplicate a row. **Undo**: `un_push_pat` / `un_push_rows` snapshot before an edit, `un_pop` restores;
  8 entries of 324 bytes in a ring (2594 bytes of RAM).

## MKS2

Little-endian, explicit field packing (no struct dumps). Version 3 (written) is version 1 plus a 32-byte voice block before
the CRC, always 4508 bytes. Version 2 (4492 bytes, the first 16 bytes of the block) and version 1 (4476 bytes, no block)
are still read; what they lack gets the defaults (version 1's sound). The table below is version 1; version 3 inserts at 4472:

| Offset | Size | Field |
| --- | --- | --- |
| 4472 | 16 | voice: attack, release, scoop, human, vibrato rate, vibrato delay, tremolo, chorus, delay, reverb, drive (0..100 each), character (0..4: DEEP LOW MIKU YOUNG TINY), language (0..3: JAPANESE ANIMAL SCAT VOWELS), 3 bytes 0 |
| 4488 | 16 | (version 3) tension, fry, whisper, formant; reverb size, delay time, feedback, chorus rate; consonant length, consonant level, vowel speed, on-beat; harmony interval, mix, detune (0..100 each), harmony character (0..4) |
| 4504 | 4 | CRC32 |

Version 1:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 4 | magic `MKS2` |
| 4 | 2 | version = 1 |
| 6 | 4 | total length in bytes, CRC included (4476) |
| 10 | 8 | name |
| 18 | 6 | bpm, swing, master, transpose (int8), loop, nrows |
| 24 | 32 | per track: level, mute, solo, preset, vib, porta, breath, bright |
| 56 | 320 | 64 rows: 4 pattern indices, section |
| 376 | 4096 | patterns, track-major: 4 x 16 x 16 steps x 4 bytes |
| 4472 | 4 | CRC32 (IEEE 802.3, reflected, init/xorout 0xFFFFFFFF) of bytes 0..4471 |

`song_decode` checks, in order: exact length, magic, version, the length field, the CRC, then the bounds of **every**
field (name characters, bpm, swing, levels, indices, section, and each step as for `sg_step_ok`). It validates the
whole buffer before it writes the first byte of the destination, so a failed decode leaves the song untouched.
Change the version when the layout changes.

## Sequencer

`sq_block(Seq*, const Song*, frames, events, max)` (`seq.h`) is called from the audio callback and fills note-on,
note-off, drum and step events with exact frame offsets (44.1 kHz; integer accumulators, no drift at any tempo).
See the header for the event fields and the timing rules.

## MIK1 (legacy host prototype, version 1)

Superseded by MKS2; kept for reference. Used by `shared/song_io.c` and the host demo tools only.

Canonical little-endian binary format; no C struct dumps.

- Bytes 0-3: ASCII `MIK1`
- Bytes 4-5: uint16 version = 1
- Bytes 6-7: uint16 tempo BPM (20-300)
- Byte 8: number of patterns (0-16)
- Bytes 9-12: four one-byte mute flags
- Byte 13: reserved zero
- Repeat per pattern: uint16 note count (0-128), then note records (12 bytes each)
- Note: uint32 tick start, uint32 tick duration, uint8 MIDI pitch, uint8 velocity, uint8 vowel index, uint8 track index
- Final four bytes: CRC32 (IEEE) of all preceding bytes.

Every decode verifies CRC, exact length, note bounds, pattern bounds and version before applying the result. All times
use 96 PPQN. Patterns only: no arrangement, lyrics or expression.

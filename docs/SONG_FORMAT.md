# MIK1 (prototype, version 1)

Canonical little-endian binary format; no C struct dumps.

- Bytes 0–3: ASCII `MIK1`
- Bytes 4–5: uint16 version = 1
- Bytes 6–7: uint16 tempo BPM (20–300)
- Byte 8: number of patterns (0–16)
- Bytes 9–12: four one-byte mute flags
- Byte 13: reserved zero
- Repeat per pattern: uint16 note count (0–128), then note records (12 bytes each)
- Note: uint32 tick start, uint32 tick duration, uint8 MIDI pitch, uint8 velocity, uint8 vowel index, uint8 track index
- Final four bytes: CRC32 (IEEE) of all preceding bytes.

Every decode verifies CRC, exact length, note bounds, pattern bounds and version before applying the result. The decoder does not mutate the destination on validation failure. All times use 96 PPQN. File format is presently for *patterns*, not yet arrangement references, lyric metadata or expression curves. Update the version when extending it.

# DX7 factory bank (original voices)

`factory.packed` is 10 x 128 bytes of standard packed DX7 voices, bank index 0..9 (preset
IDs 64..73). All ten were authored for this project in `tools/dx7_candidates.py`, which holds every
parameter. There is no third-party cartridge or patch data in this file, so the voices are
GPL-3.0-only project data (see `LICENSING.md`).

| Index | Name | GEN style | Track |
|---:|---|---|---|
| 0 | HYPER GLAS | HYPERPOP | chords |
| 1 | HYPER 808 | HYPERPOP | bass |
| 2 | FUTURE SAW | FUTURE BASS | chords |
| 3 | FUTURE SUB | FUTURE BASS | bass |
| 4 | CLUB STAB | JERSEY CLUB | chords |
| 5 | CLUB BOUNC | JERSEY CLUB | bass |
| 6 | DIGI PLUCK | DIGICORE | chords |
| 7 | DIGI GROWL | DIGICORE | bass |
| 8 | IDOL KEYS | IDOL POP | chords |
| 9 | IDOL BASS | IDOL POP | bass |

Single source of truth and rebuild:

```sh
python3 tools/dx7_candidates.py          # writes assets/dx7/factory.packed ( --check verifies it )
python3 tools/gen_dx7_bank.py            # writes firmware/audio/dx7/bank.h ( --check verifies it )
```

Names in `bank.h` come from the patches. The bank holds at most 10 voices because preset IDs
74 (edited) and 75 (user bank) follow the factory range. Audition renders are in
`docs/evidence/dx7-candidates/`. Engine code has separate Apache-2.0 provenance
(`firmware/audio/dx7/`). The voices are first drafts that have not been heard on a device speaker.

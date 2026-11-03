/* SPDX-License-Identifier: GPL-3.0-only */
/* Step 4: song generator. Writes ordinary patterns and rows into a Song, so everything it makes is editable on the device.
 * Deterministic: the same settings give the same song on the host and the device. Integer only, no libc, no heap. */
#ifndef GEN_H
#define GEN_H
#include "firmware/song/song.h"
enum { GS_POP, GS_BALLAD, GS_DANCE, GS_CHIP, GS_N };
enum { GK_MAJOR, GK_MINOR };
typedef struct {
    uint8_t style;              /* GS_* */
    uint8_t key;                /* 0..11 = C..B (the tonic) */
    uint8_t scale;              /* GK_* */
    uint8_t lock;               /* bit per track (1 << TR_*): a locked track is never changed */
    uint16_t seed;
} GenCfg;
const char *gen_style(uint32_t s);  /* "POP", "BALLAD", "DANCE", "CHIP"; "" out of range */
/* Whole song. No track locked (or no rows): a new arrangement (intro 2, verse 4, chorus 4, verse 4, chorus 4, outro 2 rows),
   name, tempo and presets from the style, all four tracks. Some track locked: the arrangement and the locked tracks stay;
   the unlocked tracks are rewritten over the existing rows, and fit the chords of the CHORDS track (locked or new). */
void gen_song(Song *g, const GenCfg *c);
/* One phrase: rewrite the pattern of `trk` used in `row` (a free one if the row has none; the change shows in every row that
   uses that pattern). `var` picks the variation (0 = what gen_song would write). 1 = done, 0 = refused (no row, track locked,
   no free pattern). */
int gen_phrase(Song *g, const GenCfg *c, int row, int trk, uint32_t var);
#endif

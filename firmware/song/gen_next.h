/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental candidate generator. No change to the shipping GEN lineup or MKS2. */
#ifndef GEN_NEXT_H
#define GEN_NEXT_H
#include "firmware/song/song.h"
enum { GX_HYPERPOP, GX_FUTURE_BASS, GX_JERSEY, GX_DIGICORE, GX_IDOL_POP, GX_N };
typedef struct {
    uint16_t seed;
    uint8_t style, key, minor, lock, energy, complexity; /* key 0..11, lock track bits; amounts 0..100 */
} GenNextCfg;
const char *gen_next_style(uint32_t style);
/* Returns 1 on success. Capacity/invalid settings return 0 with g unchanged. Locked tracks and arrangement stay exact.
 * A locked arrangement with more than 16 required musical contexts is refused rather than silently misvoiced.
 * Settings are host/experimental state, not serialized into the Song. Output is ordinary editable MKS2 patterns. */
int gen_next_song(Song *g, const GenNextCfg *cfg);
/* Replaces the referenced pattern only (shared rows hear the change); refuses invalid/locked/off rows. */
int gen_next_phrase(Song *g, const GenNextCfg *cfg, int row, int track, uint32_t variation);
#endif

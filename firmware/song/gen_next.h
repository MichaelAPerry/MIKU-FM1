/* SPDX-License-Identifier: GPL-3.0-only */
/* Candidate generator alongside the legacy styles. Ordinary Song output; no MKS2 format change. */
#ifndef GEN_NEXT_H
#define GEN_NEXT_H
#include "firmware/song/song.h"
enum { GX_HYPERPOP, GX_FUTURE_BASS, GX_JERSEY, GX_DIGICORE, GX_IDOL_POP, GX_N };
typedef struct {
    uint16_t seed;
    uint8_t style, key, minor, lock, energy, complexity; /* key 0..11, lock track bits; amounts 0..100 */
} GenNextCfg;
/* Caller-owned scratch: 840 bytes on pi32v2. Never alias g/c; reuse after call returns.
 * Native caller places this in a fixed pool/main-loop workspace, never on the audio ISR stack. */
typedef struct {
    Row row[NROW];
    uint32_t keys[NTRK][NPAT];
    uint8_t index[NTRK][NROW], counts[NTRK], nrows;
} GenNextWorkspace;
const char *gen_next_style(uint32_t style);
/* Returns 1 on success. Capacity/invalid settings return 0 with g unchanged. Locked tracks and arrangement stay exact.
 * A locked arrangement with more than 16 required musical contexts is refused rather than silently misvoiced.
 * Generator settings are not serialized into the Song. Output is ordinary editable MKS2 patterns. */
int gen_next_song_ws(Song *g, const GenNextCfg *cfg, GenNextWorkspace *workspace);
/* Host convenience wrapper uses an 840-byte local workspace. Native integration must call _ws. */
#ifndef GEN_NEXT_NO_STACK_WRAPPER
int gen_next_song(Song *g, const GenNextCfg *cfg);
#endif
/* Replaces the referenced pattern only (shared rows hear the change); refuses invalid/locked/off rows. */
int gen_next_phrase(Song *g, const GenNextCfg *cfg, int row, int track, uint32_t variation);
#endif

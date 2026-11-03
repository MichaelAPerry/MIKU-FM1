/* SPDX-License-Identifier: GPL-3.0-only */
/* Internal helpers shared by the fmband sources (Q24 envelopes, per-block). */
#ifndef FMBAND_ENV_H
#define FMBAND_ENV_H
#include <stdint.h>
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_tables.h"
#define FB_FULL (1u << 24)
static inline int32_t fb_clamp(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline uint32_t fb_xs(uint32_t x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
/* exponential fall of e toward t (k = fmb_kdec); snaps to t within -72 dB, never sticks */
static inline uint32_t fb_ap(uint32_t e, uint32_t t, uint32_t k) {
    if (e <= t) return t;
    uint32_t d = e - t, s = ((d >> 8) * k) >> 8;
    if (!s) s = 1;
    return d - s < 4096 ? t : e - s;
}
static inline uint32_t fb_sus(uint8_t s) { return (uint32_t)s * 65793u > FB_FULL ? FB_FULL : (uint32_t)s * 65793u; }
static inline int fb_step(FbEnv *v, uint8_t atk, uint8_t dec, uint8_t sus, uint8_t rel) { /* returns 1 while sounding */
    switch (v->st) {
    case 1: v->e += fmb_katk[atk & 63]; if (v->e >= FB_FULL) { v->e = FB_FULL; v->st = 2; } break;
    case 2: { uint32_t t = fb_sus(sus); v->e = fb_ap(v->e, t, fmb_kdec[dec & 63]); if (v->e <= t) v->st = t ? 3 : 0; break; }
    case 4: v->e = fb_ap(v->e, 0, fmb_kdec[rel & 63]); if (!v->e) v->st = 0; break;
    default: break;
    }
    return v->st != 0;
}
static inline int32_t fb_vg(uint32_t lvl, uint8_t vel) { return (int32_t)((lvl * (24u + (vel > 127 ? 127u : vel))) >> 7); }
#endif

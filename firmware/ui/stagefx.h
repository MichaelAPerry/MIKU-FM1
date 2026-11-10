/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_STAGEFX_H
#define MIKU_STAGEFX_H
#include <stdint.h>
enum { STFX_OFF, STFX_FIREWORKS, STFX_SPARKLES, STFX_CONFETTI, STFX_SMOKE, STFX_BEAMS, STFX_N };
/* Absolute-time, deterministic overlay. At most three 32px sprites, no state,
 * allocations or drawing/USB calls. Zero means transparent. */
/* Optional caller-owned decoded rows: one row per bounded sprite instance.
 * No shared mutable cache; init before first use. Output equals stagefx_pixel. */
typedef struct {uint32_t valid,mode,age,intensity,y[3];uint8_t rows[3][32];} StageFxCache;
void stagefx_cache_init(StageFxCache *cache);
uint16_t stagefx_cached_pixel(StageFxCache *cache,uint32_t mode,uint32_t age_ms,uint32_t intensity,uint32_t x,uint32_t y);
uint16_t stagefx_pixel(uint32_t mode,uint32_t age_ms,uint32_t intensity,uint32_t x,uint32_t y);
#endif

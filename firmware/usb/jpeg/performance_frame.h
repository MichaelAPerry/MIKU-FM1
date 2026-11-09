/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_JPEG_PERFORMANCE_FRAME_H
#define MIKU_JPEG_PERFORMANCE_FRAME_H
#include "firmware/ui/performance.h"
/* MjPixel callback:160x120 frame contains 120x120 original240x240 scene at x20.
 * Caller passes immutable PerfState snapshot; original PCL art,see LICENSING.md. */
/* One MCU row spans at most 12 source rows at the fixed 2:3 video/sprite
 * scale. Cache decoded index rows, never a framebuffer; caller owns storage. */
typedef struct {
    const PerfState *scene;
    uint8_t row_y[12],rows[12][61];
    uint32_t decoded_rows;
} MjPerfCache;
void mj_perf_cache_init(MjPerfCache *cache,const PerfState *scene);
uint16_t mj_perf_cached_pixel(void *cache,uint32_t x,uint32_t y);
uint16_t mj_perf_pixel(void *state,uint32_t x,uint32_t y);
#endif

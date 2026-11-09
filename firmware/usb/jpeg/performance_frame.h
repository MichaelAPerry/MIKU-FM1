/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_JPEG_PERFORMANCE_FRAME_H
#define MIKU_JPEG_PERFORMANCE_FRAME_H
#include "firmware/ui/performance.h"
/* MjPixel callback:160x120 frame contains 120x120 original240x240 scene at x20.
 * Caller passes immutable PerfState snapshot; original PCL art,see LICENSING.md. */
uint16_t mj_perf_pixel(void *state,uint32_t x,uint32_t y);
#endif

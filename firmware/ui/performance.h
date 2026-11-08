/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_PERFORMANCE_H
#define MIKU_PERFORMANCE_H
#include <stdint.h>
enum { PERF_W=240, PERF_H=240, PERF_SCENE_H=214 };
enum { PERF_BG_ORIGINAL, PERF_BG_GRID, PERF_BG_HORIZON, PERF_BG_N };
enum { PERF_FRAME_NONE, PERF_FRAME_CORNERS, PERF_FRAME_RAILS, PERF_FRAME_N };
enum { PERF_PALETTE_ORIGINAL, PERF_PALETTE_AMBER, PERF_PALETTE_N };
enum { PERF_DIRTY_SPRITE=1, PERF_DIRTY_MOUTH=2, PERF_DIRTY_STAGE=4, PERF_DIRTY_STATUS=8 };
/* Atlas IDs refer to renderer-owned sprites, not copies of image data. Atlas0 is
   singer_sprites.h SINGER_STAGE (PCL art; retain LICENSING.md/attribution). */
typedef struct {
    const char *name;
    uint8_t atlas, first_frame, frames, width, height, scale;
    int16_t x, y;
} PerfOutfit;
#define PERF_OUTFIT_N 1u
const PerfOutfit *perf_outfit(uint32_t id); /* NULL for unregistered IDs */
typedef struct {
    uint32_t now_ms, beat_q8; /* absolute beat position including fractional part; no redraw-derived time */
    uint8_t playing, section, mouth, vocal_level, audio_level, transient;
} PerfInput;
typedef struct {
    uint32_t now_ms, bow_until, beat_q8;
    uint8_t outfit, background, frame, palette;
    uint8_t sprite, mouth, light, section, playing, initialized, bow_active;
} PerfState;
void perf_init(PerfState *s);
/* No mutation on invalid selection. Nondefault visual selections are unapproved
   exploration presets; no costume is synthesized or substituted. */
uint32_t perf_select(PerfState *s,uint32_t outfit,uint32_t background,uint32_t frame,uint32_t palette);
uint32_t perf_update(PerfState *s,const PerfInput *in);
/* Procedural background + decorative framing only; overlay the original atlas
   transparently afterwards. Outside the scene is black (status/footer belongs
   to caller). No framebuffer, allocations, floats, USB or display operations. */
uint16_t perf_pixel(const PerfState *s,uint32_t x,uint32_t y);
#endif

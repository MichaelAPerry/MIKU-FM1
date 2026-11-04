/* SPDX-License-Identifier: GPL-3.0-only */
/* The voice's effects (integer only, no libc, fixed buffers): drive -> tremolo -> chorus -> delay -> reverb.
 * Mono voice in (the left channel), stereo out, in place. One instance (module state). A stage whose knob is 0 costs
 * nothing and passes the sound through exactly; its buffer is cleared when it is switched on again.
 * On the device the buffers (about 147 KB) go to Dinghy's .pool section: define VX_POOL before including vfx.c. */
#ifndef VFX_H
#define VFX_H
#include <stdint.h>
typedef struct {
    uint8_t drv, trem, cho, dly, rev;         /* amounts, 0..100 */
    uint8_t rsize, dtime, dfb, crate;         /* reverb size (42: feedback 0.80), delay time (0..100 in 5 steps: 1/16, 1/8 triplet,
                                                 1/8, dotted 1/8, 1/4), delay feedback (0.18..0.73), chorus rate (0.1..4 Hz) */
} VxSet;
void vx_init(void);
void vx_render(int32_t *st, uint32_t n, const VxSet *s, uint32_t bpm);   /* st: n stereo frames, L = R = the voice on entry */
#endif

/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_PEDAL_H
#define MIKU_PEDAL_H
#include <stdint.h>
#define PEDAL_MAX_FRAMES 256u
typedef struct {
    uint8_t wet, drive, tone;        /* 0..100; tone100 = no low-pass; drive0 = exact dry FX */
    uint8_t level, synth;            /* 0..127, 127 = unity */
    uint8_t bypass, mix_headroom;    /* bypass0/1; combined input+synth gain 0..100 (default50) */
} PedalSet;
typedef struct {
    int32_t filter[2], wet_q8, level_q8, synth_q8;
    uint32_t mean_square, clipped_input, clipped_output, frames;
    uint16_t peak, transient;        /* incoming stereo peak and increase from preceding block */
    uint8_t initialized, connected;
} Pedal;
void pedal_default(PedalSet *s);
void pedal_init(Pedal *p);
/* Caller supplies a stable settings snapshot. Stereo signed-Q15 samples in int32 containers; output OVERWRITES.
 * n=1..256, returns1; invalid arguments return0 without touching output or state.
 * input=NULL means disconnected/starved: clear FX state and input analysis; synth may continue.
 * synth=NULL means no generated audio. All buffers may alias. No allocation, blocking or global state.
 * First settings take effect exactly; later wet/level/synth changes smooth once per frame.
 * Analysis always observes incoming samples BEFORE effects, mute and mixing. */
int pedal_process(Pedal *p,const PedalSet *s,const int32_t *input,const int32_t *synth,int32_t *out,uint32_t n);
#endif

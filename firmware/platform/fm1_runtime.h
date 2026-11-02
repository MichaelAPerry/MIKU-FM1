/* SPDX-License-Identifier: MIT
 * Hardware-independent half-buffer interface for FM-1 audio integration.
 * No claims of being a flashable firmware or device HAL. */
#ifndef MIKU_FM1_RUNTIME_H
#define MIKU_FM1_RUNTIME_H
#include <stdint.h>
#include "firmware/audio/band/band.h"
#include "firmware/audio/vocal/vocal.h"
#include "firmware/sequencer/sequencer.h"
typedef struct {
    MikuSequencer tracks[MIKU_TRACKS];
    MikuVoice vocal;
    MikuBand band;
    MikuDrums drums;
    uint64_t frame;
    uint32_t sample_rate;
    uint8_t mute[MIKU_TRACKS];
    uint8_t running, vocal_pitch;
} MikuRuntime;
int miku_runtime_init(MikuRuntime *rt,const MikuSong *song,uint32_t sample_rate);
void miku_runtime_render(MikuRuntime *rt,int32_t *stereo_interleaved,uint32_t frames);
#endif

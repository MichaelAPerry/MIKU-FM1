/* SPDX-License-Identifier: MIT */
#ifndef MIKU_BAND_H
#define MIKU_BAND_H
#include <stdint.h>
#include "shared/song.h"
#define MIKU_CHORD_VOICES 4
typedef struct { uint8_t active,pitch;float phase,mod_phase,frequency,level,age; } MikuTone;
typedef struct {MikuTone chords[MIKU_CHORD_VOICES],bass;uint32_t sample_rate;float bass_slide;} MikuBand;
typedef struct {float phase,remaining,level,frequency;uint8_t kind;uint32_t noise;} MikuDrums;
int miku_band_init(MikuBand *b,uint32_t sample_rate);
void miku_band_note(MikuBand *b,const MikuNote *n,int on);
float miku_band_render(MikuBand *b);
void miku_drums_init(MikuDrums *d);
void miku_drums_trigger(MikuDrums *d,uint8_t note,uint8_t velocity,uint32_t sample_rate);
float miku_drums_render(MikuDrums *d,uint32_t sample_rate);
#endif

/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2012-2013 Google Inc.; C embedded adaptation 2026 MIKU-FM1 contributors. */
#ifndef MIKU_DX7_H
#define MIKU_DX7_H
#include <stdint.h>
#define DX_VOICES 4u
#define DX_BLOCK 64u
#define DX_PACKED 128u
#define DX_FADE 64u /* samples over which a stolen voice's tail fades out */
/* All state belongs to the caller; no heap, runtime table generation or libm. */
typedef struct {
    uint8_t rates[4],levels[4],stage,down,rising,scale;
    int32_t output,level,target,inc;
} DxEnv;
typedef struct {
    uint8_t rates[4],levels[4],stage,down,rising;
    int32_t level,target,inc;
} DxPitchEnv;
typedef struct {
    uint32_t phase,delta,delay,delayinc,delayinc2;
    uint8_t wave,sync,random;
} DxLfo;
typedef struct {
    DxEnv env[6];DxPitchEnv pitch;DxLfo lfo;
    uint32_t phase[6],age;
    int32_t base[6],gain[6],feedback[2],audio[DX_BLOCK];
    int32_t gofs,ginc; /* legato glide: log-frequency offset still to travel (added to ratio operators), step per block */
    uint16_t gleft;    /* blocks of glide remaining */
    uint8_t ams[6],algorithm,fbshift,pmd,pms,amd,note,active,gated,left,carriers;
    uint8_t keyed,fixed,midi,xpose; /* key physically held; bitmask of fixed-frequency operators; sounding transposed key; patch transpose-24 (two's complement) */
} DxVoice;
typedef struct {
    uint8_t patch[DX_PACKED],voices,level,pan,bend_range,sustain;
    int32_t bend;      /* current pitch-bend offset in log-frequency units (1<<24 per octave) */
    DxVoice voice[DX_VOICES];
    int32_t bus[2][DX_BLOCK];
    int32_t gl,gr,ghost[DX_FADE];uint32_t age,clock;
    uint8_t gpos,glen;
} DxSynth;
uint32_t dx_patch_valid(const uint8_t packed[DX_PACKED]);
void dx_init(DxSynth *s,uint32_t voices);
uint32_t dx_patch(DxSynth *s,const uint8_t packed[DX_PACKED]);
void dx_note_on(DxSynth *s,uint8_t note,uint8_t velocity);
void dx_note_off(DxSynth *s,uint8_t note);
void dx_all_off(DxSynth *s);
/* Mono legato / glide: with a gated voice, retune it to `note` without retriggering envelopes, LFO or phases, sliding the
 * log-frequency of every ratio operator linearly over glide_ms (0 = jump; capped at 60000). Fixed-frequency operators do not
 * move. The original note's velocity, key scaling and rate scaling are kept (velocity is only used when it falls back).
 * With no gated voice it is dx_note_on. With more than one voice the newest gated voice is retuned. */
void dx_note_legato(DxSynth *s,uint8_t note,uint8_t velocity,uint32_t glide_ms);
/* Pitch bend (signed 14-bit, -8192..8191) times range (semitones, default 2) applies at once to every sounding voice. */
void dx_set_bend(DxSynth *s,int32_t bend);
void dx_set_bend_range(DxSynth *s,uint8_t semis);
/* Sustain pedal: while on, dx_note_off keeps the voice sounding; turning it off releases every voice whose key is up.
 * dx_all_off and dx_hush ignore the pedal. */
void dx_set_sustain(DxSynth *s,uint8_t on);
/* Fade every sounding voice to silence within two blocks and free it (SAVE, panic): no hard step. */
void dx_hush(DxSynth *s);
void dx_set_mix(DxSynth *s,uint8_t level,uint8_t pan);
void dx_render(DxSynth *s,int32_t *stereo,uint32_t frames);
uint32_t dx_active(const DxSynth *s);
#endif

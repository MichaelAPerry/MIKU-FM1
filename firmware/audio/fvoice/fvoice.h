/* SPDX-License-Identifier: GPL-3.0-only */
/* Integer-only formant singing voice (no float/libm/heap/64-bit math; 44.1 kHz). */
#ifndef FVOICE_H
#define FVOICE_H
#include <stdint.h>
#define FV_BLK 16               /* frames per control block */
enum { FV_A, FV_I, FV_U, FV_E, FV_O };
typedef struct { int32_t g, a1, a2; } FvRes;
typedef struct {
    /* params: set directly (fv_init gives defaults) */
    uint16_t vib_depth;         /* cents 0..100 (peak) */
    uint16_t vib_rate;          /* 0.1 Hz units, ~30..80 */
    uint16_t vib_delay;         /* ms before the vibrato fades in */
    uint16_t porta;             /* ms, 0..510 */
    uint16_t attack, release;   /* ms, 0..510; release = time to -40 dB */
    uint8_t breath, bright;     /* 0..127 */
    /* state */
    uint32_t ph, inc, lfo, noise;
    int32_t pcur, ptgt;         /* pitch, 4096 units/octave above MIDI 0 */
    int32_t lp, env, gcur, gstep;
    int32_t y1[4], y2[4], cur[4][3];
    FvRes r[4];
    uint16_t vt;
    uint8_t gated, vowel, velg, left;
} FvVoice;
void fv_init(FvVoice *v);
void fv_note_on(FvVoice *v, uint8_t midi_note, uint8_t vowel, uint8_t velocity); /* gated: legato */
void fv_note_off(FvVoice *v);
void fv_set_vowel(FvVoice *v, uint8_t vowel);       /* 0..4 = A I U E O */
void fv_render(FvVoice *v, int32_t *out, uint32_t frames); /* ADDS stereo Q15 L R */
uint8_t fv_mouth(const FvVoice *v);                 /* 0..4 vowel, 5 closed */
uint8_t fv_level(const FvVoice *v);                 /* envelope 0..255 */
#endif

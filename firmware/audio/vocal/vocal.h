/* SPDX-License-Identifier: MIT */
#ifndef MIKU_VOCAL_H
#define MIKU_VOCAL_H
#include <stdint.h>
#include "shared/song.h"
typedef struct {
    float sample_rate, phase, frequency, amplitude, breath;
    float z1[3], z2[3], a1[3], a2[3], gain[3];
    uint32_t noise_state;
    uint8_t active, vowel;
} MikuVoice;
int miku_voice_init(MikuVoice *v, uint32_t sample_rate);
void miku_voice_note_on(MikuVoice *v, uint8_t midi_pitch, uint8_t velocity, MikuVowel vowel);
void miku_voice_note_off(MikuVoice *v);
float miku_voice_render(MikuVoice *v);
#endif

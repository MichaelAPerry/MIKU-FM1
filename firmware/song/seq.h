/* SPDX-License-Identifier: GPL-3.0-only */
/* Sample-accurate song sequencer (44.1 kHz), integer only, for an audio ISR. sizeof(Seq) ~ 130 bytes.
 * sq_block() fills events for the next `frames` frames, each with a frame offset 0..frames-1 (events of one frame are
 * ordered NOTE_OFF, STEP, NOTE_ON/DRUM). Give it room for SQ_EVMAX events per block (frames <= 1024); excess is dropped.
 * Event fields: NOTE_ON/OFF: trk, note (MIDI, transpose applied), vel 0..127, aux (VOCAL: vowel MV_*, mora = mora index;
 * BASS: bit0 accent, bit1 "slide": legato into this note, the previous note got no NOTE_OFF; CHORDS 0).
 * DRUM: trk=3, note = instrument bit number 0..5 (D_KICK..D_PERC), vel. STEP: note = row, aux = step 0..15, at the
 * (swung) moment the step sounds. Track levels and the master are NOT applied here (the mixer does); mute/solo are.
 * Timing: swing delays odd sixteenths by swing% of a sixteenth. Notes: VOCAL off at 85% of len, BASS likewise unless
 * BF_SLIDE (held to the next note), chords at len. Arp: one note per sixteenth (CM_*) or per 32nd (CM_*32), strum 15 ms. */
#ifndef SEQ_H
#define SEQ_H
#include <stdint.h>
#include "firmware/song/song.h"
enum { SQ_NOTE_ON = 1, SQ_NOTE_OFF, SQ_DRUM, SQ_STEP };
typedef struct { uint32_t at; uint8_t type, trk, note, vel, aux, mora; } SqEvent;
#define SQ_FS 44100u
#define SQ_STRUM 661u
#define SQ_EVMAX 64u
#define SQ_NTM 8
typedef struct { int32_t due; uint8_t kind, a, b, c; } SqTm;
typedef struct {
    uint8_t state, rowloop, row, step, fl;      /* row/step: next step to start (row 0xFF: song ended) */
    uint16_t bpm, ap_left;
    uint32_t rem, slen;                         /* accumulator, frames of the current step */
    int32_t to_grid;                            /* frames to the next nominal sixteenth */
    uint8_t vn, bn, bsl;                        /* sounding vocal / bass note (0 none), bass slide pending */
    uint8_t ncs, cs[8], ncn, cn[8], npv, pv[8]; /* sounding chord notes, current voicing, previous voicing */
    uint8_t cvel, ap_on, ap_mode, ap_i, ap_fast, sm_i;
    SqTm tm[SQ_NTM];
} Seq;
void sq_init(Seq *s);
int sq_play(Seq *s, const Song *g, uint32_t row, int row_loop);   /* from step 0 of `row`; 0 if row >= nrows */
void sq_stop(Seq *s);               /* the next sq_block emits NOTE_OFFs for everything sounding, then idles */
int sq_playing(const Seq *s);
int sq_set_bpm(Seq *s, uint32_t bpm);   /* 40..240, mid-play without a glitch */
uint32_t sq_block(Seq *s, const Song *g, uint32_t frames, SqEvent *ev, uint32_t max);
#endif

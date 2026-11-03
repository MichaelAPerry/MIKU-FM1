/* SPDX-License-Identifier: GPL-3.0-only */
/* The song engine: sequencer events -> voice (fvoice), chords, bass, drums (fmband), mixed into stereo Q15.
 * The same code runs in the device's audio interrupt (native/dinghy.c) and in the host render tool, so a host WAV is the
 * device mix. en_render() ADDS into `out`, which must be zero on entry; any block size up to 1024 frames.
 * Threads: the main loop talks to the ISR only through en_post() (a single-producer ring of one-word commands), the volatile
 * pan[] bytes, en_lock()/en_unlock() and the published words below (each written by the ISR only). */
#ifndef ENGINE_H
#define ENGINE_H
#include <stdint.h>
#include "firmware/song/song.h"
#include "firmware/song/seq.h"
#include "firmware/song/mora.h"
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/audio/fmband/fmband.h"
enum { EN_PLAY = 1, EN_STOP, EN_BPM, EN_ON, EN_OFF, EN_VOW, EN_PANIC };   /* PLAY a=row b=loop-one-row; BPM a; ON a=trk b=note c=mora (VOCAL) / unused; OFF a=trk b=note; VOW a=mora */
#define EN_QN 32u
typedef struct {
    Seq sq; FvVoice fv; ChSynth ch; BsSynth bs; DrKit dr;
    const Song *g;
    SqEvent ev[SQ_EVMAX];
    int32_t vl, vr;
    uint8_t pre[NTRK], vh, up;
    volatile uint8_t pan[NTRK];               /* 0..127, 64 centre; main loop writes (device setting, not in the song) */
    volatile uint32_t q[EN_QN], qh, qt;       /* command ring: main writes q[qh] then qh; ISR reads q[qt] then qt */
    volatile uint32_t lock, ack;              /* main: lock=1 and wait ack==1: the ISR no longer runs the sequencer or reads commands */
    volatile uint32_t pos, mouth, level, act, mora;   /* ISR -> main: playing<<16|row<<8|step; fvoice mouth, level; track activity bits; last sung mora */
} Engine;
void en_init(Engine *e, const Song *g);       /* instruments and sequencer only; does not touch the mailbox */
uint32_t en_post(Engine *e, uint32_t kind, uint32_t a, uint32_t b, uint32_t c);   /* main side; 0 if the ring is full */
void en_render(Engine *e, int32_t *out, uint32_t n);
uint32_t en_lock(Engine *e, void (*poll)(void), uint32_t tries);   /* 1 = the ISR has stopped touching the song; poll() is called while waiting */
void en_unlock(Engine *e, void (*poll)(void), uint32_t tries);
uint32_t en_pre(uint32_t trk, uint32_t p);    /* the preset that is actually used for p */
#endif

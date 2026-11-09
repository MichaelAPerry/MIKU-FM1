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
#include "firmware/audio/vfx/vfx.h"
#include "firmware/audio/dx7/dx7.h"
enum { EN_PLAY = 1, EN_STOP, EN_BPM, EN_ON, EN_OFF, EN_VOW, EN_PANIC, EN_RATE, EN_HUSH };   /* PLAY a=row b=loop-one-row; BPM a; ON a=trk b=note c=mora (VOCAL) / unused; OFF a=trk b=note; VOW a=mora; RATE a=ANIMAL speech rate 0..100 (session setting); HUSH = PANIC plus a fast fade-out of the DX7 tails (SAVE) */
#define EN_QN 32u
typedef struct { uint32_t due; uint8_t on, note, mora, vel, flags; uint16_t id; } EnVq;   /* a delayed vocal note event, including VF_SUSTAIN (on-beat) */
#define EN_VQN 16u
typedef struct {
    Seq sq; FvVoice fv, fv2; ChSynth ch; BsSynth bs; DrKit dr;   /* fv2: the harmony voice */
    DxSynth dx[2]; uint32_t dx_crc[2];uint8_t dx_loaded[2];          /* chord/bass DX7 engines; legacy voices stay selectable */
    const Song *g;
    SqEvent ev[SQ_EVMAX];
    int32_t vl, vr;
    uint8_t pre[NTRK], vh, up, vnote, vvel, vmora, arate;
    uint32_t bt, rng;                         /* ANIMAL / SCAT language: frames to the next babbled syllable; its random state */
    uint32_t clk, bw;                         /* frames rendered; band delay write position */
    EnVq vq[EN_VQN];                          /* on-beat: vocal note events waiting for their moment */
    uint16_t vid, vcur;                       /* note ids: the newest, the one sounding (a delayed note-off ends only its own note) */
    uint8_t vqn, bdon;
    volatile uint8_t pan[NTRK];               /* 0..127, 64 centre; main loop writes (device setting, not in the song) */
    volatile uint32_t q[EN_QN], qh, qt;       /* command ring: main writes q[qh] then qh; ISR reads q[qt] then qt */
    volatile uint32_t lock, ack;              /* main: lock=1 and wait ack==1: the ISR no longer runs the sequencer or reads commands */
    volatile uint32_t pos, mouth, level, act, mora;   /* ISR -> main: playing<<16|row<<8|step; fvoice mouth, level; track activity bits; last sung mora */
} Engine;
void en_init(Engine *e, const Song *g);       /* instruments and sequencer only; does not touch the mailbox */
uint32_t en_post(Engine *e, uint32_t kind, uint32_t a, uint32_t b, uint32_t c);   /* main side; 0 if the ring is full */
void en_render(Engine *e, int32_t *out, uint32_t n);
uint32_t en_lock(Engine *e, uint32_t (*wait)(void));   /* main side. 1 = the ISR has stopped touching the song. wait() is called while waiting and returns 0 to give up */
void en_unlock(Engine *e, uint32_t (*wait)(void));
uint32_t en_pre(uint32_t trk, uint32_t p);    /* the preset that is actually used for p */
#endif

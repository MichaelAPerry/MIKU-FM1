/* SPDX-License-Identifier: GPL-3.0-only */
/* Integer-only FM band: chord/keys (4-voice 2-op PM), mono bass, synthesized drums, tiny mixer.
   No float/libm/libc/heap/64-bit math; 44.1 kHz; envelopes/coefficients once per FMB_BLK frames.
   Every *_render ADDS stereo interleaved Q15 (L R) into an int32 buffer; finish with mx_limit(). */
#ifndef FMBAND_H
#define FMBAND_H
#include <stdint.h>
#define FMB_BLK 16
#define CH_VOICES 4
#define CH_NPRESET 8
#define BS_NPRESET 6
#define DR_NPRESET 4
typedef struct { uint32_t e; uint8_t st; } FbEnv;   /* Q24 envelope; st: 0 idle 1 attack 2 decay 3 sustain 4 release */
/* times are indices 0..63 into fmb_tms[] (2 ms .. 8 s); sus/isus 0..255; ratio Q8 (mod:carrier); ibase = peak index, Q8 radians */
typedef struct {
    uint16_t ratio, ibase, lvl;           /* lvl <= 8192 (4 voices must not overflow) */
    uint8_t wave, atk, dec, sus, rel, idec, isus, ivel, ikt; /* wave 0 sine 1 soft square; ivel: velocity->brightness; ikt: index fall per semitone above C4 (/256) */
    uint8_t detune;                      /* optional second carrier, cents 0..24; 0 = original single carrier */
} ChPreset;
typedef struct {
    FbEnv env;
    uint32_t pc, pm, ic, im, ienv, age, pu, iu;
    int32_t a, da, ik, dik, vg, ib;
    uint8_t note;
} ChVoice;
typedef struct {
    ChPreset p;                           /* copy of the selected preset: tweak fields freely */
    ChVoice v[CH_VOICES];
    uint32_t age;
    int32_t gl, gr;
    uint8_t left, preset;
} ChSynth;
void ch_init(ChSynth *s);
void ch_set_preset(ChSynth *s, uint8_t n);            /* 0 bells 1 EP 2 chip arp 3 pad 4 organ 5 pluck 6 marimba 7 clav */
void ch_set_modern(ChSynth *s, uint8_t n);            /* opt-in profiles: 0 digital pluck, 1 glass pad; not song preset indices */
void ch_set_mix(ChSynth *s, uint8_t level, uint8_t pan);
void ch_note_on(ChSynth *s, uint8_t note, uint8_t vel);
void ch_note_off(ChSynth *s, uint8_t note);
void ch_all_off(ChSynth *s);                          /* release everything */
void ch_render(ChSynth *s, int32_t *out, uint32_t frames);
uint8_t ch_active(const ChSynth *s);                  /* sounding voices */

typedef struct {
    uint16_t ratio, ibase, lvl, cut, cmod, res;
    uint8_t wave, atk, dec, sus, rel, idec, isus, porta, cdec;  /* wave 0 sine 1 soft square 2 saw+resonant SVF */
    uint8_t drop, pdec, drive;            /* pitch drop in quarter-semitones, its decay index, saturation 0..127; all 0 bypass */
} BsPreset;
typedef struct {
    BsPreset p;
    FbEnv env;
    uint32_t pc, pm, ic, ienv, cenv, penv;
    int32_t pcur, ptgt, a, da, ik, dik, f, df, vg, ib, lp, bp, gl, gr;
    uint8_t acc, left, preset;
} BsSynth;
void bs_init(BsSynth *s);
void bs_set_preset(BsSynth *s, uint8_t n);            /* 0 FM bass 1 sub 2 chip square 3 acid 4 sub drop 5 hard acid */
void bs_set_mix(BsSynth *s, uint8_t level, uint8_t pan);
void bs_note_on(BsSynth *s, uint8_t note, uint8_t vel, uint8_t accent, uint8_t slide); /* slide while gated: glide, no retrigger */
void bs_note_off(BsSynth *s);
void bs_render(BsSynth *s, int32_t *out, uint32_t frames);
uint8_t bs_active(const BsSynth *s);

enum { DR_KICK, DR_SNARE, DR_CLAP, DR_CHH, DR_OHH, DR_PERC, DR_N };
typedef struct {                          /* times are fmb_tms indices; frequencies are phase increments; *_lv Q15 base level */
    uint32_t k_f, k_sw, s_f, p_f, p_sw;
    uint16_t k_lv, s_lv, s_tl, s_nl, s_khp, s_klp, c_lv, c_klp, c_khp, h_lv, h_khp, p_lv, p_ratio, p_idx;
    uint8_t k_rsw, k_ra, k_cl, k_rc, k_wv, s_rt, s_rn, s_wv, hold, c_rb, c_rt, c_sp, h_rc, h_ro, h_noise, p_rsw, p_ra, p_ri, p_wv;
} DrPreset;                               /* wv: 0 sine 1 triangle 2 square; hold: noise sample-and-hold length */
typedef struct {
    DrPreset p;
    uint32_t rng, kph, kinc, kae, kse, kce, sph, ste, sne, cne, hae, hao, pph, ppm, pinc, pae, pse, pie, hph[6];
    int32_t kvg, svg, cvg, hvg, ovg, pvg, ka, kda, kca, kdca, ta, dta, na, dna, ca, dca, ha, dha, oa, doa, pa, dpa, pik, pdik, pikb;
    int32_t sn1, snlp, cl1, cl2, hlp, hn, gl, gr;
    uint8_t cbl, cnb, choke, hc, left, preset;
} DrKit;
void dr_init(DrKit *d);
void dr_set_preset(DrKit *d, uint8_t n);              /* 0 808-ish, 1 chip/8-bit, 2 punch, 3 digital */
void dr_set_mix(DrKit *d, uint8_t level, uint8_t pan);
void dr_trig(DrKit *d, uint8_t inst, uint8_t vel);    /* closed hat chokes the open hat */
void dr_render(DrKit *d, int32_t *out, uint32_t frames);
uint8_t dr_active(const DrKit *d);

void mx_gain(uint8_t level, uint8_t pan, int32_t *gl, int32_t *gr);   /* level 0..127, pan 0..127 (64 centre), Q15 */
void mx_limit(int32_t *buf, uint32_t frames);         /* in place: soft knee from 3/4 full scale, hard cap +-32767 */
#endif

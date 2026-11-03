/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_env.h"
/* 4-voice 2-operator FM (phase modulation): modulator sine -> carrier phase offset.
   Per voice and sample: 2 multiplies (index, amplitude); amplitude and index are ramped linearly across each
   16-frame block (adds only). ADSR + index envelope (decay toward isus) + velocity/key tracking run per block.
   Steal order: free, else oldest in release, else oldest. */
static const ChPreset pre[CH_NPRESET] = {
    /* ratio ibase lvl  wave atk dec sus rel idec isus ivel ikt */
    { 896, 640, 6000, 0, 0, 56, 0, 36, 44, 20, 140, 3 },     /* 0 FM bells: 1:3.5, sparkle falls away fast */
    { 256, 560, 6500, 0, 1, 50, 0, 34, 36, 40, 200, 2 },     /* 1 EP: 1:1, soft tine from the index decay */
    { 256, 300, 5200, 1, 0, 30, 90, 28, 30, 90, 100, 0 },    /* 2 chip arp: soft-square carrier, short */
    { 512, 180, 5000, 0, 40, 45, 200, 44, 40, 150, 60, 2 },  /* 3 pad: slow attack, gentle */
    { 512, 250, 5200, 0, 3, 20, 255, 18, 20, 255, 0, 0 },    /* 4 organ: 2:1, constant index */
    { 256, 900, 6500, 0, 0, 38, 0, 26, 24, 15, 200, 3 },     /* 5 pluck: bright burst, quick damp */
    { 1024, 400, 6500, 0, 0, 40, 0, 28, 28, 0, 100, 4 },     /* 6 marimba: 4:1 */
    { 768, 560, 6000, 0, 0, 30, 80, 26, 26, 60, 180, 3 },    /* 7 clav: 3:1 */
};
static void cpy(void *d, const void *s, uint32_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; }
static void zero(void *p, uint32_t n) { for (uint8_t *b = (uint8_t *)p, *e = b + n; b < e; b++) *b = 0; }
void ch_init(ChSynth *s) { zero(s, sizeof *s); ch_set_preset(s, 0); ch_set_mix(s, 100, 64); }
void ch_set_preset(ChSynth *s, uint8_t n) { s->preset = n >= CH_NPRESET ? 0 : n; cpy(&s->p, &pre[s->preset], sizeof s->p); }
void ch_set_mix(ChSynth *s, uint8_t level, uint8_t pan) { mx_gain(level, pan, &s->gl, &s->gr); }
void ch_note_on(ChSynth *s, uint8_t note, uint8_t vel) {
    if (note > 127) note = 127;
    int k = -1;
    for (int i = 0; i < CH_VOICES; i++) if (s->v[i].env.st && s->v[i].env.st != 4 && s->v[i].note == note) k = i;   /* retrigger */
    for (int i = CH_VOICES - 1; i >= 0 && k < 0; i--) if (!s->v[i].env.st) k = i;                                    /* free */
    if (k < 0) {                                                       /* oldest released, else oldest */
        for (int i = 0; i < CH_VOICES; i++) if (s->v[i].env.st == 4 && (k < 0 || s->v[i].age < s->v[k].age)) k = i;
        if (k < 0) { k = 0; for (int i = 1; i < CH_VOICES; i++) if (s->v[i].age < s->v[k].age) k = i; }
    }
    ChVoice *v = &s->v[k];
    const ChPreset *p = &s->p;
    v->note = note; v->age = ++s->age;
    v->ic = fmb_inc[note]; v->im = (v->ic >> 8) * p->ratio;
    v->vg = fb_vg(p->lvl > 8192 ? 8192 : p->lvl, vel);
    uint32_t ib = ((uint32_t)p->ibase * 5214u) >> 10;          /* Q8 rad -> index units (65536/ (2^16 * ...) = 5.09/Q8) */
    ib = (ib * (uint32_t)(256 - p->ivel + ((p->ivel * (vel > 127 ? 127 : vel)) >> 7))) >> 8;
    if (note > 60) { int32_t sc = 256 - (int32_t)p->ikt * (note - 60); ib = (ib * (uint32_t)(sc < 32 ? 32 : sc)) >> 8; }
    v->ib = ib > 65535 ? 65535 : (int32_t)ib;
    v->ienv = FB_FULL; v->env.st = 1;
}
void ch_note_off(ChSynth *s, uint8_t note) {
    for (int i = 0; i < CH_VOICES; i++) if (s->v[i].env.st && s->v[i].env.st != 4 && s->v[i].note == note) s->v[i].env.st = 4;
}
void ch_all_off(ChSynth *s) { for (int i = 0; i < CH_VOICES; i++) if (s->v[i].env.st) s->v[i].env.st = 4; }
uint8_t ch_active(const ChSynth *s) { uint8_t n = 0; for (int i = 0; i < CH_VOICES; i++) n += s->v[i].env.st != 0; return n; }
static void block(ChSynth *s) {
    const ChPreset *p = &s->p;
    for (int i = 0; i < CH_VOICES; i++) {
        ChVoice *v = &s->v[i];
        if (!v->env.st) continue;
        if (!fb_step(&v->env, p->atk, p->dec, p->sus, p->rel)) { v->a = v->da = v->ik = v->dik = 0; continue; }
        v->ienv = fb_ap(v->ienv, fb_sus(p->isus), fmb_kdec[p->idec & 63]);
        int32_t ta = (int32_t)(((v->env.e >> 9) * (uint32_t)v->vg) >> 15);
        int32_t tk = (int32_t)(((uint32_t)v->ib * (v->ienv >> 8)) >> 16);
        v->da = (ta - v->a) >> 4; v->dik = (tk - v->ik) >> 4;
    }
}
void ch_render(ChSynth *s, int32_t *out, uint32_t n) {
    const int16_t *w = s->p.wave ? fmb_sq : fmb_sin;
    while (n) {
        if (!s->left) {
            if (!ch_active(s)) return;
            block(s); s->left = FMB_BLK;
        }
        uint32_t m = n < s->left ? n : s->left;
        s->left = (uint8_t)(s->left - m); n -= m;
        int32_t mb[FMB_BLK];
        for (uint32_t f = 0; f < m; f++) mb[f] = 0;
        for (int i = 0; i < CH_VOICES; i++) {
            ChVoice *v = &s->v[i];
            if (!v->env.st) continue;
            uint32_t pc = v->pc, pm = v->pm;
            int32_t a = v->a, ik = v->ik;
            for (uint32_t f = 0; f < m; f++) {
                pc += v->ic; pm += v->im; a += v->da; ik += v->dik;
                uint32_t off = (uint32_t)(fmb_sin[pm >> 22] * ik) << 4;
                mb[f] += (w[(pc + off) >> 22] * a) >> 15;
            }
            v->pc = pc; v->pm = pm; v->a = a; v->ik = ik;
        }
        for (uint32_t f = 0; f < m; f++, out += 2) {
            int32_t x = fb_clamp(mb[f], -32767, 32767);
            out[0] += (x * s->gl) >> 15; out[1] += (x * s->gr) >> 15;
        }
    }
}

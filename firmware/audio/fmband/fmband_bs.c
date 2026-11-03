/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_env.h"
/* Monophonic bass: 2-op FM (sine / soft-square carrier) or naive saw through a Chamberlin SVF low-pass.
   Pitch in Q8 semitones, exponential glide per block (slide while gated = legato, no retrigger).
   Accent: louder, 1.5x index / higher cutoff, more resonance, shorter decays. Per sample: FM 2 mul, acid 4 mul. */
static const BsPreset pre[BS_NPRESET] = {
    /* ratio ibase lvl cut cmod res | wave atk dec sus rel idec isus porta cdec */
    { 256, 800, 20000, 0, 0, 0, 0, 0, 34, 150, 26, 30, 40, 28, 0 },        /* 0 FM bass: punchy 1:1 */
    { 256, 80, 24000, 0, 0, 0, 0, 2, 40, 220, 28, 30, 50, 30, 0 },         /* 1 sub: near-sine */
    { 256, 0, 15000, 0, 0, 0, 1, 0, 26, 160, 22, 0, 0, 24, 0 },            /* 2 chip square */
    { 256, 0, 19000, 4000, 16000, 5500, 2, 0, 30, 200, 22, 0, 0, 30, 32 }, /* 3 acid: saw + resonant SVF */
};
static void cpy(void *d, const void *s, uint32_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; }
static void zero(void *p, uint32_t n) { for (uint8_t *b = (uint8_t *)p, *e = b + n; b < e; b++) *b = 0; }
void bs_init(BsSynth *s) { zero(s, sizeof *s); bs_set_preset(s, 0); bs_set_mix(s, 100, 64); }
void bs_set_preset(BsSynth *s, uint8_t n) { s->preset = n >= BS_NPRESET ? 0 : n; cpy(&s->p, &pre[s->preset], sizeof s->p); }
void bs_set_mix(BsSynth *s, uint8_t level, uint8_t pan) { mx_gain(level, pan, &s->gl, &s->gr); }
uint8_t bs_active(const BsSynth *s) { return s->env.st != 0; }
void bs_note_off(BsSynth *s) { if (s->env.st && s->env.st != 4) s->env.st = 4; }
void bs_note_on(BsSynth *s, uint8_t note, uint8_t vel, uint8_t accent, uint8_t slide) {
    const BsPreset *p = &s->p;
    if (note > 127) note = 127;
    if (vel > 127) vel = 127;
    int legato = slide && s->env.st && s->env.st != 4;
    s->ptgt = (int32_t)note << 8;
    if (!legato) { s->pcur = s->ptgt; s->env.st = 1; s->ienv = FB_FULL; s->cenv = FB_FULL; }
    s->acc = accent != 0;
    int32_t vg = fb_vg(p->lvl > 26000 ? 26000 : p->lvl, vel);
    if (s->acc) vg += vg >> 2;
    s->vg = vg > 32000 ? 32000 : vg;
    uint32_t ib = (((uint32_t)p->ibase * 5214u) >> 10) * (128u + vel) >> 8;
    if (s->acc) ib += ib >> 1;
    s->ib = ib > 65535 ? 65535 : (int32_t)ib;
}
static void block(BsSynth *s) {
    const BsPreset *p = &s->p;
    int32_t d = s->ptgt - s->pcur;
    if (d) {                                                    /* exponential glide, 1/256 semitone floor */
        uint32_t ad = (uint32_t)(d < 0 ? -d : d), st = (ad * fmb_kdec[p->porta & 63]) >> 16;
        if (!st) st = 1;
        s->pcur = st >= ad ? s->ptgt : s->pcur + (d < 0 ? -(int32_t)st : (int32_t)st);
    }
    int32_t n = s->pcur >> 8, fr = s->pcur & 255;
    uint32_t ic = fmb_inc[n] + (((fmb_inc[n + 1] - fmb_inc[n]) >> 8) * (uint32_t)fr);
    s->ic = ic;
    uint8_t ac = s->acc;
    if (!fb_step(&s->env, p->atk, (uint8_t)(p->dec + (ac && p->dec < 57 ? 6 : 0)), p->sus, p->rel)) { s->a = s->da = s->ik = s->dik = 0; return; }
    s->ienv = fb_ap(s->ienv, fb_sus(p->isus), fmb_kdec[(p->idec + (ac && p->idec < 55 ? 8 : 0)) & 63]);
    s->cenv = fb_ap(s->cenv, 0, fmb_kdec[(p->cdec + (ac ? 4 : 0)) & 63]);
    int32_t ta = (int32_t)(((s->env.e >> 9) * (uint32_t)s->vg) >> 15);
    int32_t tk = (int32_t)(((uint32_t)s->ib * (s->ienv >> 8)) >> 16);
    int32_t tf = p->cut + (int32_t)(((uint32_t)p->cmod * (s->cenv >> 8)) >> 16);
    if (ac) tf += tf >> 2;
    tf = fb_clamp(tf, 0, 20000);
    s->da = (ta - s->a) >> 4; s->dik = (tk - s->ik) >> 4; s->df = (tf - s->f) >> 4;
}
void bs_render(BsSynth *s, int32_t *out, uint32_t n) {
    const int16_t *w = s->p.wave == 1 ? fmb_sq : fmb_sin;
    while (n) {
        if (!s->left) {
            if (!s->env.st) return;
            block(s); s->left = FMB_BLK;
        }
        uint32_t m = n < s->left ? n : s->left;
        s->left = (uint8_t)(s->left - m); n -= m;
        int32_t mb[FMB_BLK];
        if (s->env.st) {
            uint32_t ic = s->ic, im = (ic >> 8) * s->p.ratio, pc = s->pc, pm = s->pm;
            int32_t a = s->a, ik = s->ik, f = s->f, lp = s->lp, bp = s->bp;
            int32_t q = s->acc ? s->p.res - (s->p.res >> 2) : s->p.res;
            for (uint32_t i = 0; i < m; i++) {
                pc += ic; a += s->da;
                int32_t o;
                if (s->p.wave == 2) {                           /* saw -> Chamberlin SVF low-pass */
                    int32_t x = (int32_t)(pc >> 17) - 16384; x += x >> 1; f += s->df;
                    lp = fb_clamp(lp + ((f * bp) >> 15), -65535, 65535);
                    int32_t hp = fb_clamp(x - lp - ((q * bp) >> 15), -65535, 65535);
                    bp = fb_clamp(bp + ((f * hp) >> 15), -65535, 65535);
                    o = fb_clamp(lp, -32767, 32767);
                } else {
                    pm += im; ik += s->dik;
                    o = w[(pc + ((uint32_t)(fmb_sin[pm >> 22] * ik) << 4)) >> 22];
                }
                mb[i] = (o * a) >> 15;
            }
            s->pc = pc; s->pm = pm; s->a = a; s->ik = ik; s->f = f; s->lp = lp; s->bp = bp;
        } else for (uint32_t i = 0; i < m; i++) mb[i] = 0;
        for (uint32_t i = 0; i < m; i++, out += 2) {
            int32_t x = fb_clamp(mb[i], -32767, 32767);
            out[0] += (x * s->gl) >> 15; out[1] += (x * s->gr) >> 15;
        }
    }
}

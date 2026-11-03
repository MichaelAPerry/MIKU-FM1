/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/audio/fvoice/fvoice_tables.h"
/* Glottal-pulse wavetable -> brightness one-pole -> 3 formant + 1 presence parallel
   2-pole resonators (alternating sign) -> envelope gain. All 32-bit; coefficient, LFO,
   glide, envelope and vowel-morph work runs once per FV_BLK frames.
   Morph: coefficients move by a convex combination (cur += (tgt-cur)*k, 0<k<1) and the
   2nd-order stability region {|a2|<1, |a1|<1-a2} is convex, so every in-between filter is stable. */
#define PMAX 45055
static void quiet(FvVoice *v) { v->lp = 0; for (int i = 0; i < 4; i++) v->y1[i] = v->y2[i] = 0; }
static int32_t clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static void load(FvVoice *v, int snap) {
    for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) {
        int32_t t = (int32_t)fv_vow[v->vowel][i][j] * 16;
        v->cur[i][j] = snap ? t : v->cur[i][j] + ((((t - v->cur[i][j]) >> 4) * FV_KM) >> 8);
    }
}
void fv_init(FvVoice *v) {
    for (uint8_t *b = (uint8_t *)v, *e = b + sizeof *v; b < e; b++) *b = 0;
    v->vib_rate = 55; v->vib_delay = 150; v->porta = 60; v->attack = 8; v->release = 60;
    v->breath = 6; v->bright = 90; v->noise = 0x12345678u; v->velg = 100;
    load(v, 1);
}
void fv_set_vowel(FvVoice *v, uint8_t vw) { v->vowel = vw > FV_O ? FV_O : vw; }
void fv_note_on(FvVoice *v, uint8_t note, uint8_t vw, uint8_t vel) {
    int32_t p = fv_notep[note > 127 ? 127 : note];
    int sounding = v->gated || v->env > 0;
    fv_set_vowel(v, vw);
    if (!sounding) { v->pcur = p; quiet(v); load(v, 1); }
    if (!v->gated) { v->vt = 0; v->lfo = 0; }
    v->ptgt = p; v->gated = 1;
    v->velg = (uint8_t)(vel > 127 ? 127 : vel);
}
void fv_note_off(FvVoice *v) { v->gated = 0; }
static void block(FvVoice *v) {
    /* vibrato: sine LFO in pitch units, delayed then faded in over 256 blocks */
    v->lfo += (uint32_t)FV_LFOK * v->vib_rate;
    if (v->vt < 65535) v->vt++;
    int32_t gv = clampi((int32_t)v->vt - ((v->vib_delay * 2822) >> 10), 0, 256);
    int32_t vib = (((int32_t)fv_sin[v->lfo >> 24] * ((v->vib_depth * 873) >> 8)) >> 15) * gv >> 8;
    /* portamento: exponential glide in the log-pitch domain */
    int32_t d = v->ptgt - v->pcur, st = (d * (int32_t)fv_kpor[v->porta > 510 ? 255 : v->porta >> 1]) >> 15;
    if (!st) st = (d > 0) - (d < 0);
    v->pcur += st;
    int32_t p = clampi(v->pcur + vib, 0, PMAX);
    int32_t i = (p >> 4) & 255;
    v->inc = (uint32_t)(fv_inc[i] + (((int32_t)(fv_inc[i + 1] - fv_inc[i]) * (p & 15)) >> 4)) << (p >> 12);
    load(v, 0);
    for (int a = 0; a < 4; a++) { v->r[a].g = v->cur[a][0] >> 4; v->r[a].a1 = v->cur[a][1] >> 4; v->r[a].a2 = v->cur[a][2] >> 4; }
    /* envelope: linear attack, exponential release (floor of 1/block ends the tail) */
    if (v->gated) { if (v->env < 32767) { v->env += fv_katk[v->attack > 510 ? 255 : v->attack >> 1]; if (v->env > 32767) v->env = 32767; } }
    else if (v->env > 0) {
        uint32_t k = ((uint32_t)v->env * fv_krel[v->release > 510 ? 255 : v->release >> 1]) >> 16;
        v->env -= k ? (int32_t)k : 1;
    }
    int32_t tg = (v->env * (8192 + v->velg * 193)) >> 15, df = tg - v->gcur;
    if (df > -16 && df < 16) { v->gcur = tg; v->gstep = 0; } else v->gstep = df >> 4;
}
void fv_render(FvVoice *v, int32_t *out, uint32_t n) {
    while (n) {
        if (!v->left) {
            if (!v->gated && !v->env && !v->gcur) { quiet(v); return; }
            block(v); v->left = FV_BLK;
        }
        uint32_t m = n < v->left ? n : v->left;
        v->left = (uint8_t)(v->left - m); n -= m;
        int32_t kb = 3000 + v->bright * 230, bg = v->breath;
        uint32_t ph = v->ph, inc = v->inc, ns = v->noise;
        int32_t lp = v->lp, g = v->gcur;
        for (uint32_t f = 0; f < m; f++, out += 2) {
            ph += inc;
            int32_t a = fv_glot[ph >> 24], s = (a + (((fv_glot[(ph >> 24) + 1] - a) * (int32_t)((ph >> 16) & 255)) >> 8)) >> 2;
            lp += ((s - lp) * kb) >> 15;
            ns ^= ns << 13; ns ^= ns >> 17; ns ^= ns << 5;
            int32_t x = lp + (((int16_t)(ns >> 16) * bg) >> 10), sum = 0;
            for (int q = 0; q < 4; q++) {
                int32_t y = (v->r[q].g * x + v->r[q].a1 * v->y1[q] + v->r[q].a2 * v->y2[q]) >> 14;
                y = clampi(y, -32767, 32767);
                v->y2[q] = v->y1[q]; v->y1[q] = y; sum += y;
            }
            g += v->gstep;
            int32_t o = (clampi(sum, -32767, 32767) * g) >> 15;
            out[0] += o; out[1] += o;
        }
        v->ph = ph; v->noise = ns; v->lp = lp; v->gcur = g;
    }
}
uint8_t fv_level(const FvVoice *v) { return (uint8_t)(v->env >> 7); }
uint8_t fv_mouth(const FvVoice *v) { return v->env < 768 ? 5 : v->vowel; }

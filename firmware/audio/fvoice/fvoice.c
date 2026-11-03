/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/audio/fvoice/fvoice_tables.h"
/* Sharp-closure glottal wavetable -> pre-emphasis -> brightness one-pole (+ glottal-gated breath noise)
   -> 6 parallel 2-pole resonators (F1, nasal, F2, F3, singer's formant, air; alternating sign) -> gain.
   All 32-bit; coefficient, LFO, glide, envelope, vowel-morph and "human" work runs once per FV_BLK frames.
   Morph: coefficients move by a convex combination (cur += (tgt-cur)*k, 0<k<1) and the
   2nd-order stability region {|a2|<1, |a1|<1-a2} is convex, so every in-between filter is stable.
   The high-pitch zone table is blended in by the same convex rule. */
#define PMAX 45055
#define P_C5 24576              /* pitch units of MIDI 72 */
static void quiet(FvVoice *v) { v->lp = v->sp = v->np = 0; for (int i = 0; i < FV_NR; i++) v->y1[i] = v->y2[i] = 0; }
static int32_t clampi(int32_t x, int32_t lo, int32_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static uint32_t xs(uint32_t x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
static int32_t decay(int32_t x, int32_t k) {   /* x *= 1-k/32768, never sticks, never crosses zero */
    int32_t d = ((x < 0 ? -x : x) * k) >> 15; if (!d) d = 1;
    return x > 0 ? x - d : x < 0 ? x + d : 0;
}
static void load(FvVoice *v, int snap, int wz) {
    for (int i = 0; i < FV_NR; i++) for (int j = 0; j < 3; j++) {
        int32_t t0 = fv_vow[0][v->vowel][i][j], t1 = fv_vow[1][v->vowel][i][j];
        int32_t t = (t0 + (((t1 - t0) * wz) >> 4)) * 16;
        v->cur[i][j] = snap ? t : v->cur[i][j] + ((((t - v->cur[i][j]) >> 4) * FV_KM) >> 8);
    }
}
void fv_init(FvVoice *v) {
    for (uint8_t *b = (uint8_t *)v, *e = b + sizeof *v; b < e; b++) *b = 0;
    v->vib_rate = 55; v->vib_delay = 150; v->porta = 60; v->attack = 8; v->release = 60;
    v->breath = 6; v->bright = 100; v->noise = 0x12345678u; v->rng = 0x9E3779B9u; v->velg = 100;
    v->human = 80; v->scoop = 40; v->scoop_ms = 75;
    load(v, 1, 0);
}
void fv_set_vowel(FvVoice *v, uint8_t vw) { v->vowel = vw > FV_O ? FV_O : vw; }
void fv_note_on(FvVoice *v, uint8_t note, uint8_t vw, uint8_t vel) {
    int32_t n = (int32_t)(note > 127 ? 127 : note) + v->transpose;
    int32_t p = fv_notep[clampi(n, 0, 127)];
    int sounding = v->gated || v->env > 0;
    fv_set_vowel(v, vw);
    if (!sounding) { v->pcur = p; quiet(v); load(v, 1, clampi((p - P_C5) >> 8, 0, 16)); }
    if (!v->gated) {
        v->vt = 0; v->lfo = 0; v->ovt = v->ovo = v->ova = 0; v->ovd = 0;
        v->sc = -(((int32_t)(v->scoop > 100 ? 100 : v->scoop) * 873) >> 8);   /* onset scoop from below */
    } else {                                    /* legato leap: arm a small overshoot that fires as the glide arrives */
        int32_t d = p - v->pcur, m = d < 0 ? -d : d;
        if (m >= 1024 && v->human) {
            m = clampi(m >> 6, 0, 60) * v->human >> 7;
            v->ova = d < 0 ? -m : m; v->ovd = (uint16_t)((v->porta * 1693) >> 10); v->ovt = 0;
            if (!v->ovd) { v->ovt = v->ova; v->ova = 0; }
        }
    }
    v->ptgt = p; v->gated = 1;
    v->velg = (uint8_t)(vel > 127 ? 127 : vel);
}
void fv_note_off(FvVoice *v) { v->gated = 0; }
static void block(FvVoice *v) {
    int32_t h = v->human;
    /* deterministic slow drift / jitter / shimmer: three smoothed LFSR random walks */
    v->rng = xs(v->rng); v->dr[0] += ((int32_t)(int16_t)(v->rng >> 16) - v->dr[0]) >> 8;
    v->rng = xs(v->rng); v->dr[1] += ((int32_t)(int16_t)(v->rng >> 16) - v->dr[1]) >> 5;
    v->rng = xs(v->rng); v->dr[2] += ((int32_t)(int16_t)(v->rng >> 16) - v->dr[2]) >> 5;
    int32_t jit = clampi(((v->dr[0] * h) >> 14) + ((v->dr[1] * h) >> 16), -34, 34);
    int32_t wob = (v->dr[0] * h) >> 14, dwb = (v->dr[1] * h) >> 14;   /* vibrato rate / depth wander */
    /* vibrato: sine LFO in pitch units, delayed then faded in over 512 blocks (~190 ms) */
    v->lfo += (uint32_t)(((uint32_t)FV_LFOK * v->vib_rate) >> 8) * (uint32_t)(256 + wob);
    if (v->vt < 65535) v->vt++;
    int32_t gv = clampi((int32_t)v->vt - ((v->vib_delay * 2822) >> 10), 0, 512);
    int32_t vd = (((int32_t)v->vib_depth * 873) >> 8) * (256 + dwb) >> 8;
    int32_t vib = (((int32_t)fv_sin[v->lfo >> 24] * vd) >> 15) * gv >> 9;
    /* portamento: exponential glide in the log-pitch domain */
    int32_t d = v->ptgt - v->pcur, st = (d * (int32_t)fv_kpor[v->porta > 510 ? 255 : v->porta >> 1]) >> 15;
    if (!st) st = (d > 0) - (d < 0);
    v->pcur += st;
    /* onset scoop and leap overshoot */
    if (v->sc) v->sc = decay(v->sc, fv_kpor[v->scoop_ms > 510 ? 255 : v->scoop_ms >> 1]);
    if (v->ovd && !--v->ovd) { v->ovt = v->ova; v->ova = 0; }
    if (v->ovt) v->ovt = decay(v->ovt, fv_kpor[20]);
    v->ovo += (v->ovt - v->ovo) >> 2;
    int32_t p = clampi(v->pcur + vib + jit + v->sc + v->ovo, 0, PMAX);
    int32_t i = (p >> 4) & 255;
    v->inc = (uint32_t)(fv_inc[i] + (((int32_t)(fv_inc[i + 1] - fv_inc[i]) * (p & 15)) >> 4)) << (p >> 12);
    int32_t wz = clampi((v->pcur - P_C5) >> 8, 0, 16);
    load(v, 0, wz);
    for (int a = 0; a < FV_NR; a++) { v->r[a].g = v->cur[a][0] >> 4; v->r[a].a1 = v->cur[a][1] >> 4; v->r[a].a2 = v->cur[a][2] >> 4; }
    /* envelope: linear attack, exponential release (floor of 1/block ends the tail) */
    if (v->gated) { if (v->env < 32767) { v->env += fv_katk[v->attack > 510 ? 255 : v->attack >> 1]; if (v->env > 32767) v->env = 32767; } }
    else if (v->env > 0) {
        uint32_t k = ((uint32_t)v->env * fv_krel[v->release > 510 ? 255 : v->release >> 1]) >> 16;
        v->env -= k ? (int32_t)k : 1;
    }
    int32_t tg = (v->env * (8192 + v->velg * 193)) >> 16;                 /* half scale: pgain carries the other 6 dB */
    tg = (tg * (32768 + ((v->dr[2] * h) >> 9))) >> 15;                    /* shimmer */
    tg = clampi((tg * fv_pgain[clampi((v->pcur * 3) >> 10, 0, 127)]) >> 12, 0, 49152);
    int32_t df = tg - v->gcur;
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
        /* brightness and pre-emphasis fall with pitch above C5 (less aliasing, no harshness up high) */
        int32_t sw = clampi((v->pcur - P_C5) >> 8, 0, 32);
        int32_t mx = clampi((v->pcur - P_C5) >> 7, 0, 32);   /* sharp -> soft pulse crossfade */
        int32_t kb = 8000 + v->bright * 190, bg = v->breath, pe = v->bright >> 2;
        kb -= (kb * sw) >> 7; pe -= (pe * sw) >> 6;
        uint32_t ph = v->ph, inc = v->inc, ns = v->noise;
        int32_t lp = v->lp, g = v->gcur, sp = v->sp, np = v->np;
        for (uint32_t f = 0; f < m; f++, out += 2) {
            ph += inc;
            uint32_t gi = ph >> 24; int32_t fr = (int32_t)((ph >> 16) & 255);
            int32_t a = fv_glot[gi], s = a + (((fv_glot[gi + 1] - a) * fr) >> 8);
            int32_t b2 = fv_glot2[gi], s2 = b2 + (((fv_glot2[gi + 1] - b2) * fr) >> 8);
            s = (s + (((s2 - s) * mx) >> 5)) >> 2;
            lp += ((s + (((s - sp) * pe) >> 6) - lp) * kb) >> 15; sp = s;
            ns ^= ns << 13; ns ^= ns >> 17; ns ^= ns << 5;
            int32_t nz = ((int16_t)(ns >> 16) * (a < 0 ? -a : a)) >> 15;   /* noise gated by the glottal phase, */
            int32_t x = lp + (((nz - np) * bg) >> 9), sum = 0; np = nz;    /* first-difference high-passed */
            for (int q = 0; q < FV_NR; q++) {
                int32_t y = (v->r[q].g * x + v->r[q].a1 * v->y1[q] + v->r[q].a2 * v->y2[q]) >> 14;
                y = clampi(y, -32767, 32767);
                v->y2[q] = v->y1[q]; v->y1[q] = y; sum += y;
            }
            g += v->gstep;
            int32_t o = clampi((clampi(sum, -32767, 32767) * g) >> 15, -32767, 32767);
            out[0] += o; out[1] += o;
        }
        v->ph = ph; v->noise = ns; v->lp = lp; v->gcur = g; v->sp = sp; v->np = np;
    }
}
uint8_t fv_level(const FvVoice *v) { return (uint8_t)(v->env >> 7); }
uint8_t fv_mouth(const FvVoice *v) { return v->env < 768 ? 5 : v->vowel; }

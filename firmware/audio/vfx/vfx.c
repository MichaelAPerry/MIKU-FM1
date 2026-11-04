/* SPDX-License-Identifier: GPL-3.0-only */
/* Voice effects (see vfx.h). Levels: the voice is Q15 (+-32767); every stage keeps its output inside that.
 * drive    gain up to 9x into a cubic soft clip (y = 1.5u - 0.5u^3), then 20/(gain*16+4) back down (about level loudness)
 * tremolo  5 Hz parabolic-sine amplitude LFO
 * chorus   two taps around 13 ms, +-2.5 ms at 0.9 Hz in opposite phase (left / right), linear interpolation
 * delay    dotted eighth at the song tempo (halved until it fits), feedback 0.18..0.46; echoes alternate left / right
 * reverb   four damped combs and two all-passes per side (Freeverb's structure, shorter), fed with the mono sum */
#include "firmware/audio/vfx/vfx.h"
#ifndef VX_POOL
#define VX_POOL
#endif
#define CN 2048u                    /* chorus line, 46 ms */
#define DN 32768u                   /* delay line, 743 ms */
static const uint16_t CL[4] = { 1116, 1188, 1277, 1356 }, AL[4] = { 556, 441, 579, 464 };
#define RN (1116u + 1188u + 1277u + 1356u + 556u + 441u + 579u + 464u)
static int16_t cb[CN] VX_POOL, db[DN] VX_POOL, rb[RN] VX_POOL;
static uint32_t cw, dw, ci[4], ai[4], lph, cph, on;
static int32_t cf[4];
static int32_t sat(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }
static int32_t psin(uint32_t ph) {  /* parabolic sine of a 32-bit phase, Q15 */
    int32_t x = (int32_t)(ph >> 16) - 32768, a = x < 0 ? -x : x;
    return sat(-((x * (32768 - a)) >> 13));
}
static void clr(int16_t *b, uint32_t n) { while (n--) *b++ = 0; }
static int32_t mz(int32_t x, int32_t g) { int32_t p = x * g; return (p + (p < 0 ? 32767 : 0)) >> 15; }   /* x*g/32768 toward zero: loops with gain < 1 die out */
void vx_init(void) {
    uint32_t k;
    clr(cb, CN); clr(db, DN); clr(rb, RN);
    cw = dw = lph = cph = on = 0;
    for (k = 0; k < 4; k++) { ci[k] = ai[k] = 0; cf[k] = 0; }
}
static int32_t chtap(uint32_t w, int32_t d8) {   /* delay d8 (Q8 samples) behind write position w */
    uint32_t i = (w - (uint32_t)(d8 >> 8)) & (CN - 1u), j = (i - 1u) & (CN - 1u);
    int32_t a = cb[i], b = cb[j];
    return a + (((b - a) * (d8 & 255)) >> 8);
}
void vx_render(int32_t *st, uint32_t n, const VxSet *s, uint32_t bpm) {
    uint32_t f, k, now = (s->cho ? 1u : 0) | (s->dly ? 2u : 0) | (s->rev ? 4u : 0);
    int32_t g16 = 16 + (s->drv * 13) / 10, comp = 81920 / (g16 + 4), td = s->trem * 327, wc = s->cho * 164, wd = s->dly * 200, wr = s->rev * 150;
    int32_t fb = 6000 + s->dly * 90;
    uint32_t dd = bpm >= 20u ? 33075u * 60u / bpm : DN;   /* dotted eighth: 0.75 beat */
    if (!s->drv && !s->trem && !now) return;
    while (dd > DN / 2u - 1u) dd >>= 1;
    if (now & ~on & 1u) clr(cb, CN);
    if (now & ~on & 2u) clr(db, DN);
    if (now & ~on & 4u) { clr(rb, RN); for (k = 0; k < 4; k++) cf[k] = 0; }
    on = now;
    for (f = 0; f < n; f++, st += 2) {
        int32_t x = st[0], l, r;
        if (s->drv) {
            int32_t u = sat((x * g16) >> 4), t = (u * u) >> 15;
            x = (((3 * u - ((t * u) >> 15)) >> 1) * comp) >> 12;
        }
        if (td) { lph += 486958u; x = (x * (32768 - ((td * ((32768 + psin(lph)) >> 1)) >> 15))) >> 15; }
        l = r = x;
        if (s->cho) {
            int32_t y;
            cph += 87666u;                                 /* 0.9 Hz */
            y = psin(cph);
            cb[cw] = (int16_t)x;
            l = sat(x + ((chtap(cw, 573 * 256 + ((y * 110) >> 7)) * wc) >> 15));
            r = sat(x + ((chtap(cw, 573 * 256 - ((y * 110) >> 7)) * wc) >> 15));
            cw = (cw + 1u) & (CN - 1u);
        }
        if (s->dly) {
            int32_t e1 = db[(dw - dd) & (DN - 1u)], e2 = db[(dw - 2u * dd) & (DN - 1u)];
            db[dw] = (int16_t)sat(((l + r) >> 1) + mz(e1, fb));
            dw = (dw + 1u) & (DN - 1u);
            l = sat(l + ((e1 * wd) >> 15)); r = sat(r + (((e2 - mz(e1, fb)) * wd) >> 15));
        }
        if (s->rev) {
            int16_t *p = rb;
            int32_t in = (l + r) >> 3, sum = 0, ol, orr;
            for (k = 0; k < 4; k++) {                      /* combs: feedback 0.80, damping 0.25 */
                int32_t o = p[ci[k]];
                cf[k] = mz(o, 24576) + mz(cf[k], 8192);
                p[ci[k]] = (int16_t)sat(in + mz(cf[k], 26214));
                if (++ci[k] >= CL[k]) ci[k] = 0;
                sum += o; p += CL[k];
            }
            ol = orr = sum >> 1;
            for (k = 0; k < 4; k++) {                      /* all-passes, gain 0.5: two for the left, two for the right */
                int32_t *v = k < 2 ? &ol : &orr, b = p[ai[k]];
                p[ai[k]] = (int16_t)sat(*v + mz(b, 16384));
                *v = b - *v;
                if (++ai[k] >= AL[k]) ai[k] = 0;
                p += AL[k];
            }
            l = sat(l + ((ol * wr) >> 15)); r = sat(r + ((orr * wr) >> 15));
        }
        st[0] = l; st[1] = r;
    }
}

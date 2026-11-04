/* SPDX-License-Identifier: GPL-3.0-only */
/* Voice effects (see vfx.h). Levels: the voice is Q15 (+-32767); every stage keeps its output inside that.
 * Knob curves: the wet levels and the drive gain follow the square of the knob (0..100), the rest is linear.
 * drive    gain up to 9x into a cubic soft clip (y = 1.5u - 0.5u^3), then 20/(gain*16+4) back down (about level loudness)
 * tremolo  5 Hz parabolic-sine amplitude LFO
 * chorus   two taps around 13 ms, +-2.5 ms at 0.9 Hz in opposite phase (left / right), linear interpolation
 * delay    1/16 .. 1/4 at the song tempo (halved until it fits), feedback 0.18..0.73; the right echo 15 ms after the left
 * reverb   four damped combs (feedback 0.70..0.94: size) and two all-passes per side (Freeverb's structure, shorter), at half
 *          rate (every other sample, linearly interpolated in between: half the cost; the tail is dark anyway) */
#include "firmware/audio/vfx/vfx.h"
#ifndef VX_POOL
#define VX_POOL
#endif
#define CN 2048u                    /* chorus line, 46 ms */
#define DN 65536u                   /* delay line, 1.49 s */
#define DS 661u                     /* the right channel's echo comes 15 ms after the left one */
static const uint16_t CL[4] = { 558, 594, 638, 678 }, AL[4] = { 278, 220, 290, 232 };   /* at 22.05 kHz (the reverb runs at half rate) */
#define RN (558u + 594u + 638u + 678u + 278u + 220u + 290u + 232u)
static int16_t cb[CN] VX_POOL, db[DN] VX_POOL, rb[RN] VX_POOL;
static uint32_t cw, dw, ci[4], ai[4], lph, cph, on, rph, tct, cct;   /* rph: the reverb's half-rate clock; tct, cct: LFO update counters */
static int32_t tg = 32768, cy;              /* tremolo gain, chorus LFO value: updated every 16 / 8 frames */
static int32_t cf[4], rvl, rvr, rpl, rpr;   /* reverb: damping states; the last two outputs (left, right) */
static int32_t sat(int32_t x) { return x > 32767 ? 32767 : x < -32767 ? -32767 : x; }
static int32_t psin(uint32_t ph) {  /* parabolic sine of a 32-bit phase, Q15 */
    int32_t x = (int32_t)(ph >> 16) - 32768, a = x < 0 ? -x : x;
    return sat(-((x * (32768 - a)) >> 13));
}
static void clr(int16_t *b, uint32_t n) { while (n--) *b++ = 0; }
static int32_t mz(int32_t x, int32_t g) { int32_t p = x * g; return (p + ((p >> 31) & 32767)) >> 15; }   /* x*g/32768 toward zero: loops with gain < 1 die out */
void vx_init(void) {
    uint32_t k;
    clr(cb, CN); clr(db, DN); clr(rb, RN);
    cw = dw = lph = cph = on = 0;
    for (k = 0; k < 4; k++) { ci[k] = ai[k] = 0; cf[k] = 0; }
    rvl = rvr = rpl = rpr = 0; tct = cct = 0; tg = 32768; cy = 0;
}
static int32_t chtap(uint32_t w, int32_t d8) {   /* delay d8 (Q8 samples) behind write position w */
    uint32_t i = (w - (uint32_t)(d8 >> 8)) & (CN - 1u), j = (i - 1u) & (CN - 1u);
    int32_t a = cb[i], b = cb[j];
    return a + (((b - a) * (d8 & 255)) >> 8);
}
void vx_render(int32_t *st, uint32_t n, const VxSet *s, uint32_t bpm) {
    uint32_t f, k, now = (s->cho ? 1u : 0) | (s->dly ? 2u : 0) | (s->rev ? 4u : 0);
    int32_t q = s->drv * s->drv, g16 = 16 + (q * 13) / 1000, comp = 81920 / (g16 + 4), td = s->trem * 327;   /* knob curves: squared (fine low end) */
    int32_t wc = s->cho * s->cho * 164 / 100, wd = s->dly * s->dly * 2, wr = s->rev * s->rev * 3 / 2;
    static const uint8_t dn[5] = { 1, 1, 1, 3, 1 }, dv[5] = { 4, 3, 2, 4, 1 };   /* delay time in beats: 1/4 1/3 1/2 3/4 1 */
    uint32_t di = s->dtime * 5u / 101u, dd = bpm >= 20u ? 2646000u / bpm * dn[di] / dv[di] : DN;
    int32_t fb = 6000 + s->dfb * 180, rfb = 22938 + s->rsize * 78;
    uint32_t cinc = 9740u + s->crate * 3896u;
    if (!s->drv && !s->trem && !now) return;
    while (dd > DN - DS - 1u) dd >>= 1;
    if (now & ~on & 1u) clr(cb, CN);
    if (now & ~on & 2u) clr(db, DN);
    if (now & ~on & 4u) { clr(rb, RN); for (k = 0; k < 4; k++) cf[k] = 0; rvl = rvr = rpl = rpr = 0; }
    on = now;
    for (f = 0; f < n; f++, st += 2) {
        int32_t x = st[0], l, r;
        if (s->drv) {
            int32_t u = sat((x * g16) >> 4), t = (u * u) >> 15;
            x = (((3 * u - ((t * u) >> 15)) >> 1) * comp) >> 12;
        }
        if (td) {
            if (!(tct++ & 15u)) { lph += 16u * 486958u; tg = 32768 - ((td * ((32768 + psin(lph)) >> 1)) >> 15); }
            x = (x * tg) >> 15;
        }
        l = r = x;
        if (s->cho) {
            if (!(cct++ & 7u)) { cph += 8u * cinc; cy = (psin(cph) * 110) >> 7; }
            cb[cw] = (int16_t)x;
            l = sat(x + ((chtap(cw, 573 * 256 + cy) * wc) >> 15));
            r = sat(x + ((chtap(cw, 573 * 256 - cy) * wc) >> 15));
            cw = (cw + 1u) & (CN - 1u);
        }
        if (s->dly) {
            int32_t e1 = db[(dw - dd) & (DN - 1u)], e2 = db[(dw - dd - DS) & (DN - 1u)];
            db[dw] = (int16_t)sat(((l + r) >> 1) + mz(e1, fb));
            dw = (dw + 1u) & (DN - 1u);
            l = sat(l + ((e1 * wd) >> 15)); r = sat(r + ((e2 * wd) >> 15));
        }
        if (s->rev) {
            int32_t ol, orr;
            if ((rph ^= 1u) != 0) {                        /* a new reverb sample every other frame */
                int16_t *p = rb;
                int32_t in = (l + r) >> 3, sum = 0;
                for (k = 0; k < 4; k++) {                  /* combs: feedback rfb, damping 0.25 */
                    int32_t o = p[ci[k]];
                    cf[k] = mz(o, 24576) + mz(cf[k], 8192);
                    p[ci[k]] = (int16_t)sat(in + mz(cf[k], rfb));
                    if (++ci[k] >= CL[k]) ci[k] = 0;
                    sum += o; p += CL[k];
                }
                ol = orr = sum >> 1;
                for (k = 0; k < 4; k++) {                  /* all-passes, gain 0.5: two for the left, two for the right */
                    int32_t *v = k < 2 ? &ol : &orr, b = p[ai[k]];
                    p[ai[k]] = (int16_t)sat(*v + mz(b, 16384));
                    *v = b - *v;
                    if (++ai[k] >= AL[k]) ai[k] = 0;
                    p += AL[k];
                }
                rpl = rvl; rpr = rvr; rvl = ol; rvr = orr;
                ol = (rpl + rvl) >> 1; orr = (rpr + rvr) >> 1;   /* half way from the last one */
            } else { ol = rvl; orr = rvr; }
            l = sat(l + ((ol * wr) >> 15)); r = sat(r + ((orr * wr) >> 15));
        }
        st[0] = l; st[1] = r;
    }
}

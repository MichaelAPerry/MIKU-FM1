/* SPDX-License-Identifier: GPL-3.0-only */
/* Engine: renders two bars of the demo from row 2 (the first vocal bar) and checks the mix. */
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static Song g; static Engine en;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)
#define NF (44100 * 33 / 10)
static int32_t *buf;
static void render(uint32_t frames) {
    uint32_t done = 0;
    buf = calloc((size_t)frames * 2, 4);
    while (done < frames) {
        uint32_t n = frames - done < 256 ? frames - done : 256;
        en_render(&en, buf + 2 * done, n);
        done += n;
    }
}
static double f0(const int32_t *x, uint32_t n) {   /* autocorrelation pitch, 150..800 Hz, on the left channel */
    uint32_t lo = 44100 / 800, hi = 44100 / 150, best = lo;
    double bv = -1e30;
    for (uint32_t l = lo; l <= hi; l++) {
        double s = 0;
        for (uint32_t i = 0; i + l < n; i++) s += (double)x[2 * i] * x[2 * (i + l)];
        if (s > bv) { bv = s; best = l; }
    }
    return 44100.0 / best;
}
static int32_t peak(const int32_t *x, uint32_t n) { int32_t p = 0; for (uint32_t i = 0; i < n; i++) if (abs(x[i]) > p) p = abs(x[i]); return p; }
int main(void) {
    int32_t pk; uint32_t i, steps = 0;
    song_demo(&g);
    en_init(&en, &g);
    memset((void *)en.pan, 64, NTRK);
    /* full mix, row 2 for two bars */
    en_post(&en, EN_PLAY, 2, 0, 0);
    render(NF);
    pk = peak(buf, 2 * NF);
    CHECK(pk > 3000, "mix too quiet: %d", pk);
    CHECK(pk < 32767, "mix hit the hard cap: %d", pk);
    printf("two bars from row 2: peak %d, step now %u/%u\n", pk, (unsigned)(en.pos >> 8 & 255), (unsigned)(en.pos & 255));
    CHECK(en.pos >> 16 == 1, "not playing after 2 bars");
    CHECK((en.pos >> 8 & 255) == 4, "playhead should be on row 4, is %u", (unsigned)(en.pos >> 8 & 255));
    free(buf);
    /* solo vocal: first note of row 2 is G4 = 392 Hz */
    en_init(&en, &g);
    for (i = 0; i < NTRK; i++) g.trk[i].solo = i == TR_VOCAL;
    en_post(&en, EN_PLAY, 2, 0, 0);
    render(NF);
    {
        double hz = f0(buf + 2 * 5000, 4410);   /* 113..213 ms: after the onset scoop */
        printf("first vocal note f0 %.1f Hz (G4 = 392.0)\n", hz);
        CHECK(hz > 380 && hz < 404, "f0 %.1f", hz);
        CHECK(peak(buf, 2 * 3000) > 1000, "vocal silent at the start");
        for (i = 0; i < NF; i++) steps += buf[2 * i] != 0;
        CHECK(steps > NF / 4, "mostly silent");
    }
    free(buf);
    /* stop flushes and lock holds the sequencer */
    en_post(&en, EN_STOP, 0, 0, 0);
    { int32_t o[512]; memset(o, 0, sizeof o); en_render(&en, o, 256); }
    CHECK(en.pos >> 16 == 0, "still playing after STOP");
    en.lock = 1;
    en_post(&en, EN_PLAY, 0, 0, 0);
    { int32_t o[512]; memset(o, 0, sizeof o); en_render(&en, o, 256); CHECK(en.ack == 1 && !(en.pos >> 16), "lock did not hold"); }
    en.lock = 0;
    { int32_t o[512]; memset(o, 0, sizeof o); en_render(&en, o, 256); CHECK(en.ack == 0 && (en.pos >> 16) == 1, "unlock did not resume"); }
    /* the voice's sound (Voice): effects, character, languages. Vocal solo of rows 2-3 unless said otherwise. */
    {
        static const char *const nm[5] = { "drive", "tremolo", "chorus", "delay", "reverb" };
        int32_t *ref; double rd, rr;
        uint8_t *f[5] = { &g.vx.drv, &g.vx.trem, &g.vx.cho, &g.vx.dly, &g.vx.rev };
        en_init(&en, &g); en_post(&en, EN_PLAY, 2, 0, 0); render(NF); ref = buf;
        rd = 0; for (i = 0; i < 2u * NF; i++) rd += (double)ref[i] * ref[i];
        for (int k = 0; k < 5; k++) {
            double d = 0, lr = 0, e = 0; int32_t p;
            *f[k] = 60;
            en_init(&en, &g); en_post(&en, EN_PLAY, 2, 0, 0); render(NF);
            for (i = 0; i < NF; i++) { double a = buf[2 * i] - ref[2 * i]; d += a * a; lr += (double)(buf[2 * i] - buf[2 * i + 1]) * (buf[2 * i] - buf[2 * i + 1]); e += (double)buf[2 * i] * buf[2 * i] + (double)buf[2 * i + 1] * buf[2 * i + 1]; }
            p = peak(buf, 2 * NF); rr = 10 * log10(e / rd);
            printf("  %-7s 60: change %.0f rms, left-right %.0f rms, level %+.1f dB, peak %d\n", nm[k], sqrt(d / NF), sqrt(lr / NF), rr, p);
            CHECK(d > 0, "%s changes nothing", nm[k]);
            CHECK(p < 32767 && rr > -4 && rr < 4, "%s: level %+.1f dB, peak %d", nm[k], rr, p);
            CHECK(k < 2 ? lr == 0 : lr > 0, "%s: stereo %.0f", nm[k], sqrt(lr / NF));
            free(buf); *f[k] = 0;
        }
        /* all at 0 again: exactly the dry voice (no state left behind) */
        en_init(&en, &g); en_post(&en, EN_PLAY, 2, 0, 0); render(NF);
        CHECK(!memcmp(buf, ref, (size_t)NF * 8), "effects off is not the dry voice");
        free(buf);
        /* tails die: reverb and delay full, stop, 3 s later silence */
        g.vx.rev = 100; g.vx.dly = 100; g.vx.cho = 100;
        en_init(&en, &g); en_post(&en, EN_PLAY, 2, 0, 0); render(NF); free(buf);
        en_post(&en, EN_STOP, 0, 0, 0); render(44100 * 6); free(buf); render(4410);
        CHECK(peak(buf, 2 * 4410) == 0, "tail still sounding 6 s after stop: %d", peak(buf, 2 * 4410));
        free(buf);
        g.vx.rev = g.vx.dly = g.vx.cho = 0;
        /* character: the spectral centroid (harmonics of G2, dense enough to see the formants) averaged over the five vowels
           rises from DEEP to TINY. One vowel at one pitch is no test: a shifted formant lands on or between harmonics. */
        {
            double c[VC_N];
            for (int k = 0; k < VC_N; k++) {
                c[k] = 0;
                g.vx.chr = (uint8_t)k;
                for (int w = 1; w <= 5; w++) {
                    double s2 = 0, m2 = 0; int32_t o[512];
                    en_init(&en, &g); en_post(&en, EN_ON, TR_VOCAL, 43, (uint32_t)w);
                    for (i = 0; i < 40; i++) { memset(o, 0, sizeof o); en_render(&en, o, 256); }
                    render(8192);
                    for (int h = 1; h < 80; h++) {
                        double f = 97.9989 * h, re = 0, im = 0;
                        for (i = 0; i < 8192; i++) { double ww = .5 - .5 * cos(2 * M_PI * i / 8192); re += ww * buf[2 * i] * cos(2 * M_PI * f * i / 44100); im += ww * buf[2 * i] * sin(2 * M_PI * f * i / 44100); }
                        s2 += re * re + im * im; m2 += (re * re + im * im) * f;
                    }
                    c[k] += m2 / s2 / 5;
                    free(buf);
                }
            }
            printf("  character DEEP..TINY: brightness %.0f %.0f %.0f %.0f %.0f Hz\n", c[0], c[1], c[2], c[3], c[4]);
            for (int k = 1; k < VC_N; k++) CHECK(c[k] > c[k - 1], "character %d not brighter than %d", k, k - 1);
            g.vx.chr = VC_MIKU;
        }
        /* languages, on a held live note (2 s): ANIMAL and SCAT babble new syllables, VOWEL never sings a consonant */
        for (int l = VL_JP; l < VL_N; l++) {
            uint32_t ch = 0, last = 999, cons = 0; int32_t o[512];
            g.vx.lang = (uint8_t)l;
            en_init(&en, &g);
            en_post(&en, EN_ON, TR_VOCAL, 64, 7);          /* "ki" */
            for (i = 0; i < 344; i++) { memset(o, 0, sizeof o); en_render(&en, o, 256); if (en.mora != last) { ch++; last = en.mora; } cons |= fv_cons(&en.fv) != 0; }
            printf("  language %d: %u syllables in 2 s, consonants %s\n", l, (unsigned)ch, cons ? "yes" : "no");
            CHECK(l == VL_ANIMAL ? ch > 15 : l == VL_SCAT ? ch > 5 && ch < 15 : ch == 1, "language %d: %u syllables", l, (unsigned)ch);
            CHECK(l == VL_VOWEL ? !cons : cons, "language %d consonants", l);
        }
        g.vx.lang = VL_JP;
        free(ref);
    }
    printf(fails ? "engine: %d FAILED\n" : "engine: all ok\n", fails);
    return fails != 0;
}

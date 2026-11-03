/* SPDX-License-Identifier: GPL-3.0-only */
/* Engine: renders two bars of the demo from row 2 (the first vocal bar) and checks the mix. */
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    printf(fails ? "engine: %d FAILED\n" : "engine: all ok\n", fails);
    return fails != 0;
}

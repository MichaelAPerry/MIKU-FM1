/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_tables.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define FS 44100
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static double hz(int n) { return 440.0 * pow(2.0, (n - 69) / 12.0); }
typedef enum { K_CH, K_BS, K_DR } Kind;
static void rend(Kind k, void *s, double *x, int n) {   /* append mono (left) render as double; asserts silence stays exact */
    static int32_t b[2 * 4096];
    for (int i = 0; i < n; i += 4096) {
        int m = n - i < 4096 ? n - i : 4096;
        memset(b, 0, sizeof b);
        if (k == K_CH) ch_render(s, b, (uint32_t)m); else if (k == K_BS) bs_render(s, b, (uint32_t)m); else dr_render(s, b, (uint32_t)m);
        for (int j = 0; j < m; j++) { if (x) x[i + j] = b[2 * j]; CHECK(abs(b[2 * j]) < 65536 && abs(b[2 * j + 1]) < 65536, "overrange"); }
    }
}
static double zc_freq(const double *x, int n) {        /* rising zero crossings, interpolated */
    double first = -1, last = 0; int cnt = 0;
    for (int i = 1; i < n; i++) if (x[i - 1] < 0 && x[i] >= 0) {
        double t = i - 1 + (-x[i - 1]) / (x[i] - x[i - 1]);
        if (first < 0) first = t; last = t; cnt++;
    }
    return cnt < 3 ? 0 : (cnt - 1) * FS / (last - first);
}
static double peak(const double *x, int a, int b) { double p = 0; for (int i = a; i < b; i++) if (fabs(x[i]) > p) p = fabs(x[i]); return p; }
static double rms(const double *x, int a, int b) { double s = 0; for (int i = a; i < b; i++) s += x[i] * x[i]; return sqrt(s / (b - a)); }
static int ms(double t) { return (int)(t * FS / 1000.0); }
static double *buf(int n) { double *p = calloc((size_t)n, sizeof *p); if (!p) exit(2); return p; }

static void t_pitch(void) {
    static ChSynth s; BsSynth b;
    int n = FS;
    double *x = buf(n);
    for (int note = 36; note <= 96; note++) {
        ch_init(&s); ch_set_preset(&s, 4); s.p.ibase = 0; s.p.sus = 255;       /* pure sine */
        ch_note_on(&s, (uint8_t)note, 100); rend(K_CH, &s, x, n);
        double f = zc_freq(x + n / 2, n / 2);
        CHECK(fabs(f / hz(note) - 1) < 0.005, "chord note %d f=%.2f want %.2f", note, f, hz(note));
        CHECK(peak(x, n / 2, n) > 1000, "chord note %d too quiet", note);
    }
    for (int note = 28; note <= 72; note++) {
        bs_init(&b); bs_set_preset(&b, 1); b.p.ibase = 0; b.p.sus = 255;
        bs_note_on(&b, (uint8_t)note, 100, 0, 0); rend(K_BS, &b, x, n);
        double f = zc_freq(x + n / 2, n / 2);
        CHECK(fabs(f / hz(note) - 1) < 0.005, "bass note %d f=%.2f want %.2f", note, f, hz(note));
    }
    /* a real preset (integer ratio, harmonic spectrum) keeps its fundamental: organ 2:1 -> autocorrelation peak at the period */
    ch_init(&s); ch_set_preset(&s, 4); ch_note_on(&s, 57, 100); rend(K_CH, &s, x, n);
    int lag = (int)lround(FS / hz(57)); double best = -1e30; int bl = 0;
    for (int l = lag - 40; l < lag + 40; l++) { double c = 0; for (int i = 20000; i < 30000; i++) c += x[i] * x[i + l]; if (c > best) best = c, bl = l; }
    CHECK(abs(bl - lag) <= 1, "organ autocorr lag %d want %d", bl, lag);
    free(x);
}
static void t_poly(void) {
    static ChSynth s;
    double *x = buf(FS);
    ch_init(&s); ch_set_preset(&s, 4);
    for (int i = 0; i < 4; i++) { ch_note_on(&s, (uint8_t)(60 + i), 100); rend(K_CH, &s, NULL, 441); }
    CHECK(ch_active(&s) == 4, "4 voices active");
    ch_note_on(&s, 70, 100);                                   /* 5th steals the oldest (60) */
    int has[128] = {0}; for (int i = 0; i < 4; i++) has[s.v[i].note] = 1;
    CHECK(ch_active(&s) == 4 && !has[60] && has[61] && has[70], "steal oldest");
    ch_note_off(&s, 62); rend(K_CH, &s, NULL, 100);            /* released voice goes first */
    ch_note_on(&s, 72, 100);
    memset(has, 0, sizeof has); for (int i = 0; i < 4; i++) has[s.v[i].note] = 1;
    CHECK(!has[62] && has[61] && has[63] && has[70] && has[72], "steal released first");
    ch_note_on(&s, 63, 100);                                   /* same note retriggers its voice, steals nothing */
    memset(has, 0, sizeof has); for (int i = 0; i < 4; i++) has[s.v[i].note] = 1;
    CHECK(has[61] && has[63] && has[70] && has[72], "retrigger in place");
    /* no stuck voices after all_off, for every preset, within release + 50 ms */
    for (int p = 0; p < CH_NPRESET; p++) {
        ch_init(&s); ch_set_preset(&s, (uint8_t)p);
        for (int i = 0; i < 4; i++) ch_note_on(&s, (uint8_t)(48 + 4 * i), 127);
        rend(K_CH, &s, NULL, FS / 2);
        ch_all_off(&s);
        int n = ms(fmb_tms[s.p.rel] + 50); rend(K_CH, &s, x, n);
        CHECK(ch_active(&s) == 0, "preset %d stuck voices after release", p);
        CHECK(peak(x, n - 256, n) == 0, "preset %d not silent after release", p);
        for (int i = 0; i < 4; i++) ch_note_on(&s, (uint8_t)(50 + i), 90);
        for (int i = 0; i < 4; i++) ch_note_off(&s, (uint8_t)(50 + i));       /* note_off path */
        rend(K_CH, &s, x, n);
        CHECK(ch_active(&s) == 0, "preset %d stuck after note_offs", p);
    }
    /* envelopes end at exact silence on their own (sus 0), pitch/bell presets */
    int pr[4] = { 0, 1, 5, 6 };
    for (int q = 0; q < 4; q++) {
        ch_init(&s); ch_set_preset(&s, (uint8_t)pr[q]); ch_note_on(&s, 60, 127);
        int n = FS * 12; double *y = buf(n); rend(K_CH, &s, y, n);
        CHECK(ch_active(&s) == 0 && peak(y, n - FS, n) == 0, "preset %d does not end in silence (active %d)", pr[q], ch_active(&s));
        CHECK(peak(y, 0, FS / 2) > 2000, "preset %d quiet", pr[q]);
        free(y);
    }
    free(x);
}
static void t_bass(void) {
    static BsSynth s;
    for (int p = 0; p < BS_NPRESET; p++) {
        double *x = buf(FS);
        bs_init(&s); bs_set_preset(&s, (uint8_t)p);
        bs_note_on(&s, 40, 100, 0, 0); rend(K_BS, &s, NULL, FS / 4);
        CHECK(bs_active(&s), "p%d active", p);
        /* glide up an octave, then down; monotonic, reaches target within porta+10% */
        int dirs[2] = { 52, 40 };
        for (int d = 0; d < 2; d++) {
            int tgt = dirs[d] << 8, prev = s.pcur, ok = 1, reach = -1;
            bs_note_on(&s, (uint8_t)dirs[d], 100, 0, 1);
            CHECK(s.env.st == 3 || s.env.st == 2, "p%d slide must not retrigger (st %d)", p, s.env.st);
            double T = fmb_tms[s.p.porta];
            for (int i = 1; i <= 400; i++) {
                bs_render(&s, (int32_t[32]){0}, 16);
                if (d == 0 ? s.pcur < prev : s.pcur > prev) ok = 0;
                prev = s.pcur;
                if (s.pcur == tgt && reach < 0) reach = i;
            }
            CHECK(ok, "p%d glide not monotonic", p);
            CHECK(reach > 0 && reach * 16 * 1000.0 / FS <= T * 1.1, "p%d glide reach %.1f ms > %.1f ms", p, reach * 16 * 1000.0 / FS, T * 1.1);
        }
        bs_note_on(&s, 60, 100, 0, 0); CHECK(s.pcur == 60 << 8, "p%d non-slide jumps", p);
        bs_note_on(&s, 64, 100, 0, 1); bs_note_off(&s);
        bs_render(&s, (int32_t[32]){0}, 16); bs_note_on(&s, 67, 100, 0, 1); CHECK(s.env.st == 1 && s.pcur == 67 << 8, "p%d slide after release retriggers", p);
        /* accent louder */
        double e[2];
        for (int a = 0; a < 2; a++) {
            bs_init(&s); bs_set_preset(&s, (uint8_t)p); bs_note_on(&s, 36, 90, (uint8_t)a, 0);
            rend(K_BS, &s, x, FS / 2); e[a] = rms(x, 0, ms(120));
        }
        CHECK(e[1] > e[0] * 1.1, "p%d accent not louder (%.0f vs %.0f)", p, e[1], e[0]);
        /* ends in silence after release */
        bs_note_off(&s); rend(K_BS, &s, x, ms(fmb_tms[s.p.rel] + 50) < FS ? ms(fmb_tms[s.p.rel] + 50) : FS);
        CHECK(!bs_active(&s), "p%d stuck", p);
        free(x);
    }
}
static int rising(const double *x, int n, double *t, int mx) {
    int c = 0;
    for (int i = 1; i < n && c < mx; i++) if (x[i - 1] < 0 && x[i] >= 0) t[c++] = i - 1 + (-x[i - 1]) / (x[i] - x[i - 1]);
    return c;
}
static void t_drums(void) {
    static DrKit d;
    const double maxs[DR_N] = { 0.8, 0.6, 0.6, 0.2, 1.0, 0.7 };
    int n = FS * 2;
    double *x = buf(n);
    for (int p = 0; p < DR_NPRESET; p++) for (int i = 0; i < DR_N; i++) {
        dr_init(&d); dr_set_preset(&d, (uint8_t)p);
        dr_trig(&d, (uint8_t)i, 110); rend(K_DR, &d, x, n);
        CHECK(peak(x, 0, FS / 5) > 1500, "preset %d drum %d silent (%.0f)", p, i, peak(x, 0, FS / 5));
        CHECK(peak(x, 0, FS / 5) < 32000 * 1.0, "drum %d hot", i);
        int end = (int)(maxs[i] * FS);
        CHECK(peak(x, end, n) == 0 && dr_active(&d) == 0, "preset %d drum %d does not end by %.1f s", p, i, maxs[i]);
        dr_trig(&d, (uint8_t)i, 30); rend(K_DR, &d, x, FS / 4);
        double q = peak(x, 0, FS / 4);
        dr_init(&d); dr_set_preset(&d, (uint8_t)p); dr_trig(&d, (uint8_t)i, 127); rend(K_DR, &d, x, FS / 4);
        CHECK(peak(x, 0, FS / 4) > q, "drum %d velocity", i);
    }
    for (int p = 0; p < DR_NPRESET; p++) {                      /* kick pitch falls */
        dr_init(&d); dr_set_preset(&d, (uint8_t)p); dr_trig(&d, DR_KICK, 120); rend(K_DR, &d, x, FS / 2);
        double t[64]; int c = rising(x + ms(4), ms(60), t, 64);
        int c2 = 0; double t2[64]; c2 = rising(x + ms(100), ms(70), t2, 64);
        CHECK(c >= 3 && c2 >= 3, "kick crossings %d %d", c, c2);
        if (c >= 3 && c2 >= 3) {
            double f1 = FS / (t[2] - t[1]), f2 = FS / (t2[2] - t2[1]);
            CHECK(f1 > 1.4 * f2, "kick p%d not falling %.0f -> %.0f Hz", p, f1, f2);
        }
    }
    for (int p = 0; p < DR_NPRESET; p++) {                      /* choke */
        DrKit a, b;
        dr_init(&a); dr_set_preset(&a, (uint8_t)p); dr_trig(&a, DR_OHH, 120);
        dr_init(&b); dr_set_preset(&b, (uint8_t)p); dr_trig(&b, DR_OHH, 120);
        rend(K_DR, &a, NULL, ms(30)); rend(K_DR, &b, NULL, ms(30));
        dr_trig(&b, DR_CHH, 120);
        double *xa = buf(ms(150)), *xb = buf(ms(150));
        rend(K_DR, &a, xa, ms(150)); rend(K_DR, &b, xb, ms(150));
        CHECK(b.hao == 0 && a.hao > 4096, "choke state %u vs %u", b.hao, a.hao);
        CHECK(rms(xb, ms(80), ms(150)) < 0.1 * rms(xa, ms(80), ms(150)), "p%d closed hat does not choke open (%.1f vs %.1f)", p, rms(xb, ms(80), ms(150)), rms(xa, ms(80), ms(150)));
        free(xa); free(xb);
    }
    free(x);
}
/* ---------- full band stress through the mixer ---------- */
typedef struct { ChSynth c; BsSynth b; DrKit d; uint32_t rng; uint8_t lc, lb, ld; } Band;
static uint32_t rnd(Band *w) { w->rng ^= w->rng << 13; w->rng ^= w->rng >> 17; w->rng ^= w->rng << 5; return w->rng; }
static void band_init(Band *w, uint32_t seed) {
    memset(w, 0, sizeof *w); w->rng = seed;
    ch_init(&w->c); bs_init(&w->b); dr_init(&w->d);
    ch_set_mix(&w->c, 127, 40); bs_set_mix(&w->b, 127, 64); dr_set_mix(&w->d, 127, 90);
}
static void rand_event(Band *w) {
    uint32_t r = rnd(w);
    switch (r % 9) {
    case 0: ch_note_on(&w->c, (uint8_t)(24 + (r >> 8) % 90), (uint8_t)(r >> 16)); break;
    case 1: ch_note_off(&w->c, (uint8_t)(24 + (r >> 8) % 90)); break;
    case 2: if ((r >> 8) % 8 == 0) ch_all_off(&w->c); else ch_set_preset(&w->c, (uint8_t)((r >> 12) % 8)); break;
    case 3: bs_note_on(&w->b, (uint8_t)(20 + (r >> 8) % 60), (uint8_t)(r >> 16), (r >> 24) & 1, (r >> 25) & 1); break;
    case 4: bs_note_off(&w->b); if ((r >> 8) % 6 == 0) bs_set_preset(&w->b, (uint8_t)((r >> 12) % 4)); break;
    default: dr_trig(&w->d, (uint8_t)((r >> 8) % 6), (uint8_t)(r >> 16)); if ((r >> 28) == 0) dr_set_preset(&w->d, (uint8_t)((r >> 12) & 1)); break;
    }
}
static void band_block(Band *w, int32_t *o, uint32_t n) { memset(o, 0, (size_t)n * 8); ch_render(&w->c, o, n); bs_render(&w->b, o, n); dr_render(&w->d, o, n); mx_limit(o, n); }
static void t_stress(void) {
    static Band w; static int32_t o[2 * 256];
    band_init(&w, 0xC0FFEEu);
    long worst = 0; int zero_run = 0, maxzero = 0;
    for (int blk = 0; blk < 30 * FS / 256; blk++) {
        if (blk % 5 == 0) for (int e = 0; e < 3; e++) rand_event(&w);
        band_block(&w, o, 256);
        for (int i = 0; i < 512; i++) if (labs(o[i]) > worst) worst = labs(o[i]);
        zero_run = 0; for (int i = 0; i < 512; i++) zero_run += o[i] == 0; if (zero_run == 512) maxzero++;
        CHECK(labs(w.b.lp) <= 65535 && labs(w.b.bp) <= 65535, "svf state escaped");
    }
    CHECK(worst <= 32767, "stress peak %ld", worst);
    CHECK(worst > 20000, "stress too quiet %ld", worst);
    CHECK(maxzero < 30 * FS / 256 / 4, "silent blocks %d (stuck/idle?)", maxzero);
    ch_all_off(&w.c); bs_note_off(&w.b);
    for (int i = 0; i < 12 * FS / 256; i++) band_block(&w, o, 256);   /* drum tails and releases die out */
    int act = ch_active(&w.c) + bs_active(&w.b) + dr_active(&w.d);
    CHECK(act == 0, "stuck after stress: %d", act);
    long pk = 0; for (int i = 0; i < 512; i++) if (labs(o[i]) > pk) pk = labs(o[i]);
    CHECK(pk == 0, "not silent after stress");
    printf("stress peak %ld\n", worst);
    /* limiter: monotonic, odd, no wrap for huge inputs */
    int32_t t[2], prev = 0; int mono = 1;
    for (int32_t v = 0; v < 400000; v += 7) { t[0] = v; t[1] = -v; mx_limit(t, 1); if (t[0] < prev || t[0] > 32767 || t[1] != -t[0]) mono = 0; prev = t[0]; }
    t[0] = 2147483647; t[1] = (-2147483647 - 1); mx_limit(t, 1);
    CHECK(mono && t[0] == 32767 && t[1] == -32767, "limiter");
    t[0] = 20000; t[1] = -24576; mx_limit(t, 1); CHECK(t[0] == 20000 && t[1] == -24576, "limiter passes below knee");
    int32_t gl, gr; mx_gain(127, 0, &gl, &gr); CHECK(gr == 0 && gl > 30000, "pan hard left");
    mx_gain(127, 127, &gl, &gr); CHECK(gl == 0 && gr > 30000, "pan hard right");
    mx_gain(100, 64, &gl, &gr); CHECK(abs(gl - gr) < 2 && gl > 0, "pan centre");
    mx_gain(0, 64, &gl, &gr); CHECK(gl == 0 && gr == 0, "level 0");
}
static void t_determ(void) {
    static Band a, b; static int32_t oa[2 * 100], ob[2 * 100];
    band_init(&a, 77); band_init(&b, 77);
    long h1 = 0; int same = 1;
    for (int step = 0; step < 4000; step++) {               /* A in 100-frame chunks, B in 100 single frames */
        if (step % 3 == 0) { rand_event(&a); rand_event(&b); }
        band_block(&a, oa, 100);
        memset(ob, 0, sizeof ob);
        for (int i = 0; i < 100; i++) { ch_render(&b.c, ob + 2 * i, 1); bs_render(&b.b, ob + 2 * i, 1); dr_render(&b.d, ob + 2 * i, 1); }
        mx_limit(ob, 100);
        if (memcmp(oa, ob, sizeof oa)) same = 0;
        h1 += oa[3] + oa[100];
    }
    CHECK(same, "chunking changes the output");
    printf("determinism checksum %ld\n", h1);
    static Band c; band_init(&c, 77); long h2 = 0;
    for (int step = 0; step < 4000; step++) { if (step % 3 == 0) rand_event(&c); band_block(&c, oa, 100); h2 += oa[3] + oa[100]; }
    CHECK(h1 == h2, "run-to-run mismatch");
}
static void t_cost(void) {
    static Band w; static int32_t o[2 * 256];
    for (int mode=0;mode<3;mode++) {
    band_init(&w, 5);
    if (mode) { ch_set_modern(&w.c,(uint8_t)(mode-1)); bs_set_preset(&w.b,(uint8_t)(mode+3)); dr_set_preset(&w.d,(uint8_t)(mode+1)); }
    for (int i = 0; i < 4; i++) ch_note_on(&w.c, (uint8_t)(48 + 4 * i), 127);
    if (!mode) bs_set_preset(&w.b, 3); bs_note_on(&w.b, 36, 127, 1, 0);
    struct timespec t0, t1; double best = 1e9; long sink = 0;
    for (int rep = 0; rep < 200; rep++) {
        for (int i = 0; i < 6; i++) dr_trig(&w.d, (uint8_t)i, 127);
        memset(o, 0, sizeof o);
        clock_gettime(CLOCK_MONOTONIC, &t0);
        ch_render(&w.c, o, 256); bs_render(&w.b, o, 256); dr_render(&w.d, o, 256); mx_limit(o, 256);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double us = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
        if (us < best) best = us; sink += o[100];
        if (rep % 8 == 7) { for (int i = 0; i < 4; i++) ch_note_on(&w.c, (uint8_t)(48 + 4 * i), 127); bs_note_on(&w.b, 36, 127, 1, 0); }
    }
    printf("host: band mode %d, all instruments sounding, best of 200: %.1f us / 256-frame block (sink %ld)\n", mode, best, sink);
    }
}
/* Frozen against origin/main using little-endian int32 FNV-1a: all original presets stay exact. */
static void t_baseline(void) {
    static const uint32_t expected[3][8] = {
        {2623350273u,272721381u,2635900673u,586310193u,1385016497u,106538605u,1178010009u,3191860485u},
        {2079423785u,4200575773u,1119758805u,2318241549u},
        {1826567869u,2977136449u}
    };
    for (int kind = 0; kind < 3; kind++) for (uint32_t p = 0; p < (kind == 0 ? 8u : kind == 1 ? 4u : 2u); p++) {
        ChSynth c; BsSynth b; DrKit d; uint32_t h = 2166136261u; int32_t o[512];
        ch_init(&c); bs_init(&b); dr_init(&d); ch_set_preset(&c, (uint8_t)p); bs_set_preset(&b, (uint8_t)p); dr_set_preset(&d, (uint8_t)p);
        if (!kind) { ch_note_on(&c,48,110); ch_note_on(&c,55,100); ch_note_on(&c,64,90); }
        else if (kind == 1) bs_note_on(&b,36,110,1,0);
        else for (int i = 0; i < 6; i++) dr_trig(&d,(uint8_t)i,100);
        for (int blk = 0; blk < 48; blk++) {
            if (blk == 16) { ch_all_off(&c); bs_note_off(&b); }
            memset(o,0,sizeof o);
            if (!kind) ch_render(&c,o,256); else if (kind == 1) bs_render(&b,o,256); else dr_render(&d,o,256);
            for (int i = 0; i < 512; i++) { uint32_t v = (uint32_t)o[i]; for (int by = 0; by < 4; by++) { h ^= v & 255u; h *= 16777619u; v >>= 8; } }
        }
        CHECK(h == expected[kind][p], "original bank %d preset %u changed: %u", kind, p, h);
    }
}
static double harmonic(const double *x, int start, int n, double freq) {
    double re=0,im=0;
    for(int i=0;i<n;i++) { double w=.5-.5*cos(2*3.141592653589793*i/n); re+=x[start+i]*w*cos(2*3.141592653589793*freq*i/FS); im+=x[start+i]*w*sin(2*3.141592653589793*freq*i/FS); }
    return re*re+im*im;
}
static void t_modern(void) {
    double *x = buf(FS * 2);
    BsSynth b;
    bs_init(&b); bs_set_preset(&b,4); bs_note_on(&b,36,110,0,0);
    bs_render(&b,(int32_t[32]){0},16);
    CHECK(b.ic > fmb_inc[36] * 1.39 && b.ic < fmb_inc[36] * 1.43, "sub drop did not begin six semitones above note");
    rend(K_BS,&b,NULL,ms(100));
    CHECK(b.ic == fmb_inc[36] && b.pcur == 36 << 8, "sub drop did not settle at base pitch");
    bs_note_on(&b,43,110,0,1);
    CHECK(b.penv == 0 && b.env.st != 1, "slide retriggered pitch drop");
    for (int p = 4; p < BS_NPRESET; p++) for (int note = 24; note <= 96; note += 12) {
        bs_init(&b); bs_set_preset(&b,(uint8_t)p); bs_set_mix(&b,127,64);
        bs_note_on(&b,(uint8_t)note,127,1,0); rend(K_BS,&b,x,FS/2);
        CHECK(peak(x,0,FS/2) < 32767 && peak(x,0,FS/2) > 500, "modern bass %d note %d clipping/silent", p,note);
        bs_note_off(&b); rend(K_BS,&b,x,FS);
        CHECK(!bs_active(&b) && peak(x,FS-256,FS) == 0, "modern bass release %d",p);
    }
    { double ratio[2];
      for(int drive=0;drive<2;drive++) {
          bs_init(&b);bs_set_preset(&b,4);b.p.drive=drive?100:0;b.p.drop=0;b.p.sus=255;
          bs_note_on(&b,36,100,0,0);rend(K_BS,&b,x,FS);
          ratio[drive]=harmonic(x,FS/2,FS/4,hz(36)*3)/harmonic(x,FS/2,FS/4,hz(36));
      }
      printf("sub saturation third/fundamental power %.6f -> %.6f\n",ratio[0],ratio[1]);
      CHECK(ratio[1]>.005 && ratio[1]>ratio[0]*100,"saturation adds no measurable harmonics");
    }
    for (int p = 0; p < 2; p++) {
        ChSynth c, d; int32_t a[512], z[512];
        ch_init(&c); ch_set_modern(&c,(uint8_t)p); d=c;
        for (int i=0;i<4;i++) { ch_note_on(&c,(uint8_t)(48+4*i),127); ch_note_on(&d,(uint8_t)(48+4*i),127); }
        CHECK(ch_active(&c)==4, "modern chord changed polyphony");
        for (int blk=0;blk<100;blk++) {
            memset(a,0,sizeof a);memset(z,0,sizeof z);ch_render(&c,a,256);
            for(int f=0;f<256;f++)ch_render(&d,z+2*f,1);
            CHECK(!memcmp(a,z,sizeof a), "modern chord render chunking %d",p);
        }
        rend(K_CH,&c,x,FS/2); ch_all_off(&c); rend(K_CH,&c,x,FS);
        CHECK(!ch_active(&c) && peak(x,FS-256,FS)==0, "modern chord release %d",p);
        CHECK(c.p.detune>0, "modern chord missing unison");
    }
    free(x);
}
int main(void) {
    t_pitch(); t_poly(); t_bass(); t_drums(); t_stress(); t_determ(); t_cost(); t_baseline(); t_modern();
    printf(fails ? "fmband: %d FAILURES\n" : "fmband: all ok\n", fails);
    return fails != 0;
}

/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fvoice/fvoice.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define FS 44100
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static double hz(int n) { return 440.0 * pow(2.0, (n - 69) / 12.0); }
static void rd(FvVoice *v, double *x, int n) {   /* append mono render as double */
    static int32_t b[2 * 4096];
    for (int i = 0; i < n; i += 4096) {
        int m = n - i < 4096 ? n - i : 4096;
        memset(b, 0, sizeof b); fv_render(v, b, (uint32_t)m);
        for (int j = 0; j < m; j++) { CHECK(b[2 * j] == b[2 * j + 1], "L!=R"); x[i + j] = b[2 * j]; }
    }
}
/* f0 by autocorrelation: coarse lag, then a long lag m*T with parabolic refinement */
static double acf(const double *x, int n, int lag) {
    double s = 0; for (int i = 0; i + lag < n; i++) s += x[i] * x[i + lag]; return s;
}
static double f0_est(const double *x, int n, double fmin, double fmax) {
    int lo = (int)(FS / fmax) - 1, hi = (int)(FS / fmin) + 1, best = lo; double bv = -1e30;
    for (int l = lo; l <= hi; l++) { double c = acf(x, n, l); if (c > bv) bv = c, best = l; }
    int m = (int)((n / 2 < 900 ? n / 2 : 900) / best); if (m < 1) m = 1;
    int c0 = best * m, bl = c0; bv = -1e30;
    for (int l = c0 - m - 1; l <= c0 + m + 1; l++) { double c = acf(x, n, l); if (c > bv) bv = c, bl = l; }
    double a = acf(x, n, bl - 1), b = acf(x, n, bl), c = acf(x, n, bl + 1);
    double d = a - 2 * b + c, off = d != 0 ? 0.5 * (a - c) / d : 0;
    return FS * m / (bl + off);
}
static void setup(FvVoice *v) { fv_init(v); v->vib_depth = 0; v->breath = 0; v->bright = 90; }

static void t_f0(void) {
    for (int n = 48; n <= 84; n += 6) {
        static double x[FS / 2]; FvVoice v; setup(&v);
        fv_note_on(&v, (uint8_t)n, FV_A, 127); rd(&v, x, FS / 4);
        rd(&v, x, FS / 2);
        double f = f0_est(x, 8192 * 2, hz(n) * .8, hz(n) * 1.25), e = fabs(f / hz(n) - 1);
        CHECK(e < 0.005, "f0 note %d: %.3f vs %.3f (%.3f%%)", n, f, hz(n), e * 100);
    }
}
/* harmonic magnitudes (Hann-windowed DFT at k*f0) of note 45 steady vowel */
#define NH 40
static void harm(int vow, double *H, double *f0o, int note, double bright) {
    static double x[FS]; FvVoice v; setup(&v); v.bright = (uint8_t)bright;
    fv_note_on(&v, (uint8_t)note, (uint8_t)vow, 127); rd(&v, x, FS / 4);
    int N = FS / 2; rd(&v, x, N);
    double f0 = hz(note); *f0o = f0;
    for (int k = 1; k <= NH; k++) {
        double re = 0, im = 0, w0 = 2 * M_PI * k * f0 / FS;
        for (int i = 0; i < N; i++) { double w = .5 - .5 * cos(2 * M_PI * i / N); re += w * x[i] * cos(w0 * i); im += w * x[i] * sin(w0 * i); }
        H[k] = hypot(re, im) / N;
    }
}
static const double FT[5][2] = {{900, 1400}, {330, 2700}, {380, 1500}, {550, 2300}, {500, 900}};
static void t_formants(void) {
    static double H[5][NH + 1]; double f0 = 0;
    for (int w = 0; w < 5; w++) {
        harm(w, H[w], &f0, 45, 90);
        double pf[NH], pm[NH]; int np = 0;     /* local maxima, parabolic in dB */
        for (int k = 1; k * f0 <= 3300 + f0; k++) {
            double l = k > 1 ? H[w][k - 1] : 0, r = H[w][k + 1];
            if (H[w][k] > l && H[w][k] >= r) {
                double a = 20 * log10(l + 1e-9), b = 20 * log10(H[w][k]), c = 20 * log10(r + 1e-9), d = a - 2 * b + c;
                double off = d != 0 && k > 1 ? 0.5 * (a - c) / d : 0;
                if (k * f0 < 3200) { pf[np] = (k + off) * f0; pm[np++] = H[w][k]; }
            }
        }
        double f1 = 0, m1 = 0, f2 = 0, m2 = 0;
        for (int i = 0; i < np; i++) {
            if (pm[i] > m1) { f2 = f1; m2 = m1; f1 = pf[i]; m1 = pm[i]; } else if (pm[i] > m2) { f2 = pf[i]; m2 = pm[i]; }
        }
        double lo = f1 < f2 ? f1 : f2, hi = f1 < f2 ? f2 : f1;
        if (getenv("FV_DUMP")) { for (int k = 1; k <= 30; k++) printf("%.0f:%.0f ", k * f0, 20 * log10(H[w][k] + 1e-9)); printf("\n"); }
        printf("  vowel %d peaks %.0f %.0f (want %.0f %.0f)\n", w, lo, hi, FT[w][0], FT[w][1]);
        CHECK(fabs(lo / FT[w][0] - 1) <= .15 && fabs(hi / FT[w][1] - 1) <= .15, "vowel %d peaks %.0f/%.0f want %.0f/%.0f", w, lo, hi, FT[w][0], FT[w][1]);
    }
    double dmin = 1e9;
    for (int a = 0; a < 5; a++) for (int b = a + 1; b < 5; b++) {
        double s = 0; int c = 0;
        for (int k = 1; k * f0 < 4000; k++, c++) { double d = 20 * log10(H[a][k] / H[b][k]); s += d * d; }
        double dd = sqrt(s / c); if (dd < dmin) dmin = dd;
        CHECK(dd > 4.0, "vowels %d,%d too similar: %.1f dB", a, b, dd);
    }
    printf("  min vowel envelope distance %.1f dB\n", dmin);
}
/* f0 track with sliding window ACF */
static int track(const double *x, int n, int win, int hop, double fmin, double fmax, double *f) {
    int c = 0; for (int s = 0; s + win <= n; s += hop) f[c++] = f0_est(x + s, win, fmin, fmax); return c;
}
static void t_vibrato(void) {
    static double x[FS * 3], f[2000]; FvVoice v; setup(&v);
    v.vib_depth = 50; v.vib_rate = 55; v.vib_delay = 0;
    fv_note_on(&v, 69, FV_A, 127); rd(&v, x, FS * 3);
    int hop = 441, win = 1323, nf = track(x, FS * 3, win, hop, 380, 520, f);
    double mx = 0, mn = 1e9, mean = 0; int c = 0, cross = 0, i0 = (FS / 2) / hop, i1 = (int)(2.5 * FS) / hop;
    for (int i = i0; i < i1 && i < nf; i++) { if (f[i] > mx) mx = f[i]; if (f[i] < mn) mn = f[i]; mean += f[i]; c++; }
    mean /= c;
    for (int i = i0 + 1; i < i1 && i < nf; i++) if ((f[i - 1] - mean) * (f[i] - mean) < 0) cross++;
    double swing = (mx - mn) / 2 / mean, rate = cross / 2.0 / ((i1 - i0) * hop / (double)FS);
    printf("  vibrato swing %.2f%% rate %.2f Hz\n", swing * 100, rate);
    CHECK(swing > .024 && swing < .034, "vibrato swing %.2f%%", swing * 100);
    CHECK(rate > 5 && rate < 6, "vibrato rate %.2f", rate);
    setup(&v); v.vib_depth = 50; v.vib_rate = 55; v.vib_delay = 300;
    fv_note_on(&v, 57, FV_A, 127); rd(&v, x, FS);
    nf = track(x, FS, 2205, 441, 180, 280, f);
    mx = 0; mn = 1e9;
    for (int i = 4; i * hop + 2205 <= FS / 4 && i < nf; i++) { if (f[i] > mx) mx = f[i]; if (f[i] < mn) mn = f[i]; }
    printf("  delay: first 250 ms deviation %.3f%%\n", (mx - mn) / 2 / hz(57) * 100);
    CHECK((mx - mn) / 2 / hz(57) < .003, "delay deviation %.3f%%", (mx - mn) / 2 / hz(57) * 100);
}
static void t_porta(void) {
    static double x[FS], f[400]; FvVoice v; setup(&v); v.porta = 100;
    fv_note_on(&v, 60, FV_A, 127); rd(&v, x, FS / 2);
    fv_note_on(&v, 72, FV_A, 127);
    int prev = v.pcur, mono = 1; double t = 0;
    for (int i = 0; i < 2 * FS / 10 / 16; i++) { static double d[16]; rd(&v, d, 16); mono &= v.pcur >= prev; prev = v.pcur; t += 16.0 / FS; }
    CHECK(mono, "pitch state not monotonic");
    setup(&v); v.porta = 100; fv_note_on(&v, 60, FV_A, 127); rd(&v, x, FS / 2);
    fv_note_on(&v, 72, FV_A, 127); rd(&v, x, FS / 2);
    int hop = 220, win = 441, nf = track(x, FS / 2, win, hop, 240, 560, f);
    if (getenv("FV_DUMP")) for (int i = 0; i < nf; i += 2) printf("%d:%.0f ", i * hop * 1000 / FS, f[i]);
    double last = 0, worst = 0;
    for (int i = 0; i < nf; i++) { double t_ms = (i * hop + win / 2) * 1000.0 / FS; if (t_ms > 40 && f[i] < last) worst = fmax(worst, last / f[i] - 1); last = f[i];
        if (t_ms > 140) CHECK(fabs(f[i] / hz(72) - 1) < .01, "porta at %.0f ms: %.1f Hz", t_ms, f[i]); }
    printf("  porta worst non-monotonic step %.2f%%\n", worst * 100);
    CHECK(worst < .02, "porta f0 not monotonic: %.2f%%", worst * 100);
    CHECK(f[nf - 1] > hz(72) * .99, "porta end");
}
static void t_env(void) {
    static double x[FS]; FvVoice v; setup(&v); v.release = 50;
    fv_note_on(&v, 69, FV_A, 127); rd(&v, x, FS / 2);
    double st = 0; rd(&v, x, 1); fv_init(&v); setup(&v); v.release = 50;
    fv_note_on(&v, 69, FV_O, 127); rd(&v, x, FS / 2);
    for (int i = 1; i < 88; i++) st = fmax(st, fabs(x[i] - x[i - 1]));   /* first 2 ms */
    double st10 = 0; for (int i = 1; i < 441; i++) st10 = fmax(st10, fabs(x[i] - x[i - 1]));
    double ss = 0; for (int i = 20000; i < 22000; i++) ss = fmax(ss, fabs(x[i] - x[i - 1]));
    printf("  steady max step %.0f\n", ss);
    CHECK(fabs(x[0]) < 50, "first sample %.0f", x[0]);
    CHECK(st <= 2000, "onset step %.0f", st);   /* the waveform itself steps ~3000 at glottal closure when steady */
    CHECK(st10 <= ss, "onset step 10 ms %.0f > steady %.0f", st10, ss);
    printf("  onset max step %.0f (2 ms), %.0f (10 ms)\n", st, st10);
    fv_note_off(&v); rd(&v, x, FS / 2);
    double mx = 0; for (int i = 200 * FS / 1000; i < FS / 2; i++) mx = fmax(mx, fabs(x[i]));
    CHECK(mx < 16, "tail after 200 ms: %.0f", mx);
    printf("  release tail max after 200 ms: %.0f\n", mx);
    CHECK(fv_level(&v) == 0, "level after release");
}
static uint32_t rs = 12345;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void t_robust(void) {
    static int32_t b[2 * 4096]; FvVoice v; fv_init(&v);
    int worst = 0, sil = 0, chunks = 0;
    for (long t = 0; t < 30L * FS;) {
        int a = (int)(rnd() % 10), ext = rnd() & 1;
        if (a < 3) fv_note_on(&v, (uint8_t)(30 + rnd() % 79), (uint8_t)(rnd() % 5), (uint8_t)(1 + rnd() % 127));
        else if (a == 3) fv_note_off(&v);
        else if (a == 4) fv_set_vowel(&v, (uint8_t)(rnd() % 5));
        else if (a == 5) { v.breath = ext ? 127 : 0; v.bright = (uint8_t)(rnd() & 1 ? 127 : 0); }
        else if (a == 6) { v.vib_depth = (uint16_t)(ext ? 100 : rnd() % 101); v.vib_rate = (uint16_t)(30 + rnd() % 51); v.vib_delay = (uint16_t)(ext ? 0 : rnd() % 400); }
        else if (a == 7) { v.porta = (uint16_t)(ext ? 0 : rnd() % 600); v.attack = (uint16_t)(ext ? 0 : rnd() % 600); v.release = (uint16_t)(ext ? 0 : rnd() % 600); }
        else { v.breath = (uint8_t)(rnd() % 128); v.bright = (uint8_t)(rnd() % 128); }
        int n = 100 + (int)(rnd() % 12000), gated0 = v.gated, mx = 0;
        for (int i = 0; i < n; i += 4096) {
            int m = n - i < 4096 ? n - i : 4096; memset(b, 0, sizeof b); fv_render(&v, b, (uint32_t)m);
            for (int j = 0; j < 2 * m; j++) { int s = abs(b[j]); if (s > mx) mx = s; }
        }
        if (mx > worst) worst = mx;
        if (gated0 && v.gated && n > 6000) { chunks++; if (mx < 30) sil++; }
        t += n;
    }
    printf("  random: peak %d, long gated chunks %d, quiet %d\n", worst, chunks, sil);
    CHECK(worst <= 32767, "overflow %d", worst);
    CHECK(sil * 20 <= chunks, "stuck/silent while gated: %d of %d", sil, chunks);
}
static void t_det(void) {
    static int32_t a[2 * 20000], b[2 * 20000]; FvVoice u, w;
    for (int k = 0; k < 2; k++) {
        FvVoice *v = k ? &w : &u; int32_t *o = k ? b : a; memset(o, 0, sizeof a); fv_init(v);
        v->vib_depth = 40; v->breath = 30;
        fv_note_on(v, 64, FV_E, 100); fv_render(v, o, 7000); fv_note_on(v, 71, FV_I, 90); fv_render(v, o + 14000, 7000);
        fv_note_off(v); fv_render(v, o + 28000, 6000);
    }
    CHECK(!memcmp(a, b, sizeof a) && !memcmp(&u, &w, sizeof u), "nondeterministic");
    int nz = 0; for (int i = 0; i < 40000; i++) nz |= a[i] != 0; CHECK(nz, "silent");
}
static void t_mouth(void) {
    FvVoice v; setup(&v); static int32_t b[2 * 4096];
    CHECK(fv_mouth(&v) == 5 && fv_level(&v) == 0, "idle");
    for (int w = 0; w < 5; w++) {
        fv_note_on(&v, 60, (uint8_t)w, 100); memset(b, 0, sizeof b); fv_render(&v, b, 4096); fv_render(&v, b, 4096);
        CHECK(fv_mouth(&v) == w, "mouth %d got %d", w, fv_mouth(&v)); CHECK(fv_level(&v) > 200, "level %d", fv_level(&v));
    }
    fv_note_off(&v); for (int i = 0; i < 40; i++) { memset(b, 0, sizeof b); fv_render(&v, b, 4096); }
    CHECK(fv_mouth(&v) == 5 && fv_level(&v) == 0, "closed after release %d %d", fv_mouth(&v), fv_level(&v));
}
static void t_level(void) {
    for (int w = 0; w < 5; w++) for (int n = 48; n <= 84; n += 12) {
        static double x[FS / 2]; FvVoice v; setup(&v); fv_note_on(&v, (uint8_t)n, (uint8_t)w, 127); rd(&v, x, FS / 4); rd(&v, x, FS / 4);
        double mx = 0; for (int i = 0; i < FS / 4; i++) mx = fmax(mx, fabs(x[i]));
        printf("  peak vowel %d note %d: %.0f\n", w, n, mx);
        CHECK(mx > 3000 && mx <= 22000, "peak vowel %d note %d: %.0f", w, n, mx);
    }
}
static void t_cost(void) {
    static int32_t b[2 * 256]; FvVoice v; fv_init(&v); v.vib_depth = 30; fv_note_on(&v, 69, FV_A, 127);
    clock_t c0 = clock(); int blocks = FS * 10 / 256;
    for (int i = 0; i < blocks; i++) { memset(b, 0, sizeof b); fv_render(&v, b, 256); if (i == blocks / 2) fv_set_vowel(&v, FV_I); }
    printf("  cost: %.1f us per 256-frame block (host)\n", (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / blocks);
}
int main(void) {
    printf("f0\n"); t_f0(); printf("formants\n"); t_formants(); printf("vibrato\n"); t_vibrato();
    printf("porta\n"); t_porta(); printf("env\n"); t_env(); printf("robust\n"); t_robust();
    printf("det\n"); t_det(); printf("mouth\n"); t_mouth(); printf("level\n"); t_level(); t_cost();
    printf(fails ? "fvoice: %d FAILED\n" : "fvoice: ok\n", fails);
    return fails != 0;
}

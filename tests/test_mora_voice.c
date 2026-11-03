/* SPDX-License-Identifier: GPL-3.0-only */
/* Step 3: consonants of the formant voice (fv_note_on_mora), measured on the rendered signal (host: double/libm allowed). */
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/song/mora.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FS 44100
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static double hz(int n) { return 440.0 * pow(2.0, (n - 69) / 12.0); }
static int ms(double t) { return (int)(t * FS / 1000.0 + 0.5); }
#define MAXN (FS * 2)
static double X[MAXN];
/* machine-exact voice: no wander, no scoop, no vibrato, no breath (so the consonant is the only noise) */
static void setup(FvVoice *v) { fv_init(v); v->vib_depth = 0; v->breath = 0; v->human = 0; v->scoop = 0; }
/* render n samples of mora (cons, vow) on `note` into X[]; pre > 0: the plain vowel `pvow` sounds for pre samples first, then the mora
   is sung legato (gated). Returns the voice for further inspection. */
static FvVoice V;
static void sing(int n, int note, int cons, int vow, int pre, int pvow, int vel) {
    static int32_t b[2 * 256]; int done = 0;
    setup(&V);
    if (pre) { fv_note_on(&V, (uint8_t)note, (uint8_t)pvow, (uint8_t)vel); for (int i = 0; i < pre; i += 256) { memset(b, 0, sizeof b); fv_render(&V, b, 256); } }
    fv_note_on_mora(&V, (uint8_t)note, (uint8_t)cons, (uint8_t)vow, (uint8_t)vel);
    while (done < n) {
        int m = n - done < 256 ? n - done : 256;
        memset(b, 0, sizeof b); fv_render(&V, b, (uint32_t)m);
        for (int j = 0; j < m; j++) { CHECK(b[2 * j] == b[2 * j + 1], "L!=R"); X[done + j] = b[2 * j]; }
        done += m;
    }
}
static double rms(int a, int e) { double s = 0; for (int i = a; i < e; i++) s += X[i] * X[i]; return sqrt(s / (e > a ? e - a : 1)); }
static double db(double r, double ref) { return 20 * log10((r + 1e-9) / (ref + 1e-9)); }
/* Hann-windowed power at f; band energy fractions and centroid from a 25 Hz grid */
static double pw(int a, int e, double f) {
    double re = 0, im = 0, w0 = 2 * M_PI * f / FS; int n = e - a;
    for (int i = 0; i < n; i++) { double w = .5 - .5 * cos(2 * M_PI * i / n); re += w * X[a + i] * cos(w0 * i); im += w * X[a + i] * sin(w0 * i); }
    return re * re + im * im;
}
static double band(int a, int e, double f0, double f1) { double s = 0; for (double f = f0; f < f1; f += 25) s += pw(a, e, f); return s; }
static double centroid(int a, int e, double f0, double f1) { double s = 0, m = 0; for (double f = f0; f < f1; f += 25) { double p = pw(a, e, f); s += p; m += p * f; } return s > 0 ? m / s : 0; }
static double nacf(int a, int e, int lag0, int lag1) {     /* max normalised autocorrelation over lags (overlap-normalised: 1 = exactly periodic) */
    double best = -1;
    for (int l = lag0; l <= lag1; l++) {
        double s = 0, p = 0, q = 0;
        for (int i = a; i + l < e; i++) { s += X[i] * X[i + l]; p += X[i] * X[i]; q += X[i + l] * X[i + l]; }
        if (p > 0 && q > 0 && s / sqrt(p * q) > best) best = s / sqrt(p * q);
    }
    return best < 0 ? 0 : best;
}
static const char *CN[] = { "-", "k", "s", "sh", "t", "ch", "ts", "n", "h", "f", "m", "y", "r", "w", "g", "z", "j", "d", "b", "p", "N" };
static const char VN[] = "aiueo";
/* first sample whose 1 ms RMS exceeds thr (absolute) from `from` */
static int onset(int from, int to, double thr) {
    int w = ms(1);
    for (int i = from; i + w < to; i += 8) if (rms(i, i + w) > thr) return i;
    return -1;
}
static double vref(int note, int vow, int vel) { sing(FS / 2, note, FC_NONE, vow, 0, 0, vel); return rms(ms(250), ms(450)); }

static void t_enums(void) {
    CHECK((int)MC_NONE == (int)FC_NONE && (int)MC_K == (int)FC_K && (int)MC_S == (int)FC_S && (int)MC_SH == (int)FC_SH && (int)MC_T == (int)FC_T && (int)MC_CH == (int)FC_CH && (int)MC_TS == (int)FC_TS && (int)MC_N == (int)FC_N &&
          (int)MC_H == (int)FC_H && (int)MC_F == (int)FC_F && (int)MC_M == (int)FC_M && (int)MC_Y == (int)FC_Y && (int)MC_R == (int)FC_R && (int)MC_W == (int)FC_W && (int)MC_G == (int)FC_G && (int)MC_Z == (int)FC_Z &&
          (int)MC_J == (int)FC_J && (int)MC_D == (int)FC_D && (int)MC_B == (int)FC_B && (int)MC_P == (int)FC_P && (int)MC_NN == (int)FC_NN, "MC_* and FC_* differ");
}
static void t_plosive(void) {
    printf("plosives (note 55, vel 110): closure drop dB / burst onset ms / burst centroid Hz per vowel a i u e o\n");
    double cen[3][5];
    static const int cl[3] = { FC_K, FC_T, FC_P };
    for (int c = 0; c < 3; c++) {
        printf("  %s:", CN[cl[c]]);
        for (int w = 0; w < 5; w++) {
            double vr = vref(55, w, 110);
            sing(FS / 5, 55, cl[c], w, 0, 0, 110);
            double clo = rms(ms(0.5), ms(8.5)), drop = db(clo, vr);
            int t0 = onset(0, ms(60), 0.05 * vr);
            double ce = t0 < 0 ? 0 : centroid(t0, t0 + ms(6), 100, 10000);
            cen[c][w] = ce;
            printf(" [%.0f dB %.1f ms %.0f]", drop, t0 * 1000.0 / FS, ce);
            CHECK(drop <= -12, "%s%c closure only %.1f dB below the vowel", CN[cl[c]], VN[w], drop);
            CHECK(t0 > ms(5) && t0 < ms(20), "%s%c burst at %.1f ms", CN[cl[c]], VN[w], t0 * 1000.0 / FS);
        }
        printf("\n");
    }
    for (int w = 0; w < 5; w++) CHECK(cen[1][w] > cen[0][w] + 300 && cen[0][w] > cen[2][w] + 300, "burst centroids t>k>p fail for vowel %c: %.0f %.0f %.0f", VN[w], cen[1][w], cen[0][w], cen[2][w]);
}
static void t_fric(void) {
    printf("fricatives (note 55): centroid Hz of 5..40 ms, fraction of energy >2 kHz, rms vs vowel dB\n");
    double cs = 0, csh = 0;
    for (int w = 0; w < 5; w++) {
        double vr = vref(55, w, 110);
        for (int c = FC_S; c <= FC_SH; c++) {
            sing(FS / 5, 55, c, w, 0, 0, 110);
            double ce = centroid(ms(5), ms(40), 300, 12000), fr = band(ms(5), ms(40), 2000, 12000) / band(ms(5), ms(40), 25, 12000), r = db(rms(ms(5), ms(40)), vr);
            if (w == 0) { if (c == FC_S) cs = ce; else csh = ce; }
            printf("  %s%c %.0f Hz  >2k %.2f  %.1f dB\n", CN[c], VN[w], ce, fr, r);
            CHECK(fr > 0.8, "%s%c noise above 2 kHz does not dominate the first 40 ms (%.2f)", CN[c], VN[w], fr);
            CHECK(ce > 2000, "%s%c centroid %.0f", CN[c], VN[w], ce);
            CHECK(r > -14 && r < 0, "%s%c level %.1f dB re vowel", CN[c], VN[w], r);
        }
    }
    CHECK(cs > csh + 500, "s centroid %.0f not above sh %.0f", cs, csh);
}
static void t_nasal(void) {
    printf("nasals / hum: energy<500 Hz fraction (note 48, 55), <1k at 72, level vs vowel dB\n");
    static const int nn[] = { 48, 55, 72, 84 };
    for (int c = 0; c < 3; c++) {
        int cc = c == 0 ? FC_N : c == 1 ? FC_M : FC_NN;
        for (int k = 0; k < 4; k++) {
            int note = nn[k];
            double vr = vref(note, FV_A, 110);
            sing(FS / 2, note, cc, FV_A, 0, 0, 110);
            int a = ms(15), e = ms(45);
            double f = note > 80 ? 1 : note < 70 ? band(a, e, 25, 500) / band(a, e, 25, 12000) : band(a, e, 25, 1000) / band(a, e, 25, 12000), r = db(rms(a, e), vr);
            printf("  %s note %d: %.2f %s  %.1f dB\n", CN[cc], note, f, note < 70 ? "<500" : "<1k", r);
            CHECK(f >= (note > 80 ? 0.5 : 0.7), "%s note %d low-frequency fraction %.2f", CN[cc], note, f);
            if (cc != FC_NN) CHECK(r < -3, "%s level %.1f dB not below the vowel", CN[cc], r);
        }
    }
}
static void t_period(void) {
    printf("periodicity (note 55, window 8..22 ms): voiced vs unvoiced, normalised autocorr at the f0 lag\n");
    int T = (int)(FS / hz(55) + 0.5);
    static const int vc[] = { FC_G, FC_D, FC_B, FC_Z, FC_J, FC_N, FC_M };
    static const int uc[] = { FC_K, FC_T, FC_P, FC_S, FC_SH, FC_H, FC_F };
    for (int i = 0; i < 7; i++) {
        sing(FS / 5, 55, vc[i], FV_A, 0, 0, 110); double a = nacf(ms(8), ms(22), T - 3, T + 3);
        printf("  voiced %s %.2f", CN[vc[i]], a);
        CHECK(a > 0.5, "voiced %s autocorrelation %.2f", CN[vc[i]], a);
        sing(FS / 5, 55, uc[i], FV_A, 0, 0, 110); a = nacf(ms(8), ms(22), T - 3, T + 3);
        printf("   unvoiced %s %.2f\n", CN[uc[i]], a);
        CHECK(a < 0.3, "unvoiced %s autocorrelation %.2f", CN[uc[i]], a);
    }
}
static double cen2(int a, int e, double lo, double hi) { return centroid(a, e, lo, hi); }
static void t_glide(void) {
    printf("glides (note 50): F2-band spectral centroid (1300..3500, o: 700..3500) at 4, 20, 40, 70, 250 ms\n");
    static const int gv[3] = { FV_A, FV_U, FV_O };
    for (int i = 0; i < 3; i++) {
        sing(FS / 2, 50, FC_Y, gv[i], 0, 0, 110);
        double lo = gv[i] == FV_O ? 700 : 1300;      /* F2 band: o's F2 (1000) is below 1300 */
        double c[5] = { cen2(ms(0), ms(8), lo, 3500), cen2(ms(14), ms(26), lo, 3500), cen2(ms(34), ms(46), lo, 3500), cen2(ms(64), ms(76), lo, 3500), cen2(ms(244), ms(256), lo, 3500) };
        printf("  y%c: %.0f %.0f %.0f %.0f %.0f\n", VN[gv[i]], c[0], c[1], c[2], c[3], c[4]);
        CHECK(c[0] > c[4] + 300 && c[1] > c[3] && c[0] > c[2], "y%c F2 does not move from i toward the vowel", VN[gv[i]]);
    }
    sing(FS / 2, 50, FC_W, FV_A, 0, 0, 110);
    double f1[3] = { cen2(ms(0), ms(10), 150, 1500), cen2(ms(40), ms(50), 150, 1500), cen2(ms(244), ms(256), 150, 1500) };
    printf("  wa F1-band centroid: %.0f %.0f %.0f\n", f1[0], f1[1], f1[2]);
    CHECK(f1[0] < f1[1] && f1[1] <= f1[2] + 30 && f1[2] > f1[0] + 100, "wa: F1 does not rise from u toward a");
    sing(FS / 2, 50, FC_W, FV_O, 0, 0, 110);
    double f2[3] = { cen2(ms(0), ms(10), 700, 3500), cen2(ms(40), ms(50), 700, 3500), cen2(ms(244), ms(256), 700, 3500) };
    printf("  wo F2-band centroid: %.0f %.0f %.0f\n", f2[0], f2[1], f2[2]);
    CHECK(f2[0] > f2[2] + 100, "wo: F2 does not fall from u toward o");
}
static void t_flap(void) {
    printf("flap r (note 60, legato after a): rms in 4 ms windows relative to the vowel, dip depth and duration\n");
    for (int w = 0; w < 5; w++) {
        double vr = vref(60, w, 110);
        sing(FS / 2, 60, FC_R, w, FS / 4, w, 110);
        double mn = 1e9; int t1 = -1, t2 = -1;
        for (int i = 0; i + ms(4) < ms(80); i += ms(0.5)) { double d = db(rms(i, i + ms(4)), vr); if (d < mn) mn = d; if (d < -3 && t1 < 0) t1 = i; if (d < -3) t2 = i; }
        double dur = t1 < 0 ? 0 : (t2 - t1) * 1000.0 / FS;
        printf("  r%c: dip %.1f dB, %.1f ms below -3 dB\n", VN[w], mn, dur);
        CHECK(mn < -3 && dur <= 30 && dur > 0, "r%c dip %.1f dB for %.1f ms", VN[w], mn, dur);
    }
}
static double Y[MAXN];
static void t_duration(void) {
    printf("consonant length is pitch independent (notes 60 and 84): fv_cons() run-out ms, and the ms after which (mora minus plain vowel) stays below 10%% of its own peak (not checked for y, w: the soprano zone tables bring i, u and the vowel closer together at 84)\n");
    for (int c = 1; c < FC_NN; c++) {
        double t[2], lm[2];
        for (int k = 0; k < 2; k++) {
            int note = k ? 84 : 60;
            setup(&V); fv_note_on_mora(&V, (uint8_t)note, (uint8_t)c, FV_A, 110);
            static int32_t b[2 * 16]; int blocks = 0;
            while (fv_cons(&V) && blocks < 1000) { memset(b, 0, sizeof b); fv_render(&V, b, 16); blocks++; }
            t[k] = blocks * 16.0 * 1000 / FS;
            sing(FS / 2, note, FC_NONE, FV_A, 0, 0, 110); memcpy(Y, X, sizeof Y);
            double vr = rms(ms(250), ms(450));
            sing(FS / 2, note, c, FV_A, 0, 0, 110);
            for (int i = 0; i < FS / 2; i++) X[i] -= Y[i];
            double pk = 0; lm[k] = 0; (void)vr;
            for (int i = 0; i + ms(2) < FS / 2; i += 8) { double r = rms(i, i + ms(2)); if (r > pk) pk = r; }
            for (int i = 0; i + ms(2) < FS / 2; i += 8) if (rms(i, i + ms(2)) > 0.1 * pk) lm[k] = i * 1000.0 / FS;
        }
        printf("  %s: %.2f/%.2f ms (cons_ms %u)  settled %.1f/%.1f ms\n", CN[c], t[0], t[1], (unsigned)fv_cons_ms((uint8_t)c), lm[0], lm[1]);
        CHECK(fabs(t[0] - t[1]) < 0.5, "%s duration differs with pitch: %.2f vs %.2f", CN[c], t[0], t[1]);
        CHECK(c == FC_Y || c == FC_W || fabs(lm[0] - lm[1]) <= 2.0, "%s settling time differs with pitch: %.1f vs %.1f", CN[c], lm[0], lm[1]);
        CHECK(fv_cons_ms((uint8_t)c) >= 10 && fv_cons_ms((uint8_t)c) <= 70, "%s cons_ms %u outside 10..70", CN[c], (unsigned)fv_cons_ms((uint8_t)c));
    }
}
static void t_vowel(void) {
    printf("the vowel after a consonant equals the plain vowel (harmonic levels, note 50, 300..500 ms)\n");
    double worst = 0;
    for (int c = 1; c <= FC_P; c++) for (int w = 0; w < 5; w++) {
        if (c == FC_NN) continue;
        static double H0[30], H1[30];
        sing(FS / 2, 50, FC_NONE, w, 0, 0, 110);
        for (int k = 1; k <= 24; k++) H0[k] = sqrt(pw(ms(300), ms(500), k * hz(50)));
        sing(FS / 2, 50, c, w, 0, 0, 110);
        for (int k = 1; k <= 24; k++) H1[k] = sqrt(pw(ms(300), ms(500), k * hz(50)));
        for (int k = 1; k <= 24; k++) { double d = fabs(db(H1[k], H0[k])); if (H0[k] > 0.02 * H0[1] && d > worst) worst = d; CHECK(d < 0.5 || H0[k] < 0.02 * H0[1], "%s%c harmonic %d differs %.2f dB", CN[c], VN[w], k, d); }
    }
    printf("  worst harmonic difference %.3f dB\n", worst);
}
static void t_mouth(void) {
    printf("mouth: closed for m / b / p closure and the n hum, vowel mouth for the others\n");
    static int32_t b[2 * 16];
    for (int c = 1; c <= FC_NN; c++) {
        int closed_early = 1, closed_late = 0, vowel_early = 1;
        setup(&V); fv_note_on_mora(&V, 55, (uint8_t)c, FV_E, 110);
        for (int k = 0; k < 400; k++) {
            memset(b, 0, sizeof b); fv_render(&V, b, 16);
            int t = k * 16 * 1000 / FS;
            if (k >= 4 && t < 9) { if (fv_mouth(&V) != 5) closed_early = 0; if (fv_mouth(&V) != FV_E) vowel_early = 0; }
            if (t > 120 && fv_mouth(&V) == 5) closed_late = 1;
        }
        int want = c == FC_M || c == FC_B || c == FC_P || c == FC_NN;
        printf("  %s: closed %d (want %d)\n", CN[c], closed_early, want);
        CHECK(want ? closed_early : vowel_early, "%s mouth in the first 9 ms", CN[c]);
        CHECK(c == FC_NN ? closed_late : !closed_late, "%s mouth after 120 ms", CN[c]);
    }
}
static void t_hum(void) {
    sing(FS, 55, FC_NN, FV_A, 0, 0, 110);
    int ok = 1; for (int a = ms(20); a + ms(40) <= FS; a += ms(40)) if (band(a, a + ms(40), 25, 500) / band(a, a + ms(40), 25, 12000) < 0.7) ok = 0;
    CHECK(ok, "n hum loses its low-frequency dominance during the note");
    double r1 = rms(ms(20), ms(60)), r2 = rms(ms(800), ms(900));
    CHECK(fabs(db(r1, r2)) < 2, "n hum level drifts %.1f dB", db(r1, r2));
}
static void t_legato(void) {
    printf("legato (note 60 after a sustained a): the consonant is still articulated, the envelope is not re-triggered\n");
    static const int lc[] = { FC_K, FC_T, FC_P, FC_S, FC_N, FC_M, FC_G };
    double vr = vref(60, FV_A, 110);
    for (int i = 0; i < 7; i++) {
        sing(FS / 3, 60, lc[i], FV_A, FS / 4, FV_A, 110);
        double clo = db(rms(ms(1), ms(8)), vr), late = db(rms(ms(200), ms(300)), vr);
        double mn = 1e9; for (int t = 0; t < ms(60); t += ms(1)) { double d = db(rms(t, t + ms(3)), vr); if (d < mn) mn = d; }
        printf("  %s: first 1..8 ms %.1f dB, deepest 3 ms window %.1f dB, steady %.1f dB, env %d\n", CN[lc[i]], clo, mn, late, V.env);
        CHECK(fabs(late) < 1.5, "%s legato: vowel level after the consonant is %.1f dB", CN[lc[i]], late);
        CHECK(mn < -6, "%s legato: no articulation dip (%.1f dB)", CN[lc[i]], mn);
        CHECK(V.env == 32767, "%s legato re-triggered the envelope", CN[lc[i]]);
    }
}
static void t_off(void) {
    static int32_t b[2 * 256];
    static const int oc[] = { FC_S, FC_K, FC_NN, FC_CH, FC_M };
    for (int i = 0; i < 5; i++) {
        setup(&V); fv_note_on_mora(&V, 60, (uint8_t)oc[i], FV_A, 110);
        memset(b, 0, sizeof b); fv_render(&V, b, 128); fv_note_off(&V);
        double last = 0;      /* a closure may be silent before its burst: look at the end of 150 ms */
        for (int n = 0; n < 100; n++) { memset(b, 0, sizeof b); fv_render(&V, b, 256); last = 0; if (n >= 90) for (int j = 0; j < 512; j++) last += fabs((double)b[j]); }
        CHECK(last == 0, "%s: still sounding 230 ms after note off", CN[oc[i]]);
        CHECK(fv_cons(&V) == FC_NONE && fv_mouth(&V) == 5 && fv_level(&V) == 0, "%s: consonant state left behind", CN[oc[i]]);
    }
}
static uint32_t hash_run(int stress, double *peak, int *clips, int *notes_done) {
    static int32_t b[2 * 256]; uint32_t h = 2166136261u, rng = 12345; FvVoice v; fv_init(&v);
    long total = 0; int ms_ = 0;
    while (total < 30L * FS) {
        rng = rng * 1664525u + 1013904223u; int mora = (int)((rng >> 8) % MORA_N), note = 48 + (int)((rng >> 16) % 49); int leg = (int)(rng >> 28 & 1);
        int vel = 30 + (int)((rng >> 4) & 63) + 34;
        if (!leg && v.gated) fv_note_off(&v);
        int vow = (int)mora_vowel((uint32_t)mora); if (vow > 4) vow = 0;
        fv_note_on_mora(&v, (uint8_t)note, (uint8_t)mora_cons((uint32_t)mora), (uint8_t)vow, (uint8_t)vel);
        int len = 3 + (int)((rng >> 12) & 7); if (!stress) len = 4;
        for (int k = 0; k < len; k++) {
            memset(b, 0, sizeof b); fv_render(&v, b, 256); total += 256;
            for (int i = 0; i < 512; i++) { double a = fabs((double)b[i]); if (a > *peak) *peak = a; if (a >= 32767) (*clips)++; h ^= (uint32_t)b[i]; h *= 16777619u; }
        }
        (*notes_done)++; (void)ms_;
    }
    return h;
}
static void t_stress(void) {
    double pk1 = 0, pk2 = 0; int c1 = 0, c2 = 0, n1 = 0, n2 = 0;
    uint32_t h1 = hash_run(1, &pk1, &c1, &n1), h2 = hash_run(1, &pk2, &c2, &n2);
    printf("stress: 30 s random morae x notes 48..96 x legato/non-legato: %d notes, peak %.0f, clipped samples %d, hash %08x\n", n1, pk1, c1, h1);
    CHECK(pk1 <= 32767 && c1 == 0, "voice reaches the clip: peak %.0f, %d samples", pk1, c1);
    CHECK(h1 == h2 && pk1 == pk2, "not deterministic");
}
int main(void) {
    t_enums(); t_plosive(); t_fric(); t_nasal(); t_period(); t_glide(); t_flap(); t_duration(); t_vowel(); t_mouth(); t_hum(); t_legato(); t_off(); t_stress();
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("mora voice tests passed\n"); return 0;
}

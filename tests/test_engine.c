/* SPDX-License-Identifier: GPL-3.0-only */
/* Engine: renders two bars of the demo from row 2 (the first vocal bar) and checks the mix. */
#include "firmware/app/engine.h"
#include "firmware/audio/dx7/bank.h"
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
/* ---- the hold-a-button layers: SOURCE, FX DEEP, ARTICULATION, HARMONY ---- */
static double pw(const int32_t *x, uint32_t a, uint32_t n, double f) {   /* Hann-windowed power at f, left channel */
    double re = 0, im = 0;
    for (uint32_t i = 0; i < n; i++) { double w = .5 - .5 * cos(2 * M_PI * i / n); re += w * x[2 * (a + i)] * cos(2 * M_PI * f * i / 44100); im += w * x[2 * (a + i)] * sin(2 * M_PI * f * i / 44100); }
    return re * re + im * im;
}
static double cen(const int32_t *x, uint32_t a, double f0, int nh) {    /* harmonic spectral centroid */
    double s = 0, m = 0;
    for (int h = 1; h <= nh; h++) { double p = pw(x, a, 8192, f0 * h); s += p; m += p * f0 * h; }
    return m / s;
}
static void live(uint32_t note, uint32_t mora, uint32_t frames) {       /* fresh engine, one held live note, render */
    int32_t o[512];
    en_init(&en, &g); memset((void *)en.pan, 64, NTRK);
    en_post(&en, EN_ON, TR_VOCAL, note, mora);
    memset(o, 0, sizeof o); en_render(&en, o, 256);
    render(frames);
}
static double rmsx(const int32_t *x, uint32_t a, uint32_t n) { double s = 0; for (uint32_t i = a; i < a + n; i++) s += (double)x[2 * i] * x[2 * i]; return sqrt(s / n); }
static void t_layers(void) {
    Voice d; double c0, c1, c2, r0, r1;
    sg_voice_default(&d);
    song_demo(&g); g.vx = d;
    for (int i = 0; i < NTRK; i++) g.trk[i].solo = 0;
    printf("layers:\n");
    /* SOURCE: tension darker below 50, brighter above */
    g.vx.tens = 0; live(55, 1, 12000); c0 = cen(buf, 3000, 195.998, 40); free(buf);
    g.vx.tens = 50; live(55, 1, 12000); c1 = cen(buf, 3000, 195.998, 40); free(buf);
    g.vx.tens = 100; live(55, 1, 12000); c2 = cen(buf, 3000, 195.998, 40); free(buf);
    printf("  tension 0/50/100: centroid %.0f %.0f %.0f Hz\n", c0, c1, c2);
    CHECK(c0 < c1 - 30 && c2 > c1 + 30, "tension does not darken / brighten");
    g.vx.tens = 50;
    /* fry: energy at f0/2 */
    live(55, 1, 12000); r0 = pw(buf, 3000, 8192, 98) / pw(buf, 3000, 8192, 196); free(buf);
    g.vx.fry = 80; live(55, 1, 12000); r1 = pw(buf, 3000, 8192, 98) / pw(buf, 3000, 8192, 196); free(buf); g.vx.fry = 0;
    printf("  fry 80: power at f0/2 re f0 %.4f -> %.4f\n", r0, r1);
    CHECK(r1 > 20 * r0 && r1 > 0.005, "fry adds no subharmonic");
    /* whisper: no pitch left, level kept */
    live(55, 1, 12000); r0 = rmsx(buf, 3000, 8192);
    { double a = pw(buf, 3000, 8192, 196); free(buf);
      g.vx.whsp = 100; live(55, 1, 12000); r1 = rmsx(buf, 3000, 8192);
      double b = pw(buf, 3000, 8192, 196), tot = 0; for (int h = 1; h < 60; h++) tot += pw(buf, 3000, 8192, 196.0 * h + 98); free(buf); g.vx.whsp = 0;
      printf("  whisper 100: f0 power %.3g -> %.3g, level %+.1f dB\n", a, b, 20 * log10(r1 / r0));
      CHECK(b < a / 100 && tot > 0, "whisper still pitched"); CHECK(20 * log10(r1 / r0) > -12 && 20 * log10(r1 / r0) < 3, "whisper level"); }
    /* formant fine shift: brightness rises with it (G2, averaged over the vowels) */
    { double c[3]; static const uint8_t fm[3] = { 0, 50, 100 };
      for (int k = 0; k < 3; k++) { c[k] = 0; g.vx.fmt = fm[k]; for (int w = 1; w <= 5; w++) { live(43, (uint32_t)w, 20000); c[k] += cen(buf, 10000, 97.9989, 79) / 5; free(buf); } }
      g.vx.fmt = 50;
      printf("  formant 0/50/100: %.0f %.0f %.0f Hz\n", c[0], c[1], c[2]);
      CHECK(c[0] < c[1] && c[1] < c[2], "formant shift not monotonic"); }
    /* FX DEEP: delay time 1/16 vs 1/4 at 120 BPM (125 vs 500 ms); reverb size makes a longer tail */
    { uint32_t at[2]; static const uint8_t dt[2] = { 0, 100 };
      g.bpm = 120; g.vx.dly = 100; g.vx.dfb = 0;
      for (int k = 0; k < 2; k++) {
          int32_t o[512]; double best = 0; at[k] = 0;
          g.vx.dtime = dt[k];
          en_init(&en, &g); memset((void *)en.pan, 64, NTRK);
          en_post(&en, EN_ON, TR_VOCAL, 60, 1); memset(o, 0, sizeof o); en_render(&en, o, 256);
          render(1764); free(buf); en_post(&en, EN_OFF, TR_VOCAL, 0, 0); render(44100);
          for (uint32_t i = 2400; i + 441 < 44100; i += 441) { double r = rmsx(buf, i, 441); if (r > best) { best = r; at[k] = i + 256 + 1764; } }   /* from 100 ms: past the dry note */
          free(buf);
      }
      printf("  delay 1/16 and 1/4 at 120 BPM: first echo at %.0f and %.0f ms after the note\n", at[0] / 44.1, at[1] / 44.1);
      CHECK(at[0] / 44.1 > 110 && at[0] / 44.1 < 180 && at[1] / 44.1 > 480 && at[1] / 44.1 < 560, "delay divisions");
      g.vx.dly = 0; g.vx.dfb = d.dfb; g.vx.dtime = d.dtime; g.bpm = 148; }
    { double t[2]; static const uint8_t rs[2] = { 0, 100 };
      g.vx.rev = 100;
      for (int k = 0; k < 2; k++) { int32_t o[512];
          g.vx.rsize = rs[k]; en_init(&en, &g); en_post(&en, EN_ON, TR_VOCAL, 60, 1); memset(o, 0, sizeof o); en_render(&en, o, 256);
          render(8820); free(buf); en_post(&en, EN_OFF, TR_VOCAL, 0, 0); render(88200); t[k] = rmsx(buf, 44100, 8820); free(buf); }
      printf("  reverb size 0/100: tail 1 s after the note %.1f / %.1f rms\n", t[0], t[1]);
      CHECK(t[1] > 10 * t[0] + 1, "reverb size does not lengthen the tail");
      g.vx.rev = 0; g.vx.rsize = d.rsize; }
    /* ARTICULATION: consonant length scales the consonant; level 0 silences the k burst */
    { uint32_t n[2]; static const uint8_t cl[2] = { 0, 100 };
      for (int k = 0; k < 2; k++) { int32_t o[32]; g.vx.clen = cl[k]; en_init(&en, &g); en_post(&en, EN_ON, TR_VOCAL, 60, 6);
          n[k] = 0; memset(o, 0, sizeof o); en_render(&en, o, 16); while (fv_cons(&en.fv) && n[k] < 1000) { memset(o, 0, sizeof o); en_render(&en, o, 16); n[k]++; } }
      g.vx.clen = 50;
      printf("  consonant length 0/100: k lasts %u / %u blocks\n", (unsigned)n[0], (unsigned)n[1]);
      CHECK(n[1] > 5 * n[0] && n[0] > 0, "consonant length"); }
    { double b[2]; static const uint8_t cv[2] = { 50, 0 };
      for (int k = 0; k < 2; k++) { g.vx.clev = cv[k]; live(60, 6, 4000); b[k] = rmsx(buf, 300, 900); free(buf); }   /* 7..27 ms: the k burst */
      g.vx.clev = 50;
      printf("  consonant level 50/0: k burst %.0f / %.0f rms\n", b[0], b[1]);
      CHECK(b[1] < b[0] / 10, "consonant level 0 keeps the burst"); }
    /* on-beat: the vowel of "ka" lands on the kick */
    { double gap[2]; static const uint8_t ob[2] = { 0, 100 };
      for (int k = 0; k < 2; k++) {
          int32_t kick = -1, vow = -1; double ref;
          Song h; sg_init(&h); h.bpm = 120; h.vx = d; h.vx.obeat = ob[k]; h.loop = 0;
          { Step v = {{ 64, 8, 6, 12 }}, dr = {{ D_KICK, 0, 0, 0 }}; sg_set_step(&h, TR_VOCAL, 0, 0, &v); sg_set_step(&h, TR_DRUMS, 0, 0, &dr); }
          sg_row_insert(&h, 0); sg_row_set(&h, 0, TR_VOCAL, 0); sg_row_set(&h, 0, TR_DRUMS, 0);
          h.trk[TR_DRUMS].mute = 1; en_init(&en, &h); en_post(&en, EN_PLAY, 0, 0, 0); render(22050);   /* vocal alone */
          ref = rmsx(buf, 13000, 2000);
          for (uint32_t i = 0; i + 88 < 22050; i += 22) if (rmsx(buf, i, 88) > ref * 0.75) { vow = (int32_t)i; break; }   /* the vowel: above the k burst (about -7 dB) */
          free(buf);
          h.trk[TR_DRUMS].mute = 0; h.trk[TR_VOCAL].mute = 1; en_init(&en, &h); en_post(&en, EN_PLAY, 0, 0, 0); render(22050);   /* the kick alone */
          for (uint32_t i = 0; i < 22050; i++) if (abs(buf[2 * i]) > 300) { kick = (int32_t)i; break; }
          free(buf);
          gap[k] = (vow - kick) / 44.1;
      }
      printf("  on-beat 0/100: vowel of ka after the kick %.1f / %.1f ms\n", gap[0], gap[1]);
      CHECK(gap[0] > 20 && fabs(gap[1]) < 8, "on-beat does not put the vowel on the beat"); }
    /* HARMONY: a fifth up is there; all at once stays under the cap; nothing sticks with on-beat */
    { double a, b;
      live(60, 1, 12000); a = pw(buf, 3000, 8192, 261.63 * 1.4983); free(buf);
      g.vx.hint = 80; g.vx.hmix = 100; live(60, 1, 12000); b = pw(buf, 3000, 8192, 261.63 * 1.4983); free(buf);   /* 80 -> +7 */
      printf("  harmony +7: power at the fifth x%.0f\n", b / a);
      CHECK(b > 30 * a, "no harmony at the fifth"); }
    { int32_t p;
      g.vx.hint = 100; g.vx.hmix = 100; g.vx.cho = g.vx.dly = g.vx.rev = g.vx.drv = g.vx.trem = 100; g.vx.obeat = 100; g.vx.fry = 100; g.vx.tens = 100;
      for (int i = 0; i < NTRK; i++) g.trk[i].level = 127;
      en_init(&en, &g); memset((void *)en.pan, 64, NTRK); en_post(&en, EN_PLAY, 0, 0, 0); render(44100 * 20);
      p = peak(buf, 2 * 44100 * 20); free(buf);
      en_post(&en, EN_STOP, 0, 0, 0); render(44100 * 2); free(buf);
      printf("  everything at 100 over 20 s of the demo: peak %d; after stop: voice gated %d/%d, queue %u\n", p, en.fv.gated, en.fv2.gated, (unsigned)en.vqn);
      CHECK(p <= 32767, "peak %d", p);
      CHECK(!en.fv.gated && !en.fv2.gated && en.vqn == 0, "a note is stuck after stop");
      song_demo(&g); }
}
/* Measure actual scheduler boundaries one sample at a time, including repeated random morae. */
static void t_animal(void) {
    static const uint32_t rate[3] = { 0, 50, 100 };
    for (uint32_t k = 0; k < 3; k++) {
        uint32_t last = 0, count = 0, min = 999999, max = 0, lo = (180u - rate[k] * 4u / 5u) * 441u / 10u;
        int32_t o[2];
        song_demo(&g); g.vx.lang = VL_ANIMAL;
        en_init(&en, &g); memset((void *)en.pan, 64, NTRK);
        CHECK(en.arate == 50, "animal default rate");
        en_post(&en, EN_RATE, rate[k], 0, 0);
        en_post(&en, EN_ON, TR_VOCAL, 64, 7);
        for (uint32_t t = 0; t < 88200; t++) {
            uint32_t before = en.bt;
            o[0] = o[1] = 0; en_render(&en, o, 1);
            if (!t) CHECK(en.bt + 1 >= lo && en.bt + 1 <= lo + 3528, "first animal interval out of range");
            if (t && en.bt > before) {
                uint32_t span = t + 1 - last;
                CHECK(span >= lo && span <= lo + 3528, "animal rate %u interval %u frames", rate[k], span);
                if (span < min) min = span;
                if (span > max) max = span;
                CHECK(en.vnote == 64 && en.fv.gated, "babble changed base note or released the key");
                last = t + 1; count++;
            }
        }
        printf("  animal rate %u: %u boundaries in 2 s, %.2f..%.2f ms\n", rate[k], count, min / 44.1, max / 44.1);
        CHECK(count >= 7 && count <= 20 && min < max, "animal rate/variation");
        /* Rate changes neither restart the timer nor force a release. Out-of-range command clamps. */
        { uint32_t before = en.bt, id = en.vcur;
          en_post(&en, EN_RATE, 255, 0, 0); o[0] = o[1] = 0; en_render(&en, o, 1);
          CHECK(en.arate == 100 && en.bt == before - 1 && en.vcur == id && en.fv.gated, "rate change retriggered the voice"); }
        /* Release immediately before a scheduled babble must win; no new mora may begin in the release tail. */
        while (en.bt > 1) { o[0] = o[1] = 0; en_render(&en, o, 1); }
        { uint32_t mora = en.mora; en_post(&en, EN_OFF, TR_VOCAL, 0, 0); render(44100);
          CHECK(!en.fv.gated && en.mora == mora, "animal retriggered after note-off");
          CHECK(peak(buf + 2 * 40000, 2 * 4100) == 0, "animal release did not die"); free(buf); }
    }
    /* A fast ANIMAL setting must not change the other language schedulers. */
    for (uint32_t lang = VL_JP; lang < VL_N; lang++) if (lang != VL_ANIMAL) {
        song_demo(&g); g.vx.lang = (uint8_t)lang; g.bpm = 120;
        en_init(&en, &g); en_post(&en, EN_RATE, 100, 0, 0); en_post(&en, EN_ON, TR_VOCAL, 64, 7);
        { int32_t o[2] = {0, 0}; en_render(&en, o, 1);
          CHECK(en.bt == (lang == VL_SCAT ? 11024u : 0u), "rate leaked into language %u", lang); }
    }
}
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
            CHECK(l == VL_ANIMAL ? ch >= 8 && ch <= 15 : l == VL_SCAT ? ch > 5 && ch < 15 : ch == 1, "language %d: %u syllables", l, (unsigned)ch);
            CHECK(l == VL_VOWEL ? !cons : cons, "language %d consonants", l);
        }
        g.vx.lang = VL_JP;
        free(ref);
    }
    t_layers();
    t_animal();
    printf(fails ? "engine: %d FAILED\n" : "engine: all ok\n", fails);
    /* HUSH (SAVE): PANIC plus a fast fade of the DX7 tails; en.act must drop within two blocks */
    {
        int32_t o[2 * 256]; int k;
        for (k = 0; k < 2; k++) {
            memcpy(g.dx7[0], dx7_bank[0], 128); g.trk[TR_CHORDS].preset = SG_DX7_FIRST; g.trk[TR_CHORDS].level = 100;
            en_init(&en, &g); memset((void *)en.pan, 64, NTRK);
            en_post(&en, EN_ON, TR_CHORDS, 60, 0); en_post(&en, EN_ON, TR_CHORDS, 64, 0);
            for (int i = 0; i < 40; i++) { memset(o, 0, sizeof o); en_render(&en, o, 256); }
            CHECK(en.act & 2u, "DX7 chord not active");
            en_post(&en, k ? EN_HUSH : EN_PANIC, 0, 0, 0);
            for (int i = 0; i < 2; i++) { memset(o, 0, sizeof o); en_render(&en, o, 256); }
            if (k) CHECK(!(en.act & 2u), "HUSH left the DX7 active");
            else CHECK(en.act & 2u, "PANIC alone is a normal release (still active)");
        }
    }
    return fails != 0;
}

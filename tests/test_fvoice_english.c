/* SPDX-License-Identifier: GPL-3.0-only */
/* fvoice English syllables (0.9): numbering, onset clusters, codas, diphthongs, visemes, bounds, determinism. */
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/audio/fvoice/fvoice_tables.h"
#include <stdio.h>
#include <string.h>
enum { AA = 1, AE, AH, AO, AW, AY, EH, ER, EY, IH, IY, OW, OY, UH, UW,
       B, CH, D, DH, F, G, HH, JH, K, L, M, N, NG, P, R, S, SH, T, TH, V, W, Y, Z, ZH, AX };
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int32_t buf[2 * 44100 * 3];
typedef struct { uint32_t n, peak, clip, tail_loud; uint8_t vis[64]; int nvis; uint64_t energy; } Run;
/* sing one syllable: note on, hold `hold` samples, coda, render until silent (or 1 s) */
static Run sing(FvVoice *v, const FvSyl *s, uint8_t note, uint32_t hold) {
    Run r; memset(&r, 0, sizeof r);
    memset(buf, 0, sizeof buf);
    fv_note_on_syl(v, note, s, 100);
    uint32_t t = 0;
    for (int phase = 0; phase < 2; phase++) {
        uint32_t end = phase == 0 ? hold : hold + 44100;
        while (t < end) {
            fv_render(v, buf + 2 * t, 64);
            uint8_t vi = fv_viseme(v);
            if (r.nvis == 0 || (r.vis[r.nvis - 1] != vi && r.nvis < 64)) r.vis[r.nvis++] = vi;
            t += 64;
            if (phase == 1 && !v->gated && !v->env && !v->gcur && !v->nl) break;
        }
        if (phase == 0) fv_note_off_syl(v, s);
    }
    r.n = t;
    for (uint32_t i = 0; i < 2 * t; i++) {
        int32_t x = buf[i] < 0 ? -buf[i] : buf[i];
        if ((uint32_t)x > r.peak) r.peak = (uint32_t)x;
        if (x >= 32767) r.clip++;
        r.energy += (uint64_t)((int64_t)buf[i] * buf[i]);
    }
    return r;
}
static int has(const Run *r, uint8_t vi) { for (int i = 0; i < r->nvis; i++) if (r->vis[i] == vi) return 1; return 0; }
int main(void) {
    /* numbering: the vowels are exactly 1..15 and 40 (AX) */
    for (int e = 0; e < FV_NEP; e++) {
        int vowel = (e >= AA && e <= UW) || e == AX;
        CHECK(vowel == (fv_epv[e] != 255), "vowel map %d", e);
        if (!vowel && e) CHECK(fv_epon[e] || e == NG || e == ZH, "onset class for consonant %d", e);
        if (!vowel && e) CHECK(fv_epco[e] || e == HH || e == W || e == Y, "coda class for consonant %d", e);
    }
    CHECK(fv_epon[NG] == 0, "NG never starts an English syllable");
    CHECK(fv_epv2[AY] != 255 && fv_epv2[OW] != 255 && fv_epv2[AA] == 255, "diphthongs");
    /* timing */
    FvSyl str = {{S, T, R}, IY, {M, 0, 0, 0}}, s1 = {{S, 0, 0}, IY, {0}}, plain = {{0}, AA, {0}};
    CHECK(fv_syl_onset_ms(&str) > fv_syl_onset_ms(&s1), "cluster onset longer");
    CHECK(fv_syl_onset_ms(&plain) == 0 && fv_syl_coda_ms(&plain) == 0, "a bare vowel has no onset/coda time");
    FvSyl nds = {{HH}, AE, {N, D, Z, 0}}, n1 = {{HH}, AE, {N, 0, 0, 0}};
    CHECK(fv_syl_coda_ms(&nds) > fv_syl_coda_ms(&n1) && fv_syl_coda_ms(&n1) > 0, "coda cluster longer");
    /* every vowel with every onset and coda: bounded, sounding, ends */
    for (int vw = AA; vw <= AX; vw = vw == UW ? AX : vw + 1)
        for (int c = B; c <= ZH; c++) {
            FvVoice v; fv_init(&v);
            FvSyl sy = {{(uint8_t)(c == NG ? 0 : c)}, (uint8_t)vw, {(uint8_t)(c == HH || c == W || c == Y ? 0 : c)}};
            Run r = sing(&v, &sy, (uint8_t)(55 + (vw * 3 + c) % 24), 12000);
            CHECK(r.clip == 0, "clip v%d c%d (%u)", vw, c, r.clip);
            CHECK(r.peak > 1500, "too quiet v%d c%d (%u)", vw, c, r.peak);
            CHECK(r.n < 12000 + 44100, "never ends v%d c%d", vw, c);
        }
    /* visemes: lips for m/b/p, teeth on lip for f/v, round for w/oo, open for ah */
    { FvVoice v; fv_init(&v); FvSyl m = {{M}, AA, {P}}; Run r = sing(&v, &m, 67, 15000);
      CHECK(has(&r, FVV_MBP) && has(&r, FVV_AH), "m-aa-p visemes"); CHECK(r.vis[r.nvis - 1] == FVV_REST || r.vis[r.nvis - 1] == FVV_MBP, "ends closed"); }
    { FvVoice v; fv_init(&v); FvSyl f = {{F}, AO, {V}}; Run r = sing(&v, &f, 67, 15000); CHECK(has(&r, FVV_FV) && has(&r, FVV_OH), "f-ao-v"); }
    { FvVoice v; fv_init(&v); FvSyl w = {{W}, UW, {0}}; Run r = sing(&v, &w, 67, 15000); CHECK(has(&r, FVV_OO), "w-uw"); }
    { FvVoice v; fv_init(&v); FvSyl l = {{L}, AY, {T}}; Run r = sing(&v, &l, 67, 30000); CHECK(has(&r, FVV_L) && has(&r, FVV_AH) && has(&r, FVV_EE), "l-ay-t glides ah -> ih"); }
    /* a nasal coda keeps sounding into the release; a stop coda goes silent before the burst */
    { FvVoice a; fv_init(&a); FvSyl hm = {{HH}, AA, {M}}; Run r1 = sing(&a, &hm, 64, 12000);
      FvVoice b; fv_init(&b); FvSyl ht = {{HH}, AA, {T}}; Run r2 = sing(&b, &ht, 64, 12000);
      CHECK(r1.n > r2.n, "the m hum lasts longer than the t (%u %u)", r1.n, r2.n); }
    /* determinism and legato: the same phrase twice is identical; a second syllable while gated glides */
    { uint64_t e[2]; for (int k = 0; k < 2; k++) { FvVoice v; fv_init(&v); Run r = sing(&v, &str, 69, 20000); e[k] = r.energy; } CHECK(e[0] == e[1], "deterministic"); }
    { FvVoice v; fv_init(&v); FvSyl a = {{HH}, EH, {0}}, b = {{L}, OW, {0}};
      fv_note_on_syl(&v, 64, &a, 100); fv_render(&v, buf, 8000); fv_note_on_syl(&v, 67, &b, 100);
      CHECK(v.gated && v.env > 0, "legato keeps the envelope"); fv_render(&v, buf, 8000); fv_note_off_syl(&v, &b); fv_render(&v, buf, 40000);
      CHECK(!v.gated, "released"); }
    /* the Japanese path is untouched by English state: a mora after an English syllable uses its own vowel */
    { FvVoice v; fv_init(&v); FvSyl d = {{0}, OY, {N}}; sing(&v, &d, 60, 9000); fv_note_on_mora(&v, 60, FC_K, FV_I, 100);
      CHECK(v.vsh == FV_I && v.vsh2 == 255 && v.oqn == 0 && v.kqn == 0 && !v.coda, "mora after English"); }
    printf(fails ? "fvoice_english: %d FAILED\n" : "fvoice_english: ok\n", fails);
    return fails != 0;
}

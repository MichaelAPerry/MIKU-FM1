/* SPDX-License-Identifier: GPL-3.0-only */
/* English through the real sequencer and engine (0.9): song syllables, codas at note-off, HOLD, legato coda carry,
   live slots (the PHRASE page), visemes, the harmony voice, bounds; and a song render for listening. */
#include "firmware/app/engine.h"
#include "firmware/song/english.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static Song g; static Engine e;
static int32_t audio[2 * 44100 * 8];
/* text -> the song's syllable table + one VOCAL step per syllable (2 sixteenths each) on a rising scale */
static int song_from(const char *text, int legato) {
    static EnSylOut o[48]; int n = 0, err = 0, k;
    static const uint8_t scale[8] = {60, 62, 64, 65, 67, 69, 71, 72};
    sg_init(&g); g.loop = 0; g.bpm = 120;
    if (en_text_ex(text, EN_SUNG, 0, 0, o, 48, &n, &err)) return -1;
    if (n > 8) n = 8;
    for (k = 0; k < n; k++) {
        uint8_t ph[8]; int i;
        for (i = 0; i < 3; i++) ph[i] = o[k].s.on[i];
        ph[3] = o[k].s.v == EP_AH && o[k].stress == 0 ? FV_EP_AX : o[k].s.v;
        for (i = 0; i < 4; i++) ph[4 + i] = o[k].s.co[i];
        int m = sg_syl_add(&g, ph, o[k].text);
        Step s = {{scale[k], 2, (uint8_t)m, (uint8_t)(12 | (o[k].flags & EN_WORD_END ? VF_WORD_END : 0) | (legato && k + 1 < n ? VF_LEGATO : 0))}};
        if (m < 0 || !sg_set_step(&g, TR_VOCAL, 0, k * 2, &s)) return -2;
    }
    sg_row_insert(&g, 0); sg_row_set(&g, 0, TR_VOCAL, 0);
    return n;
}
static void init(void) { memset(&e, 0, sizeof e); en_init(&e, &g); for (unsigned i = 0; i < NTRK; i++) e.pan[i] = 64; }
static void render(uint32_t frames, uint32_t *vis_seen, uint32_t *eng_seen) {
    for (uint32_t at = 0; at < frames;) {
        uint32_t n = frames - at < 256 ? frames - at : 256;
        en_render(&e, audio + 2 * at, n); at += n;
        if (vis_seen) *vis_seen |= 1u << e.vis;
        if (eng_seen) *eng_seen |= e.eng;
    }
}
static uint32_t peak(uint32_t frames) { uint32_t p = 0; for (uint32_t i = 0; i < 2 * frames; i++) { int32_t x = audio[i] < 0 ? -audio[i] : audio[i]; if ((uint32_t)x > p) p = (uint32_t)x; } return p; }
int main(int argc, char **argv) {
    uint32_t vis = 0, eng = 0;
    /* a song: sings, shows English visemes, ends silent */
    int n = song_from("Miku sings for you", 0);
    CHECK(n >= 5, "syllables %d", n);
    init(); en_post(&e, EN_PLAY, 0, 0, 0);
    render(44100 * 3, &vis, &eng);
    CHECK(eng, "English path taken");
    CHECK(vis & (1u << FVV_FV) && vis & (1u << FVV_OO), "visemes f/oo seen (%x)", vis);
    CHECK(peak(44100 * 3) > 2000 && peak(44100 * 3) < 32767 * 2, "level %u", peak(44100 * 3));
    render(44100 * 2, 0, 0);
    CHECK(!e.fv.gated && !e.fv.env, "silent at the end");
    /* the coda plays at note-off: "for" ends in R, "sings" in NG Z */
    { uint32_t seen_coda = 0; init(); en_post(&e, EN_PLAY, 0, 0, 0);
      for (int t = 0; t < 44100 * 3 / 64; t++) { int32_t out[128]; memset(out, 0, sizeof out); en_render(&e, out, 64); if (e.fv.coda || e.fv.kqn) seen_coda = 1; }
      CHECK(seen_coda, "a coda played"); }
    /* legato carries the previous coda into the next onset (no note-off between) */
    CHECK(song_from("world is", 1) == 2, "two syllables");
    init(); en_post(&e, EN_PLAY, 0, 0, 0); render(44100 / 2 + 2000, 0, 0);
    CHECK(e.eng && e.esyl.on[0] != 0, "worl-dis: the onset got the carried coda (%d %d %d)", e.esyl.on[0], e.esyl.on[1], e.esyl.on[2]);
    /* live slots: EN_ON with EN_LIVE0+k sings slot k; EN_OFF plays its coda */
    { FvSyl s = {{EP_S, EP_T, EP_R}, EP_IY, {EP_M, 0, 0, 0}}; vis = 0;
      sg_init(&g); init(); en_set_live(&e, 3, &s);
      en_post(&e, EN_ON, TR_VOCAL, 67, EN_LIVE0 + 3); render(20000, &vis, 0);
      CHECK(e.eng && e.esyl.v == EP_IY && e.fv.gated, "live slot sounds");
      en_post(&e, EN_OFF, TR_VOCAL, 0, 0); render(30000, &vis, 0);
      CHECK(vis & (1u << FVV_MBP), "its m closes the lips");
      CHECK(!e.fv.gated, "released"); }
    /* the harmony voice sings the same syllable a third up */
    n = song_from("hello", 0); g.vx.hint = 40; g.vx.hmix = 80;
    init(); en_post(&e, EN_PLAY, 0, 0, 0); render(8000, 0, 0);
    CHECK(e.fv2.gated && e.fv2.vsh == e.fv.vsh, "harmony follows (%d %d)", e.fv2.vsh, e.fv.vsh);
    /* a Japanese step after English goes back to the mora path */
    n = song_from("la", 0); { Step s = {{64, 2, 12, 12}}; sg_set_step(&g, TR_VOCAL, 0, 4, &s); }
    init(); en_post(&e, EN_PLAY, 0, 0, 0); render(44100 * 6 / 4, 0, 0);
    CHECK(!e.eng, "Japanese again");
    /* optional: render a song for listening */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb"); uint32_t frames = 44100 * 7;
        song_from(argc > 2 ? argv[2] : "Hello world I sing for you", 0);
        g.bpm = 100; init(); en_post(&e, EN_PLAY, 0, 0, 0); render(frames, 0, 0);
        if (f) { /* raw stereo 16-bit; convert with ffmpeg -f s16le -ar 44100 -ac 2 */
            for (uint32_t i = 0; i < 2 * frames; i++) { int32_t x = audio[i] > 32767 ? 32767 : audio[i] < -32768 ? -32768 : audio[i]; int16_t y = (int16_t)x; fwrite(&y, 2, 1, f); }
            fclose(f);
        }
    }
    printf(fails ? "english_engine: %d FAILED\n" : "english_engine: ok\n", fails);
    return fails != 0;
}

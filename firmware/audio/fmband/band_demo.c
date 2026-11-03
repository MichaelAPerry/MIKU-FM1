/* SPDX-License-Identifier: GPL-3.0-only */
/* Host render tool (not a test): usage band_demo [outdir]   (default .) -> band_demo.wav, presets.wav
   band_demo.wav: 4 bars @148 BPM, C major: C F Am G, FM-bells up/down sixteenth arps, accented eighth bass, four-on-the-floor kit.
   presets.wav: every chord preset (C major, 1 s), every bass preset, every drum of both kits. */
#include "firmware/audio/fmband/fmband.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FS 44100
typedef struct { ChSynth c; BsSynth b; DrKit d; } Band;
static int16_t *pcm; static size_t npcm, cap;
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> (8 * i)) & 255, f); }
static int save(const char *dir, const char *name) {
    char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return 0; }
    fwrite("RIFF", 1, 4, f); w32(f, (uint32_t)(36 + npcm * 4)); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w32(f, 0x20001); w32(f, FS); w32(f, FS * 4); w32(f, 0x100004); fwrite("data", 1, 4, f); w32(f, (uint32_t)(npcm * 4));
    fwrite(pcm, 4, npcm, f); fclose(f); printf("wrote %s (%.1f s)\n", path, (double)npcm / FS); return 1;
}
static void run(Band *w, uint32_t n) {                 /* render n frames and append */
    static int32_t o[2 * 256];
    while (n) {
        uint32_t m = n < 256 ? n : 256;
        memset(o, 0, sizeof o);
        ch_render(&w->c, o, m); bs_render(&w->b, o, m); dr_render(&w->d, o, m); mx_limit(o, m);
        if (npcm + m > cap) { cap = cap ? cap * 2 : 1 << 20; pcm = realloc(pcm, cap * 4); if (!pcm) exit(1); }
        for (uint32_t i = 0; i < m; i++, npcm++) { pcm[2 * npcm] = (int16_t)o[2 * i]; pcm[2 * npcm + 1] = (int16_t)o[2 * i + 1]; }
        n -= m;
    }
}
static void band_init(Band *w) {
    memset(w, 0, sizeof *w); ch_init(&w->c); bs_init(&w->b); dr_init(&w->d);
    ch_set_mix(&w->c, 115, 44); bs_set_mix(&w->b, 108, 64); dr_set_mix(&w->d, 96, 64);
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    static Band w;
    /* ---- 4-bar song ---- */
    band_init(&w); ch_set_preset(&w.c, 0); bs_set_preset(&w.b, 0);
    static const uint8_t chord[4][4] = { { 60, 64, 67, 72 }, { 60, 65, 69, 72 }, { 60, 64, 69, 72 }, { 59, 62, 67, 71 } };
    static const uint8_t root[4] = { 36, 41, 33, 43 };
    static const uint8_t ud[16] = { 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0, 1, 2, 1 };   /* up-down sixteenths */
    int steps = 64 * 4;                                       /* 4 bars + 3 bars of ring-out below */
    size_t pos = 0;
    for (int s = 0; s < steps; s++) {
        size_t next = (size_t)((long long)(s + 1) * FS * 15 / 148);
        int bar = (s >> 4) & 3, st = s & 15;
        if (s < 64) {
            uint8_t nt = chord[bar][ud[st]];
            ch_note_on(&w.c, nt, st % 4 == 0 ? 112 : 84);
            if (!(st & 1)) bs_note_on(&w.b, root[bar], 100, (st & 7) == 6 || (st == 14), (st & 7) == 2 && st != 2);
            if (st % 4 == 0) dr_trig(&w.d, DR_KICK, 120);
            if (st == 4 || st == 12) { dr_trig(&w.d, DR_SNARE, 115); dr_trig(&w.d, DR_CLAP, 100); }
            if (!(st & 1)) dr_trig(&w.d, st == 14 ? DR_OHH : DR_CHH, st % 4 == 2 ? 105 : 80);
        } else if (s == 64) { ch_all_off(&w.c); bs_note_off(&w.b); }
        run(&w, (uint32_t)(next - pos)); pos = next;
        if (s < 64) { ch_note_off(&w.c, chord[bar][ud[st]]); if (st & 1) bs_note_off(&w.b); }
        if (s >= 64 + 8) break;
    }
    if (!save(dir, "band_demo.wav")) return 1;
    /* ---- preset tour ---- */
    npcm = 0; band_init(&w);
    for (int p = 0; p < CH_NPRESET; p++) {
        ch_set_preset(&w.c, (uint8_t)p);
        ch_note_on(&w.c, 60, 100); ch_note_on(&w.c, 64, 100); ch_note_on(&w.c, 67, 100);
        run(&w, FS); ch_all_off(&w.c); run(&w, FS * 6 / 10);
    }
    for (int p = 0; p < BS_NPRESET; p++) {
        bs_set_preset(&w.b, (uint8_t)p);
        bs_note_on(&w.b, 36, 100, 0, 0); run(&w, FS / 2); bs_note_on(&w.b, 43, 110, 1, 1); run(&w, FS / 2);
        bs_note_on(&w.b, 31, 100, 0, 0); run(&w, FS / 2); bs_note_off(&w.b); run(&w, FS / 2);
    }
    for (int p = 0; p < DR_NPRESET; p++) {
        dr_set_preset(&w.d, (uint8_t)p);
        for (int i = 0; i < DR_N; i++) { dr_trig(&w.d, (uint8_t)i, 110); run(&w, FS * 7 / 10); }
    }
    int ok = save(dir, "presets.wav");
    free(pcm); return ok ? 0 : 1;
}

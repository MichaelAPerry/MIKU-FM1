/* SPDX-License-Identifier: GPL-3.0-only */
/* Host demo: renders a short phrase and the five vowels with fvoice to 16-bit 44.1 kHz WAVs.
   usage: fvoice_demo [-t semitones] [phrase.wav [vowels.wav]]   (-t transposes the phrase only) */
#include "firmware/audio/fvoice/fvoice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FS 44100
static void w16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void w32(FILE *f, unsigned v) { w16(f, v & 65535); w16(f, v >> 16); }
static int wav(const char *path, const int32_t *b, uint32_t n) {
    FILE *f = fopen(path, "wb"); if (!f) return 0;
    fwrite("RIFF", 1, 4, f); w32(f, 36 + n * 4); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w16(f, 1); w16(f, 2); w32(f, FS); w32(f, FS * 4); w16(f, 4); w16(f, 16);
    fwrite("data", 1, 4, f); w32(f, n * 4);
    for (uint32_t i = 0; i < n * 2; i++) { int32_t s = b[i]; w16(f, (unsigned)(s > 32767 ? 32767 : s < -32768 ? -32768 : s) & 65535); }
    return fclose(f) == 0;
}
int main(int argc, char **argv) {
    int tr = 0;
    if (argc > 2 && !strcmp(argv[1], "-t")) { tr = atoi(argv[2]); argc -= 2; argv += 2; }
    static const uint8_t note[] = {67, 67, 69, 72, 69, 67, 64, 67, 69, 69, 72, 74, 76, 74, 72};
    static const uint8_t vow[] = {FV_O, FV_O, FV_O, FV_A, FV_O, FV_O, FV_A, FV_A, FV_A, FV_A, FV_I, FV_A, FV_A, FV_U, FV_O};
    static const uint8_t len[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};   /* eighths; the last one is held */
    uint32_t eighth = FS * 60 / 296, total = eighth * 18 + FS / 2;
    int32_t *b = calloc((size_t)(total + FS * 4) * 2, sizeof *b);
    if (!b) return 1;
    FvVoice v; fv_init(&v);
    v.vib_delay = 120; v.porta = 50; v.release = 90; v.breath = 10; v.transpose = (int8_t)tr;
    uint32_t t = 0;
    for (unsigned i = 0; i < sizeof note; i++) {
        v.vib_depth = len[i] > 1 ? 45 : 14;
        int rep = i != 0;                                  /* every syllable is re-articulated (also restarts the onset scoop) */
        if (rep) { fv_note_off(&v); fv_render(&v, b + 2 * t, 900); t += 900; }
        fv_note_on(&v, note[i], vow[i], (uint8_t)(i % 8 == 0 ? 120 : 100));
        uint32_t n = eighth * len[i] - (rep ? 900 : 0);
        fv_render(&v, b + 2 * t, n); t += n;
    }
    fv_note_off(&v);
    fv_render(&v, b + 2 * t, total - t);
    int ok = wav(argc > 1 ? argv[1] : "fvoice_demo.wav", b, total);
    memset(b, 0, (size_t)total * 8);
    fv_init(&v); v.vib_depth = 35; v.vib_delay = 100; v.release = 80; t = 0;
    for (uint8_t w = 0; w < 5; w++) {   /* A I U E O, 0.7 s each on note 74, legato with vibrato */
        fv_note_on(&v, 74, w, 110); fv_render(&v, b + 2 * t, FS * 7 / 10); t += FS * 7 / 10;
    }
    fv_note_off(&v); fv_render(&v, b + 2 * t, FS / 2); t += FS / 2;
    ok = ok && wav(argc > 2 ? argv[2] : "fvoice_vowels.wav", b, t);
    free(b);
    return ok ? 0 : 1;
}

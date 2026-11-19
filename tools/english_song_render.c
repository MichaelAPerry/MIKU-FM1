/* SPDX-License-Identifier: GPL-3.0-only */
/* An English song from the actual firmware engine: GEN writes the song (band and vocal melody), then every vocal note keeps
   its pitch and timing but sings the next English syllable of the lyrics: the lines are one stream, and each vocal pattern,
   in order of first appearance, takes the next syllables (wrapping at the end); a repeated pattern repeats its words.
   HOLD notes keep their syllable's vowel.
   Usage: english_song_render PREFIX STYLE SEED ENERGY COMPLEXITY "line 1" ["line 2" ...]   -> PREFIX.wav, PREFIX.mks */
#include "firmware/song/gen_next.h"
#include "firmware/song/english.h"
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static Song song; static Engine engine;
static void w32(FILE *f, uint32_t n) { for (int i = 0; i < 4; i++) fputc((int)(n >> (i * 8)) & 255, f); }
typedef struct { int mora[48]; uint8_t wend[48]; int n; } Line;
static int compile(const char *text, Line *l) {
    static EnSylOut o[48]; int n = 0, err = 0;
    if (en_text_ex(text, EN_SUNG, 0, 0, o, 48, &n, &err)) { fprintf(stderr, "cannot read \"%s\" at %d\n", text, err); return 0; }
    for (int k = 0; k < n; k++) {
        uint8_t ph[8];
        for (int i = 0; i < 3; i++) ph[i] = o[k].s.on[i];
        ph[3] = o[k].s.v == EP_AH && o[k].stress == 0 ? FV_EP_AX : o[k].s.v;
        for (int i = 0; i < 4; i++) ph[4 + i] = o[k].s.co[i];
        if ((l->mora[k] = sg_syl_add(&song, ph, o[k].text)) < 0) { fprintf(stderr, "syllable table full\n"); return 0; }
        l->wend[k] = (o[k].flags & EN_WORD_END) != 0;
    }
    l->n = n;
    return n > 0;
}
int main(int argc, char **argv) {
    if (argc < 7) { fprintf(stderr, "usage: english_song_render PREFIX STYLE SEED ENERGY COMPLEXITY \"line\" ...\n"); return 1; }
    GenNextCfg c = {(uint16_t)atoi(argv[3]), (uint8_t)atoi(argv[2]), 9, 1, 0, (uint8_t)atoi(argv[4]), (uint8_t)atoi(argv[5])};
    sg_init(&song); if (!gen_next_song(&song, &c)) return 2;
    static Line lines[16]; int nl = 0;
    for (int a = 6; a < argc && nl < 16; a++) if (!compile(argv[a], &lines[nl++])) return 2;
    /* the vocal patterns in order of first appearance */
    int order[NPAT], seen[NPAT] = {0}, np = 0;
    for (uint32_t r = 0; r < song.nrows; r++) { uint32_t p = song.row[r].pat[TR_VOCAL]; if (p < NPAT && !seen[p]) { seen[p] = 1; order[np++] = (int)p; } }
    static int smora[16 * 48]; static uint8_t swend[16 * 48]; int sn = 0, at = 0, notes = 0;
    for (int k = 0; k < nl; k++) for (int i = 0; i < lines[k].n; i++) { smora[sn] = lines[k].mora[i]; swend[sn++] = lines[k].wend[i]; }
    for (int k = 0; k < np; k++) {
        int last = -1;
        for (int s = 0; s < NSTEP; s++) {
            Step *st = &song.pat[TR_VOCAL][order[k]][s];
            if (!st->v[SV_NOTE]) continue;
            if ((st->v[SV_FLAG] & VF_SUSTAIN) && last >= 0) { st->v[SV_MORA] = (uint8_t)last; continue; }   /* a held vowel */
            st->v[SV_MORA] = (uint8_t)smora[at % sn];
            st->v[SV_FLAG] = (uint8_t)((st->v[SV_FLAG] & ~(VF_WORD_END | VF_SUSTAIN)) | (swend[at % sn] ? VF_WORD_END : 0));
            last = st->v[SV_MORA]; at++; notes++;
        }
    }
    char path[1024]; static uint8_t data[SONG_ENC_CAP];
    sg_syl_compact(&song);
    snprintf(path, sizeof path, "%s.mks", argv[1]);
    FILE *f = fopen(path, "wb"); if (!f) return 3;
    uint32_t n = song_encode(&song, data, sizeof data); fwrite(data, 1, n, f); fclose(f);
    if (!n || song_decode(&song, data, n) != SIO_OK) { fprintf(stderr, "the song does not round-trip\n"); return 5; }
    song.loop = 0; en_init(&engine, &song); for (int t = 0; t < NTRK; t++) engine.pan[t] = 64;
    en_post(&engine, EN_PLAY, 0, 0, 0);
    snprintf(path, sizeof path, "%s.wav", argv[1]); f = fopen(path, "wb"); if (!f) return 3;
    fwrite("RIFF", 1, 4, f); w32(f, 0); fwrite("WAVEfmt ", 1, 8, f); w32(f, 16); w32(f, 0x20001); w32(f, 44100); w32(f, 176400); w32(f, 0x100004); fwrite("data", 1, 4, f); w32(f, 0);
    uint32_t frames = 0, tail = 3 * 44100, cap = 44100 * 180, clips = 0; int peak = 0; double sum = 0;
    while (frames < cap) {
        int32_t out[512] = {0}; int16_t pcm[512]; en_render(&engine, out, 256);
        for (int i = 0; i < 512; i++) { int x = out[i]; if (abs(x) > peak) peak = abs(x); if (x >= 32767 || x <= -32768) clips++; sum += (double)x * x; pcm[i] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x); }
        fwrite(pcm, 4, 256, f); frames += 256;
        if (!(engine.pos >> 16 & 1u) && frames > 4096) { if (tail <= 256) break; tail -= 256; }
    }
    fseek(f, 4, SEEK_SET); w32(f, 36 + frames * 4); fseek(f, 40, SEEK_SET); w32(f, frames * 4); fclose(f);
    printf("%s seed %u: %u rows, %d vocal patterns, %d sung notes, %u syllables, %.1fs, peak %d, rms %.0f, clipped %u, song %u bytes (v%u)\n",
           gen_next_style(c.style), c.seed, song.nrows, np, notes, song.nsyl, frames / 44100., peak, sqrt(sum / (frames * 2.)), clips, n, data[4]);
    return clips ? 4 : 0;
}

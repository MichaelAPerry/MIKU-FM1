/* SPDX-License-Identifier: GPL-3.0-only */
/* Host render of a song through the device engine (firmware/app/engine.c): song_render [out.wav] [song.mks]
 * Default: the built-in demo 「おとのはこ」, once through (no loop), plus a 3 s tail. The WAV is 44.1 kHz 16-bit stereo.
 * Options (host tool only; the song's own solo flag and PLAY row are what the device uses too):
 *   --solo vocal|chords|bass|drums   solo that track    --rows A B   play from row A and cut exactly at the end of row B (0-based), 10 ms fade
 *   --gen STYLE KEY SCALE SEED       render a generated song instead (firmware/song/gen.h: style 0-3 POP BALLAD DANCE CHIP, key 0-11, scale 0 major 1 minor)
 *   --vx name=v,...                  the voice's sound (Voice in song.h): atk rel scoop human vrate vdel trem cho dly rev drv chr lang,
 *                                    tens fry whsp fmt rsize dtime dfb crate clen clev vspd obeat hint hmix hdet hchr */
#include "firmware/app/engine.h"
#include "firmware/song/gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Song g; static Engine en;
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> (8 * i)) & 255, f); }
int main(int argc, char **argv) {
    const char *path = "otonohako.wav", *file = 0;
    int solo = -1, r0 = 0, r1 = 255, np = 0, gen = 0;
    GenCfg gc = { 0, 0, 0, 0, 0 };
    const char *vxs = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--solo") && i + 1 < argc) {
            const char *t = argv[++i];
            solo = !strcmp(t, "vocal") ? TR_VOCAL : !strcmp(t, "chords") ? TR_CHORDS : !strcmp(t, "bass") ? TR_BASS : !strcmp(t, "drums") ? TR_DRUMS : -1;
            if (solo < 0) { fprintf(stderr, "song_render: --solo vocal|chords|bass|drums\n"); return 1; }
        } else if (!strcmp(argv[i], "--gen") && i + 4 < argc) {
            gc.style = (uint8_t)atoi(argv[i + 1]); gc.key = (uint8_t)atoi(argv[i + 2]); gc.scale = (uint8_t)atoi(argv[i + 3]); gc.seed = (uint16_t)atoi(argv[i + 4]); gen = 1; i += 4;
        } else if (!strcmp(argv[i], "--vx") && i + 1 < argc) vxs = argv[++i];
        else if (!strcmp(argv[i], "--rows") && i + 2 < argc) { r0 = atoi(argv[i + 1]); r1 = atoi(argv[i + 2]); i += 2; }
        else if (np == 0) { path = argv[i]; np++; }
        else if (np == 1) { file = argv[i]; np++; }
    }
    static int32_t o[2 * 256];
    static int16_t blk[2 * 256];
    uint32_t frames = 0, tail = 3 * 44100u, maxf = 44100u * 400u;
    song_demo(&g);
    if (file) {
        static uint8_t b[SONG_ENC_SIZE];
        FILE *s = fopen(file, "rb");
        size_t nb = s ? fread(b, 1, sizeof b, s) : 0;
        if (!s || song_decode(&g, b, (uint32_t)nb) != SIO_OK) { fprintf(stderr, "song_render: cannot load %s\n", file); return 1; }
        fclose(s);
    }
    if (gen) gen_song(&g, &gc);
    while (vxs && *vxs) {                          /* --vx name=v,... */
        static const char *const nm[32] = { "atk", "rel", "scoop", "human", "vrate", "vdel", "trem", "cho", "dly", "rev", "drv", "chr", "lang",
            "", "", "", "tens", "fry", "whsp", "fmt", "rsize", "dtime", "dfb", "crate", "clen", "clev", "vspd", "obeat", "hint", "hmix", "hdet", "hchr" };   /* Voice field order */
        char k[8]; int n = 0, v, j;
        while (*vxs && *vxs != '=' && n < 7) k[n++] = *vxs++;
        k[n] = 0;
        if (*vxs++ != '=') break;
        v = atoi(vxs);
        while (*vxs && *vxs != ',') vxs++;
        if (*vxs == ',') vxs++;
        for (j = 0; j < 32 && (!*nm[j] || strcmp(k, nm[j])); j++) {}
        if (j == 32) { fprintf(stderr, "song_render: --vx: unknown %s\n", k); return 1; }
        ((uint8_t *)&g.vx)[j] = (uint8_t)v;
    }
    g.loop = 0;
    if (solo >= 0) g.trk[solo].solo = 1;
    en_init(&en, &g);
    en.pan[0] = 64; en.pan[1] = 44; en.pan[2] = 64; en.pan[3] = 64;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF", 1, 4, f); w32(f, 0); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w32(f, 0x20001); w32(f, 44100); w32(f, 44100 * 4); w32(f, 0x100004); fwrite("data", 1, 4, f); w32(f, 0);
    if (r1 < 255) maxf = (uint32_t)((double)(r1 - r0 + 1) * 4.0 * 60.0 * 44100.0 / g.bpm + 0.5);   /* one row = 4 beats = 16 steps */
    en_post(&en, EN_PLAY, (uint32_t)r0, 0, 0);
    int32_t peak = 0;
    while (frames < maxf) {
        memset(o, 0, sizeof o);
        en_render(&en, o, 256);
        uint32_t nb = maxf - frames < 256 ? maxf - frames : 256;
        for (uint32_t i = 0; i < 512; i++) {
            uint32_t left = maxf - frames - i / 2;                 /* frames to the cut (host tool: --rows) */
            if (r1 < 255 && left < 441) o[i] = (int32_t)((double)o[i] * left / 441.0);
            blk[i] = (int16_t)o[i]; if (abs(o[i]) > peak) peak = abs(o[i]);
        }
        fwrite(blk, 4, nb, f); frames += nb;
        if (nb < 256) break;
        if (!(en.pos >> 16 & 1u) && frames > 4096) { if (tail <= 256) break; tail -= 256; }
    }
    fseek(f, 4, SEEK_SET); w32(f, 36 + frames * 4); fseek(f, 40, SEEK_SET); w32(f, frames * 4); fclose(f);
    printf("song_render: %s %.1f s peak %d\n", path, (double)frames / 44100.0, peak);
    return 0;
}

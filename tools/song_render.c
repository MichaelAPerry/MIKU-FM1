/* SPDX-License-Identifier: GPL-3.0-only */
/* Host render of a song through the device engine (firmware/app/engine.c): song_render [out.wav] [song.mks]
 * Default: the built-in demo 「おとのはこ」, once through (no loop), plus a 3 s tail. The WAV is 44.1 kHz 16-bit stereo. */
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Song g; static Engine en;
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> (8 * i)) & 255, f); }
int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "otonohako.wav";
    static int32_t o[2 * 256];
    static int16_t blk[2 * 256];
    uint32_t frames = 0, tail = 3 * 44100u, maxf = 44100u * 400u;
    song_demo(&g);
    if (argc > 2) {
        static uint8_t b[SONG_ENC_SIZE];
        FILE *s = fopen(argv[2], "rb");
        if (!s || fread(b, 1, sizeof b, s) != sizeof b || song_decode(&g, b, sizeof b) != SIO_OK) { fprintf(stderr, "song_render: cannot load %s\n", argv[2]); return 1; }
        fclose(s);
    }
    g.loop = 0;
    en_init(&en, &g);
    en.pan[0] = 64; en.pan[1] = 44; en.pan[2] = 64; en.pan[3] = 64;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF", 1, 4, f); w32(f, 0); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w32(f, 0x20001); w32(f, 44100); w32(f, 44100 * 4); w32(f, 0x100004); fwrite("data", 1, 4, f); w32(f, 0);
    en_post(&en, EN_PLAY, 0, 0, 0);
    int32_t peak = 0;
    while (frames < maxf) {
        memset(o, 0, sizeof o);
        en_render(&en, o, 256);
        for (uint32_t i = 0; i < 512; i++) { blk[i] = (int16_t)o[i]; if (abs(o[i]) > peak) peak = abs(o[i]); }
        fwrite(blk, 4, 256, f); frames += 256;
        if (!(en.pos >> 16 & 1u) && frames > 4096) { if (tail <= 256) break; tail -= 256; }
    }
    fseek(f, 4, SEEK_SET); w32(f, 36 + frames * 4); fseek(f, 40, SEEK_SET); w32(f, frames * 4); fclose(f);
    printf("song_render: %s %.1f s peak %d\n", path, (double)frames / 44100.0, peak);
    return 0;
}

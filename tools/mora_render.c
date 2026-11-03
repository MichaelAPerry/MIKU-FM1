/* SPDX-License-Identifier: GPL-3.0-only */
/* Host render of every mora in table order through the device engine (one note each, default note 72, 250 ms + a 120 ms gap):
 * mora_render [out.wav] [note] */
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Song g; static Engine en;
static void w32(FILE *f, uint32_t v) { for (int i = 0; i < 4; i++) fputc((int)(v >> (8 * i)) & 255, f); }
int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "morae.wav";
    uint32_t note = argc > 2 ? (uint32_t)atoi(argv[2]) : 72u, frames = 0, m, k;
    static int32_t o[2 * 256];
    static int16_t blk[2 * 256];
    song_demo(&g); g.loop = 0;
    en_init(&en, &g);
    en.pan[0] = 64; en.pan[1] = 44; en.pan[2] = 64; en.pan[3] = 64;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 1; }
    fwrite("RIFF", 1, 4, f); w32(f, 0); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w32(f, 0x20001); w32(f, 44100); w32(f, 44100 * 4); w32(f, 0x100004); fwrite("data", 1, 4, f); w32(f, 0);
    int32_t peak = 0;
    for (m = 0; m < MORA_N; m++) {
        for (k = 0; k < (250u + 120u) * 44100u / 1000u / 256u + 1u; k++) {
            if (k == 0) { en_post(&en, EN_ON, TR_VOCAL, note, m); }
            if (k == 250u * 44100u / 1000u / 256u) en_post(&en, EN_OFF, TR_VOCAL, note, 0);
            memset(o, 0, sizeof o);
            en_render(&en, o, 256);
            for (uint32_t i = 0; i < 512; i++) { blk[i] = (int16_t)o[i]; if (abs(o[i]) > peak) peak = abs(o[i]); }
            fwrite(blk, 4, 256, f); frames += 256;
        }
    }
    fseek(f, 4, SEEK_SET); w32(f, 36 + frames * 4); fseek(f, 40, SEEK_SET); w32(f, frames * 4); fclose(f);
    printf("mora_render: %s %u morae note %u %.1f s peak %d\n", path, MORA_N, (unsigned)note, (double)frames / 44100.0, peak);
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/mkx.h"
#include <stdio.h>

static void hex(const uint8_t *data, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) printf("%02x", (unsigned)data[i]);
    putchar('\n');
}

int main(void)
{
    uint8_t raw[MKX_PAYLOAD_MAX], frame[MKX_FRAME_CAP], record[PH_REC];
    Phrase p = {.bpm = 136, .deflen = 3, .name = "MIKU 01     "};
    for (uint32_t i = 0; i < sizeof raw; i++) raw[i] = (uint8_t)(i * 173u + 0xb5u);
    for (uint32_t n = 0; n <= 3000; n += n == 20 ? 2980 : 1) {
        int size = mkx_pack(raw, n, frame, sizeof frame);
        if (size < 0) return 1;
        printf("%u %08x\n", (unsigned)n, (unsigned)sg_crc32(raw, n));
        hex(frame, (uint32_t)size);
        size = mkx_frame_build(MKX_SONG_CHUNK, 127, raw, n, frame, sizeof frame);
        if (size < 0) return 1;
        hex(frame, (uint32_t)size);
    }
    for (uint32_t lang = PH_EN; lang <= PH_JP; lang++) {
        const char *lyrics = lang == PH_EN ? "Hello, Miku!" : "mi-ku:2 _:2 | a!";
        if (ph_compile(lyrics, lang, &p, 0)) return 1;
        p.flags = PH_MELODY;
        p.s[0].note = 66;
        p.s[0].len |= 3;
        if (ph_encode(&p, record)) return 1;
        hex(record, sizeof record);
    }
    return 0;
}

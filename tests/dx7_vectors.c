/* SPDX-License-Identifier: GPL-3.0-only */
/* Emits JSON test vectors from the firmware's own DX7 SysEx code (firmware/song/dx7_sysex.c, dx_patch_valid) for
 * tests/test_bank_tool.js, which runs site/js/dx7syx.js against them, and for tests/test_bank_tool.py. Usage: dx7_vectors > vectors.json */
#include <stdio.h>
#include <string.h>
#include "firmware/song/dx7_sysex.h"
#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"

static uint32_t rng = 12345u;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static void arr(const uint8_t *p, uint32_t n) {
    uint32_t i;
    putchar('[');
    for (i = 0; i < n; i++) printf(i ? ",%u" : "%u", p[i]);
    putchar(']');
}
static void mask(uint8_t *p) {      /* dxs_mask is static in the firmware: this is the same list */
    uint32_t o;
    for (o = 0; o < 6u; o++) { uint8_t *q = p + 17u * o; q[11] &= 15u; q[13] &= 31u; q[15] &= 63u; }
    p[111] &= 15u;
}

int main(void) {
    uint8_t v[DXS_VCED], bank[DXS_BANK_BYTES], frame[DXS_BULK_FRAME], single[DXS_SINGLE_FRAME], init[DXS_VOICE];
    uint32_t i, k, n, first = 1;
    printf("{\n\"init\":"); dxs_init_voice(init); arr(init, DXS_VOICE);
    /* the factory bank padded with INIT VOICE, and its bulk message (F0 and F7 included) */
    for (k = 0; k < DXS_BANK_N; k++) { if (k < DX7_BANK_N) memcpy(bank + k * DXS_VOICE, dx7_bank[k], DXS_VOICE); else memcpy(bank + k * DXS_VOICE, init, DXS_VOICE); }
    dxs_build_bulk(bank, 0, frame);
    printf(",\n\"bulkMessage\":["); printf("%u,", 0xF0u);
    for (i = 0; i < DXS_BULK_FRAME; i++) printf("%u,", frame[i]);
    printf("%u]", 0xF7u);
    /* a single-voice message for factory voice 3 (VCED, channel 5) */
    dxs_packed_to_vced(dx7_bank[3], v);
    single[0] = 0x43; single[1] = 5; single[2] = 0; single[3] = 1; single[4] = 0x1B;
    memcpy(single + 5, v, DXS_VCED); single[DXS_SINGLE_FRAME - 1u] = dxs_checksum(v, DXS_VCED);
    printf(",\n\"singleMessage\":[%u,", 0xF0u);
    for (i = 0; i < DXS_SINGLE_FRAME; i++) printf("%u,", single[i]);
    printf("%u],\n\"singleVoice\":", 0xF7u); arr(dx7_bank[3], DXS_VOICE);
    /* the voices: packed -> VCED, VCED -> packed (random fields), and dx_patch_valid after the unused bits are masked */
    printf(",\n\"voices\":[\n");
    for (n = 0; n < 60u; n++) {
        uint8_t src[DXS_VOICE], vc[DXS_VCED], back[DXS_VOICE], pk[DXS_VOICE];
        if (n < DX7_BANK_N) memcpy(src, dx7_bank[n], DXS_VOICE);
        else if (n == DX7_BANK_N) memcpy(src, init, DXS_VOICE);
        else for (i = 0; i < DXS_VOICE; i++) { uint32_t r = rnd() & 127u; src[i] = (uint8_t)((n & 1u) ? r % 100u : r); }
        dxs_packed_to_vced(src, vc);
        for (i = 0; i < DXS_VCED; i++) v[i] = (uint8_t)(rnd() & 127u);
        dxs_vced_to_packed(v, back);
        memcpy(pk, src, DXS_VOICE); mask(pk);
        if (!first) printf(",\n");
        first = 0;
        printf("{\"packed\":"); arr(src, DXS_VOICE);
        printf(",\"vced\":"); arr(vc, DXS_VCED);
        printf(",\"randomVced\":"); arr(v, DXS_VCED);
        printf(",\"randomPacked\":"); arr(back, DXS_VOICE);
        printf(",\"valid\":%u}", dx_patch_valid(pk));
    }
    printf("\n]\n}\n");
    return 0;
}

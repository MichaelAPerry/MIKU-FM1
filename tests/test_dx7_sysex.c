/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <string.h>
#include "firmware/song/dx7_sysex.h"
#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"

static int fails;
static DxSynth syn;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static uint8_t bank[DXS_BANK_BYTES], out[DXS_BANK_BYTES], frame[DXS_BULK_FRAME];

static void make_bank(void) {
    uint32_t i;
    for (i = 0; i < DXS_BANK_N; i++) {
        if (i < DX7_BANK_N) memcpy(bank + i * 128u, dx7_bank[i], 128);
        else { dxs_init_voice(bank + i * 128u); bank[i * 128u + 118u + 9u] = (uint8_t)('A' + i - DX7_BANK_N); }
    }
}
static void fix_sum(uint8_t *f) { f[DXS_BULK_FRAME - 1u] = dxs_checksum(f + 5, DXS_BANK_BYTES); }

int main(void) {
    uint32_t i, k, r;
    DxsInfo info;
    uint8_t vced[DXS_VCED], back[128], sf[DXS_SINGLE_FRAME];

    /* the init voice and every factory voice are valid and survive packed -> VCED -> packed */
    dxs_init_voice(back);
    CHECK(dx_patch_valid(back));
    for (i = 0; i < DX7_BANK_N + 1u; i++) {
        const uint8_t *src = i < DX7_BANK_N ? dx7_bank[i] : back;
        uint8_t again[128];
        dxs_packed_to_vced(src, vced);
        for (k = 0; k < DXS_VCED; k++) CHECK(vced[k] < 128);
        dxs_vced_to_packed(vced, again);
        CHECK(memcmp(src, again, 128) == 0);
    }
    /* spot-check VCED positions against the factory voice 0: OP6 first, OP1 last */
    dxs_packed_to_vced(dx7_bank[0], vced);
    CHECK(vced[0] == dx7_bank[0][0] && vced[16] == dx7_bank[0][14]);            /* OP6 R1, OP6 output level */
    CHECK(vced[21 * 5 + 16] == dx7_bank[0][85 + 14]);                           /* OP1 output level */
    CHECK(vced[134] == dx7_bank[0][110] && vced[144] == dx7_bank[0][117]);
    CHECK(memcmp(vced + 145, dx7_bank[0] + 118, 10) == 0);
    CHECK(vced[13] == (dx7_bank[0][12] & 7) && vced[20] == (dx7_bank[0][12] >> 3));   /* RS, detune */
    CHECK(vced[18] == (dx7_bank[0][15] >> 1) && vced[17] == (dx7_bank[0][15] & 1));    /* coarse, mode */

    /* bulk dump, every channel nibble */
    make_bank();
    for (k = 0; k < 16; k++) {
        CHECK(dxs_build_bulk(bank, (uint8_t)k, frame) == DXS_BULK_FRAME);
        memset(out, 0xEE, sizeof out);
        r = dxs_parse(frame, DXS_BULK_FRAME, out, &info);
        CHECK(r == DXS_OK && info.kind == DXS_BULK && info.channel == k && memcmp(out, bank, sizeof bank) == 0);
    }
    CHECK(frame[0] == 0x43 && frame[2] == 9 && frame[3] == 0x20 && frame[4] == 0);

    /* checksum failure leaves `out` alone */
    dxs_build_bulk(bank, 0, frame);
    frame[DXS_BULK_FRAME - 1u] ^= 1u;
    memset(out, 0xEE, sizeof out);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_CHECKSUM && out[0] == 0xEE && info.kind == DXS_BULK);
    dxs_build_bulk(bank, 0, frame);
    frame[100] ^= 2u;                                                            /* data damaged, checksum stale */
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_CHECKSUM);

    /* truncated and over-long frames */
    dxs_build_bulk(bank, 0, frame);
    CHECK(dxs_parse(frame, 0, out, &info) == DXS_E_SHORT);
    CHECK(dxs_parse(frame, 4, out, &info) == DXS_E_SHORT);
    CHECK(dxs_parse(frame, 5, out, &info) == DXS_E_LENGTH);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME - 1u, out, &info) == DXS_E_LENGTH);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME + 1u, out, &info) == DXS_E_LENGTH);
    CHECK(dxs_parse(NULL, 100, out, &info) == DXS_E_SHORT);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, NULL) == DXS_OK);               /* info is optional */

    /* wrong manufacturer / format bytes */
    frame[0] = 0x41;
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_NOT_DX7);
    frame[0] = 0x43; frame[2] = 8;
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_FORMAT && info.kind == DXS_NONE);
    frame[2] = 9; frame[1] = 0x10;                                               /* a request / parameter change, not a dump */
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_FORMAT);
    frame[1] = 0; frame[3] = 0x1F;
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_FORMAT);
    frame[3] = 0x20;

    /* one invalid voice rejects the whole bank and says which */
    for (k = 0; k < DXS_BANK_N; k += 7) {
        dxs_build_bulk(bank, 3, frame);
        frame[5u + k * 128u + 14u] = 100;                                        /* OP6 output level 100 > 99 */
        fix_sum(frame);
        memset(out, 0xEE, sizeof out);
        CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_VOICE && info.bad == k + 1u && out[0] == 0xEE);
    }
    dxs_build_bulk(bank, 3, frame);
    frame[5u + 31u * 128u + 110u] = 32;                                          /* algorithm 33 */
    fix_sum(frame);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_VOICE && info.bad == 32);
    dxs_build_bulk(bank, 3, frame);
    frame[5u + 116u] = (uint8_t)(6u << 1);                                       /* LFO wave 6 */
    fix_sum(frame);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_VOICE && info.bad == 1);
    dxs_build_bulk(bank, 3, frame);
    frame[5u + 2u * 128u + 12u] = 120;                                           /* detune 15 */
    fix_sum(frame);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_E_VOICE && info.bad == 3);

    /* unused high bits in flag bytes are masked, not fatal */
    dxs_build_bulk(bank, 0, frame);
    frame[5u + 11u] |= 0x70u; frame[5u + 15u] |= 0x40u; frame[5u + 111u] |= 0x70u;
    fix_sum(frame);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_OK && memcmp(out, bank, sizeof bank) == 0);

    /* a byte with bit 7 cannot be SysEx data */
    dxs_build_bulk(bank, 0, frame);
    frame[200] = 0x90;
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) != DXS_OK);

    /* single voice: VCED frame for each factory voice, any channel */
    for (i = 0; i < DX7_BANK_N; i++) {
        dxs_packed_to_vced(dx7_bank[i], vced);
        sf[0] = 0x43; sf[1] = (uint8_t)(i % 16u); sf[2] = 0; sf[3] = 1; sf[4] = 0x1B;
        memcpy(sf + 5, vced, DXS_VCED);
        sf[DXS_SINGLE_FRAME - 1u] = dxs_checksum(vced, DXS_VCED);
        memset(back, 0, sizeof back);
        r = dxs_parse(sf, DXS_SINGLE_FRAME, back, &info);
        CHECK(r == DXS_OK && info.kind == DXS_SINGLE && info.channel == i % 16u && memcmp(back, dx7_bank[i], 128) == 0);
    }
    sf[DXS_SINGLE_FRAME - 1u] ^= 4u;
    memset(back, 0xEE, sizeof back);
    CHECK(dxs_parse(sf, DXS_SINGLE_FRAME, back, &info) == DXS_E_CHECKSUM && back[0] == 0xEE);
    sf[DXS_SINGLE_FRAME - 1u] ^= 4u;
    CHECK(dxs_parse(sf, DXS_SINGLE_FRAME - 1u, back, &info) == DXS_E_LENGTH);
    CHECK(dxs_parse(sf, 20, back, &info) == DXS_E_LENGTH);
    sf[4] = 0x1A;
    CHECK(dxs_parse(sf, DXS_SINGLE_FRAME, back, &info) == DXS_E_FORMAT);
    sf[4] = 0x1B;
    /* an out-of-range VCED field is invalid, not silently truncated */
    sf[5 + 20] = 15;                                                             /* OP6 detune 15 */
    sf[DXS_SINGLE_FRAME - 1u] = dxs_checksum(sf + 5, DXS_VCED);
    CHECK(dxs_parse(sf, DXS_SINGLE_FRAME, back, &info) == DXS_E_VOICE);
    sf[5 + 20] = 7;
    sf[5 + 134] = 40;                                                            /* algorithm */
    sf[DXS_SINGLE_FRAME - 1u] = dxs_checksum(sf + 5, DXS_VCED);
    CHECK(dxs_parse(sf, DXS_SINGLE_FRAME, back, &info) == DXS_E_VOICE);

    /* checksum arithmetic */
    { uint8_t d[3] = {1, 2, 3}; CHECK(dxs_checksum(d, 3) == 122); CHECK(dxs_checksum(d, 0) == 0); }

    /* the loaded bytes drive the real synth: a parsed voice is accepted by dx_patch */
    dxs_build_bulk(bank, 0, frame);
    CHECK(dxs_parse(frame, DXS_BULK_FRAME, out, &info) == DXS_OK);
    dx_init(&syn, 1);
    for (i = 0; i < DXS_BANK_N; i++) CHECK(dx_patch(&syn, out + i * 128u) == 1);
    if (fails) { printf("%d failure(s)\n", fails); return 1; }
    printf("dx7_sysex ok\n");
    return 0;
}

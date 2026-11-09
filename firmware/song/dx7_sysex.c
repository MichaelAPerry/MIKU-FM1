/* SPDX-License-Identifier: GPL-3.0-only */
#include "dx7_sysex.h"
#include "firmware/audio/dx7/dx7.h"

uint8_t dxs_checksum(const uint8_t *p, uint32_t n) {
    uint32_t s = 0, i;
    for (i = 0; i < n; i++) s += p[i];
    return (uint8_t)((0u - s) & 127u);
}
/* VCED (155): 6 operators OP6 first, 21 bytes each: R1-4 L1-4 BP LD RD LC RC RS AMS KVS OL MODE COARSE FINE DET;
 * then PR1-4 PL1-4 ALG FB OKS LFS LFD LPMD LAMD LKS LFW LPMS TRNSP and the 10-char name.
 * VMEM (128): per operator 17 bytes (R1-4 L1-4 BP LD RD LC|RC<<2 RS|DET<<3 AMS|KVS<<2 OL MODE|COARSE<<1 FINE), then PR1-4 PL1-4 ALG
 * FB|OKS<<3 LFS LFD LPMD LAMD LKS|LFW<<1|LPMS<<4 TRNSP name. */
void dxs_vced_to_packed(const uint8_t v[DXS_VCED], uint8_t p[DXS_VOICE]) {
    uint32_t o, i;
    for (o = 0; o < 6u; o++) {
        const uint8_t *s = v + 21u * o;
        uint8_t *d = p + 17u * o;
        for (i = 0; i < 11u; i++) d[i] = s[i];
        d[11] = (uint8_t)((s[11] & 3u) | (s[12] & 3u) << 2);
        d[12] = (uint8_t)((s[13] & 7u) | (s[20] & 15u) << 3);
        d[13] = (uint8_t)((s[14] & 3u) | (s[15] & 7u) << 2);
        d[14] = s[16];
        d[15] = (uint8_t)((s[17] & 1u) | (s[18] & 31u) << 1);
        d[16] = s[19];
    }
    for (i = 0; i < 8u; i++) p[102u + i] = v[126u + i];
    p[110] = v[134];
    p[111] = (uint8_t)((v[135] & 7u) | (v[136] & 1u) << 3);
    for (i = 0; i < 4u; i++) p[112u + i] = v[137u + i];
    p[116] = (uint8_t)((v[141] & 1u) | (v[142] & 7u) << 1 | (v[143] & 7u) << 4);
    p[117] = v[144];
    for (i = 0; i < 10u; i++) p[118u + i] = v[145u + i];
}
void dxs_packed_to_vced(const uint8_t p[DXS_VOICE], uint8_t v[DXS_VCED]) {
    uint32_t o, i;
    for (o = 0; o < 6u; o++) {
        const uint8_t *s = p + 17u * o;
        uint8_t *d = v + 21u * o;
        for (i = 0; i < 11u; i++) d[i] = s[i];
        d[11] = s[11] & 3u; d[12] = (uint8_t)(s[11] >> 2 & 3u);
        d[13] = s[12] & 7u; d[20] = (uint8_t)(s[12] >> 3 & 15u);
        d[14] = s[13] & 3u; d[15] = (uint8_t)(s[13] >> 2 & 7u);
        d[16] = s[14];
        d[17] = s[15] & 1u; d[18] = (uint8_t)(s[15] >> 1 & 31u);
        d[19] = s[16];
    }
    for (i = 0; i < 8u; i++) v[126u + i] = p[102u + i];
    v[134] = p[110];
    v[135] = p[111] & 7u; v[136] = (uint8_t)(p[111] >> 3 & 1u);
    for (i = 0; i < 4u; i++) v[137u + i] = p[112u + i];
    v[141] = p[116] & 1u; v[142] = (uint8_t)(p[116] >> 1 & 7u); v[143] = (uint8_t)(p[116] >> 4 & 7u);
    v[144] = p[117];
    for (i = 0; i < 10u; i++) v[145u + i] = p[118u + i];
}
static void dxs_mask(uint8_t *p) {   /* drop bits the DX7 does not use (junk in third-party cartridges) */
    uint32_t o;
    for (o = 0; o < 6u; o++) {
        uint8_t *q = p + 17u * o;
        q[11] &= 15u; q[13] &= 31u; q[15] &= 63u;
    }
    p[111] &= 15u;
}
void dxs_init_voice(uint8_t p[DXS_VOICE]) {
    static const char name[10] = {'I','N','I','T',' ','V','O','I','C','E'};
    uint32_t o, i;
    for (i = 0; i < DXS_VOICE; i++) p[i] = 0;
    for (o = 0; o < 6u; o++) {
        uint8_t *q = p + 17u * o;
        q[0] = q[1] = q[2] = q[3] = 99; q[4] = q[5] = q[6] = 99; q[7] = 0;
        q[8] = 39; q[12] = 7u << 3; q[14] = o == 5u ? 99 : 0; q[15] = 1u << 1;
    }
    for (i = 0; i < 4u; i++) { p[102u + i] = 99; p[106u + i] = 50; }
    p[111] = 8u; p[112] = 35; p[116] = 1u | 3u << 4; p[117] = 24;
    for (i = 0; i < 10u; i++) p[118u + i] = (uint8_t)name[i];
}
uint32_t dxs_parse(const uint8_t *f, uint32_t n, uint8_t *out, DxsInfo *info) {
    DxsInfo z = {0, 0, 0};
    uint32_t i, k;
    uint8_t t[DXS_VOICE], v[DXS_VCED];
    if (!info) info = &z;
    info->kind = DXS_NONE; info->channel = 0; info->bad = 0;
    if (!f || !out || n < 5u) return DXS_E_SHORT;
    if (f[0] != 0x43u) return DXS_E_NOT_DX7;
    if ((f[1] & 0x70u) != 0u) return DXS_E_FORMAT;       /* 0x0n: a dump (0x1n would be a parameter change / request) */
    info->channel = (uint8_t)(f[1] & 15u);
    if (f[2] == 9u && f[3] == 0x20u && f[4] == 0u) {
        info->kind = DXS_BULK;
        if (n != DXS_BULK_FRAME) return DXS_E_LENGTH;
        for (i = 5u; i < DXS_BULK_FRAME; i++) if (f[i] > 127u) return DXS_E_CHECKSUM;
        if (dxs_checksum(f + 5, DXS_BANK_BYTES) != f[DXS_BULK_FRAME - 1u]) return DXS_E_CHECKSUM;
        for (k = 0; k < DXS_BANK_N; k++) {
            for (i = 0; i < DXS_VOICE; i++) t[i] = f[5u + k * DXS_VOICE + i];
            dxs_mask(t);
            if (!dx_patch_valid(t)) { info->bad = (uint8_t)(k + 1u); return DXS_E_VOICE; }
        }
        for (i = 0; i < DXS_BANK_BYTES; i++) out[i] = f[5u + i];
        for (k = 0; k < DXS_BANK_N; k++) dxs_mask(out + k * DXS_VOICE);
        return DXS_OK;
    }
    if (f[2] == 0u && f[3] == 1u && f[4] == 0x1Bu) {
        info->kind = DXS_SINGLE;
        if (n != DXS_SINGLE_FRAME) return DXS_E_LENGTH;
        for (i = 5u; i < DXS_SINGLE_FRAME; i++) if (f[i] > 127u) return DXS_E_CHECKSUM;
        if (dxs_checksum(f + 5, DXS_VCED) != f[DXS_SINGLE_FRAME - 1u]) return DXS_E_CHECKSUM;
        for (i = 0; i < DXS_VCED; i++) v[i] = f[5u + i];
        /* a VCED field wider than its packed bits would corrupt its neighbour: reject before packing */
        info->bad = 1;
        for (k = 0; k < 6u; k++) { const uint8_t *s = v + 21u * k;
            if (s[11] > 3 || s[12] > 3 || s[13] > 7 || s[14] > 3 || s[15] > 7 || s[17] > 1 || s[18] > 31 || s[20] > 14) return DXS_E_VOICE; }
        if (v[135] > 7 || v[136] > 1 || v[141] > 1 || v[142] > 5 || v[143] > 7) return DXS_E_VOICE;
        dxs_vced_to_packed(v, t);
        if (!dx_patch_valid(t)) return DXS_E_VOICE;
        info->bad = 0;
        for (i = 0; i < DXS_VOICE; i++) out[i] = t[i];
        return DXS_OK;
    }
    return DXS_E_FORMAT;
}
uint32_t dxs_build_bulk(const uint8_t bank[DXS_BANK_BYTES], uint8_t ch, uint8_t f[DXS_BULK_FRAME]) {
    uint32_t i;
    f[0] = 0x43u; f[1] = (uint8_t)(ch & 15u); f[2] = 9u; f[3] = 0x20u; f[4] = 0u;
    for (i = 0; i < DXS_BANK_BYTES; i++) f[5u + i] = bank[i] & 127u;
    f[DXS_BULK_FRAME - 1u] = dxs_checksum(f + 5, DXS_BANK_BYTES);
    return DXS_BULK_FRAME;
}

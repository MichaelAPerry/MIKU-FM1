/* SPDX-License-Identifier: GPL-3.0-only */
/* DX7 SysEx import/export: the 32-voice bulk dump (VMEM, format 9) and the single voice (VCED, format 0) to and from the
 * 128-byte packed voice the firmware uses everywhere (dx7.h). Pure functions on caller buffers: no heap, no libm, host-testable.
 * A "frame" is the SysEx body as Dinghy's USB layer delivers it: the bytes between F0 and F7. */
#ifndef MIKU_DX7_SYSEX_H
#define MIKU_DX7_SYSEX_H
#include <stdint.h>
#define DXS_VOICE 128u                 /* packed voice (VMEM) */
#define DXS_VCED 155u                  /* unpacked single voice (VCED) */
#define DXS_BANK_N 32u
#define DXS_BANK_BYTES (DXS_BANK_N * DXS_VOICE)       /* 4096 */
#define DXS_BULK_FRAME (5u + DXS_BANK_BYTES + 1u)     /* 43 0n 09 20 00 + data + checksum = 4102 */
#define DXS_SINGLE_FRAME (5u + DXS_VCED + 1u)         /* 43 0n 00 01 1B + data + checksum = 161 */
enum { DXS_OK = 0, DXS_E_SHORT, DXS_E_NOT_DX7, DXS_E_FORMAT, DXS_E_LENGTH, DXS_E_CHECKSUM, DXS_E_VOICE };
enum { DXS_NONE = 0, DXS_BULK = 1, DXS_SINGLE = 2 };
typedef struct {
    uint8_t kind;      /* DXS_BULK / DXS_SINGLE once the header was recognised (also on a later error) */
    uint8_t channel;   /* the 0..15 in the status nibble (any value is accepted) */
    uint8_t bad;       /* DXS_E_VOICE: the 1-based voice (bulk) that failed dx_patch_valid; 1 for a single voice */
} DxsInfo;
/* Parse one frame. DXS_OK: `out` holds 32 packed voices (DXS_BANK_BYTES, bulk) or one (DXS_VOICE, single). On any error `out`
 * is not modified. A bulk dump with any invalid voice is rejected whole (DXS_E_VOICE, info->bad = which). Bits of a voice that the
 * DX7 leaves unused (e.g. high bits of a flag byte) are masked off before validation, as a few cartridges carry junk there. */
uint32_t dxs_parse(const uint8_t *frame, uint32_t n, uint8_t *out, DxsInfo *info);
uint8_t dxs_checksum(const uint8_t *p, uint32_t n);                 /* 7-bit two's complement of the byte sum */
void dxs_vced_to_packed(const uint8_t vced[DXS_VCED], uint8_t packed[DXS_VOICE]);   /* VCED fields are not range-checked here */
void dxs_packed_to_vced(const uint8_t packed[DXS_VOICE], uint8_t vced[DXS_VCED]);
void dxs_init_voice(uint8_t packed[DXS_VOICE]);                     /* the standard DX7 "INIT VOICE" */
uint32_t dxs_build_bulk(const uint8_t bank[DXS_BANK_BYTES], uint8_t channel, uint8_t frame[DXS_BULK_FRAME]);   /* export: the frame (no F0/F7) */
#endif

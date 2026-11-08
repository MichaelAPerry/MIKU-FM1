/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_SPRITE_CODEC_H
#define MIKU_SPRITE_CODEC_H
#include <stdint.h>
/* Borrowed immutable arrays: caller supplies row_ids[frames*height],
   offsets[row_count+1], data[data_size]. Dimensions are bounded to255.
   Every row is raw4bpp, nibble RLE, or ONE delta against a direct base row.
   offsets are byte offsets into data; row IDs deduplicate exact rows. */
typedef struct {
    uint16_t width,height,frames,row_count,data_size;
    const uint16_t *row_ids,*offsets;
    const uint8_t *data;
} SprAtlas;
/* On error output stays untouched. A valid row writes exactly width palette
   indices (0 transparent). No allocations/workspace/recursion; caller row can
   be reused per LCD strip or dirty rectangle. */
uint32_t spr_decode_row(const SprAtlas *a,uint32_t frame,uint32_t y,uint8_t *out,uint32_t capacity);
/* Random pixel access avoids a row buffer but scans RLE; prefer one row decode
   when rendering a full row. out remains untouched on error. */
uint32_t spr_pixel(const SprAtlas *a,uint32_t frame,uint32_t x,uint32_t y,uint8_t *out);
/* Full integrity/bounds pass for generated data or a newly registered atlas.
   This checks all referenced and unreferenced records, including delta bases. */
uint32_t spr_validate(const SprAtlas *a);
#endif

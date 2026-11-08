/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_MJPEG_H
#define MIKU_MJPEG_H
#include <stdint.h>
#define MJ_MAX_W 160u
#define MJ_MAX_H 120u
#define MJ_FRAME_CAP 8192u
typedef uint16_t (*MjPixel)(void *context,uint32_t x,uint32_t y);
typedef struct {
    uint8_t *out;
    uint32_t cap,pos,bitbuf,nbits;
    int32_t dc[3];
    uint8_t overflow;
    uint8_t qt[2][64];
    uint16_t recip[2][64];
    int32_t y[4][64],cb[64],cr[64],q[64];
    uint32_t w,h,mx,my;
    MjPixel pixel;
    void *context;
    uint8_t state; /* 0 invalid,1 encoding,2 complete,3 overflow */
} MjEncoder;
/* Source context and output remain owned exclusively by encoder until complete.
 * Snapshot animation state before begin. Pixel callback returns native RGB565.
 * No framebuffer,allocation,global workspace,float,USB,or display operations. */
int mj_begin(MjEncoder *j,uint32_t w,uint32_t h,uint32_t quality,
             uint8_t *out,uint32_t cap,MjPixel pixel,void *context);
/* At most max_mcus 16x16 tiles. Return0 pending,1 complete,-1 failed.
 * Call from main loop or dedicated video core,never audio/USB ISR. */
int mj_step(MjEncoder *j,uint32_t max_mcus);
uint32_t mj_size(const MjEncoder *j); /* 0 until successfully complete */
#endif

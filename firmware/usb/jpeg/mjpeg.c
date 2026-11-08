/* SPDX-License-Identifier: GPL-3.0-only */
/* Adapted from Synthesthetic vjpeg.c, revision589bd0f30b68b66f78dab84debb42909131f4cad.
 * Copyright (C)2026 Synthesthetic contributors. See NOTICE.md.
 * MIKU changes: callback pixels,caller-owned scratch,incremental MCU budget,validation. */
#include "mjpeg.h"
#include "vjpeg_tables.h"
typedef MjEncoder vjpeg_t;
#define VJ_CONST_BITS 13
#define VJ_PASS1_BITS 2
#define VJ_F_0_298 2446   /* FIX(0.298631336) */
#define VJ_F_0_390 3196   /* FIX(0.390180644) */
#define VJ_F_0_541 4433   /* FIX(0.541196100) */
#define VJ_F_0_765 6270   /* FIX(0.765366865) */
#define VJ_F_0_899 7373   /* FIX(0.899976223) */
#define VJ_F_1_175 9633   /* FIX(1.175875602) */
#define VJ_F_1_501 12299  /* FIX(1.501321110) */
#define VJ_F_1_847 15137  /* FIX(1.847759065) */
#define VJ_F_1_961 16069  /* FIX(1.961570560) */
#define VJ_F_2_053 16819  /* FIX(2.053119869) */
#define VJ_F_2_562 20995  /* FIX(2.562915447) */
#define VJ_F_3_072 25172  /* FIX(3.072711026) */
#define VJ_DESCALE(x, n) (((x) + (1 << ((n) - 1))) >> (n))

static void vjpeg_ycc_565(uint32_t c, int32_t *y, int32_t *cb, int32_t *cr)
{
    int32_t r = ((int32_t)((c >> 11) & 31u) * 527 + 23) >> 6, g = ((int32_t)((c >> 5) & 63u) * 259 + 33) >> 6,
            b = ((int32_t)(c & 31u) * 527 + 23) >> 6;
    *y = (77 * r + 150 * g + 29 * b + 128) >> 8;
    *cb = 128 + ((-43 * r - 85 * g + 128 * b + 128) >> 8);
    *cr = 128 + ((128 * r - 107 * g - 21 * b + 128) >> 8);
}

/* ---- the output ---- */
#define VJ_INLINE static inline __attribute__((always_inline))   /* (the firmware builds -Os: these are the hot path,
                                                         * two a coefficient; a call each cost most of the encode) */
VJ_INLINE void vj_byte(vjpeg_t *j, uint32_t b)
{
    if (j->pos < j->cap)
        j->out[j->pos] = (uint8_t)b;
    else
        j->overflow = 1;
    j->pos++;
}
static void vj_u16(vjpeg_t *j, uint32_t v) { vj_byte(j, v >> 8); vj_byte(j, v & 0xFFu); }

VJ_INLINE void vj_bits(vjpeg_t *j, uint32_t code, uint32_t len)   /* len 1..16 */
{
    uint32_t nb = j->nbits + len, buf = (j->bitbuf << len) | (code & ((1u << len) - 1u));
    while (nb >= 8u) {
        uint32_t b;
        nb -= 8u;
        b = (buf >> nb) & 0xFFu;
        if (j->pos + 2u <= j->cap) {                    /* room for the byte and its stuffing: no checks */
            j->out[j->pos++] = (uint8_t)b;
            if (b == 0xFFu)
                j->out[j->pos++] = 0;                   /* byte stuffing */
        } else {
            vj_byte(j, b);
            if (b == 0xFFu)
                vj_byte(j, 0);
        }
    }
    j->bitbuf = buf & ((1u << nb) - 1u);
    j->nbits = nb;
}
static void vj_flush(vjpeg_t *j)
{
    if (j->nbits)
        vj_bits(j, 0x7Fu, 8u - j->nbits);                   /* pad with ones */
}

/* ---- the DCT (IJG jfdctint), in place on 64 int32 in natural order; output = 8 x DCT ---- */
static void vj_fdct(int32_t *d)
{
    int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13, z1, z2, z3, z4, z5;
    int32_t *p;
    uint32_t i;
    for (i = 0, p = d; i < 8u; i++, p += 8) {               /* rows */
        tmp0 = p[0] + p[7]; tmp7 = p[0] - p[7];
        tmp1 = p[1] + p[6]; tmp6 = p[1] - p[6];
        tmp2 = p[2] + p[5]; tmp5 = p[2] - p[5];
        tmp3 = p[3] + p[4]; tmp4 = p[3] - p[4];
        tmp10 = tmp0 + tmp3; tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2; tmp12 = tmp1 - tmp2;
        p[0] = (tmp10 + tmp11) * (1 << VJ_PASS1_BITS);      /* (a multiply: a left shift of a negative value is UB) */
        p[4] = (tmp10 - tmp11) * (1 << VJ_PASS1_BITS);
        z1 = (tmp12 + tmp13) * VJ_F_0_541;
        p[2] = VJ_DESCALE(z1 + tmp13 * VJ_F_0_765, VJ_CONST_BITS - VJ_PASS1_BITS);
        p[6] = VJ_DESCALE(z1 - tmp12 * VJ_F_1_847, VJ_CONST_BITS - VJ_PASS1_BITS);
        z1 = tmp4 + tmp7; z2 = tmp5 + tmp6; z3 = tmp4 + tmp6; z4 = tmp5 + tmp7;
        z5 = (z3 + z4) * VJ_F_1_175;
        tmp4 *= VJ_F_0_298; tmp5 *= VJ_F_2_053; tmp6 *= VJ_F_3_072; tmp7 *= VJ_F_1_501;
        z1 *= -VJ_F_0_899; z2 *= -VJ_F_2_562; z3 *= -VJ_F_1_961; z4 *= -VJ_F_0_390;
        z3 += z5; z4 += z5;
        p[7] = VJ_DESCALE(tmp4 + z1 + z3, VJ_CONST_BITS - VJ_PASS1_BITS);
        p[5] = VJ_DESCALE(tmp5 + z2 + z4, VJ_CONST_BITS - VJ_PASS1_BITS);
        p[3] = VJ_DESCALE(tmp6 + z2 + z3, VJ_CONST_BITS - VJ_PASS1_BITS);
        p[1] = VJ_DESCALE(tmp7 + z1 + z4, VJ_CONST_BITS - VJ_PASS1_BITS);
    }
    for (i = 0, p = d; i < 8u; i++, p++) {                  /* columns */
        tmp0 = p[0] + p[56]; tmp7 = p[0] - p[56];
        tmp1 = p[8] + p[48]; tmp6 = p[8] - p[48];
        tmp2 = p[16] + p[40]; tmp5 = p[16] - p[40];
        tmp3 = p[24] + p[32]; tmp4 = p[24] - p[32];
        tmp10 = tmp0 + tmp3; tmp13 = tmp0 - tmp3;
        tmp11 = tmp1 + tmp2; tmp12 = tmp1 - tmp2;
        p[0] = VJ_DESCALE(tmp10 + tmp11, VJ_PASS1_BITS);
        p[32] = VJ_DESCALE(tmp10 - tmp11, VJ_PASS1_BITS);
        z1 = (tmp12 + tmp13) * VJ_F_0_541;
        p[16] = VJ_DESCALE(z1 + tmp13 * VJ_F_0_765, VJ_CONST_BITS + VJ_PASS1_BITS);
        p[48] = VJ_DESCALE(z1 - tmp12 * VJ_F_1_847, VJ_CONST_BITS + VJ_PASS1_BITS);
        z1 = tmp4 + tmp7; z2 = tmp5 + tmp6; z3 = tmp4 + tmp6; z4 = tmp5 + tmp7;
        z5 = (z3 + z4) * VJ_F_1_175;
        tmp4 *= VJ_F_0_298; tmp5 *= VJ_F_2_053; tmp6 *= VJ_F_3_072; tmp7 *= VJ_F_1_501;
        z1 *= -VJ_F_0_899; z2 *= -VJ_F_2_562; z3 *= -VJ_F_1_961; z4 *= -VJ_F_0_390;
        z3 += z5; z4 += z5;
        p[56] = VJ_DESCALE(tmp4 + z1 + z3, VJ_CONST_BITS + VJ_PASS1_BITS);
        p[40] = VJ_DESCALE(tmp5 + z2 + z4, VJ_CONST_BITS + VJ_PASS1_BITS);
        p[24] = VJ_DESCALE(tmp6 + z2 + z3, VJ_CONST_BITS + VJ_PASS1_BITS);
        p[8] = VJ_DESCALE(tmp7 + z1 + z4, VJ_CONST_BITS + VJ_PASS1_BITS);
    }
}

/* the DCT of a block whose rows and columns come in equal pairs (a source pixel 2 x 2 in the output, zoom 2):
 * the same sums as vj_fdct, bit for bit, with the equal halves folded into four distinct inputs a b c e per
 * line: four row transforms (the odd rows copies) and eight column transforms, about a quarter of the work,
 * and the fourth coefficient of every line is zero */
#define VJ_F2_A 15136     /* 2 F_0_541 + F_0_765 */
#define VJ_F2_B 6271      /* F_1_847 - 2 F_0_541 */
#define VJ_F2_S7 4176     /* F_0_899 + F_1_961 - 2 F_1_175 */
#define VJ_F2_S5 4925     /* F_2_562 + F_0_390 - 2 F_1_175 */
#define VJ_F2_S3 17798    /* F_2_562 + F_1_961 - 2 F_1_175 */
#define VJ_F2_S1 8697     /* 2 F_1_175 - F_0_899 - F_0_390 */
static const uint8_t VJ_NBITS[256] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
VJ_INLINE uint32_t vj_cat(uint32_t a)                       /* bit length of a (a >= 1) */
{
    return a < 256u ? VJ_NBITS[a] : 8u + VJ_NBITS[(a >> 8) & 255u];
}



static void vj_dc(vjpeg_t *j, int32_t q0, uint32_t comp, const uint32_t *dc_tab)   /* the DC: the difference to the
                                                         * previous block of this component */
{
    int32_t diff = q0 - j->dc[comp];
    uint32_t t;
    j->dc[comp] = q0;
    if (diff == 0) {
        t = dc_tab[0];
        vj_bits(j, t & 0xFFFFu, t >> 16);
    } else {
        uint32_t a = (uint32_t)(diff < 0 ? -diff : diff), n = vj_cat(a);
        t = dc_tab[n];
        vj_bits(j, t & 0xFFFFu, t >> 16);
        vj_bits(j, diff < 0 ? (uint32_t)(diff - 1) : (uint32_t)diff, n);
    }
}

/* a flat block, its DCT dc (64 x the level-shifted value; nothing else): the DC quantised as vj_block does, then
 * the end of block: the same bits as the whole path, without its DCT or the 63 zeros */
static void vj_flat(vjpeg_t *j, int32_t dc, uint32_t comp, const uint32_t *dc_tab, const uint32_t *ac_tab)
{
    uint32_t a = (uint32_t)(dc < 0 ? -dc : dc), t;
    a = (a * j->recip[comp ? 1 : 0][0] + 0x8000u) >> 16;
    vj_dc(j, dc < 0 ? -(int32_t)a : (int32_t)a, comp, dc_tab);
    t = ac_tab[0x00];
    vj_bits(j, t & 0xFFFFu, t >> 16);
}

static void vj_block(vjpeg_t *j, int32_t *d, uint32_t comp, const uint32_t *dc_tab, const uint32_t *ac_tab)
{
    const uint16_t *rc = j->recip[comp ? 1 : 0];
    int32_t *q = j->q;
    uint32_t i, run, t;
    {
        int32_t v = d[0];
        for (i = 1; i < 64u && d[i] == v; i++)
            ;
        if (i == 64u) {                                     /* flat (black around a placed frame, the interface's
                                                             * ground): its DCT is 64 x the value at DC */
            vj_flat(j, v * 64, comp, dc_tab, ac_tab);
            return;
        }
        vj_fdct(d);
    }
    for (i = 0; i < 64u; i++) {                             /* quantise: round(d / (8 q)), into zigzag order */
        int32_t v = d[i], neg = v < 0;
        uint32_t a = (uint32_t)(neg ? -v : v);
        a = (a * rc[i] + 0x8000u) >> 16;
        q[VJ_ZIGZAG[i]] = neg ? -(int32_t)a : (int32_t)a;
    }
    vj_dc(j, q[0], comp, dc_tab);
    for (i = 1, run = 0; i < 64u; i++) {                    /* AC: run lengths, ZRL, EOB */
        int32_t v = q[i];
        if (v == 0) {
            run++;
            continue;
        }
        while (run > 15u) {
            t = ac_tab[0xF0];
            vj_bits(j, t & 0xFFFFu, t >> 16);
            run -= 16u;
        }
        {
            uint32_t a = (uint32_t)(v < 0 ? -v : v), n = vj_cat(a);
            t = ac_tab[(run << 4) | n];
            vj_bits(j, t & 0xFFFFu, t >> 16);
            vj_bits(j, v < 0 ? (uint32_t)(v - 1) : (uint32_t)v, n);
        }
        run = 0;
    }
    if (run) {
        t = ac_tab[0x00];
        vj_bits(j, t & 0xFFFFu, t >> 16);
    }
}

/* ---- the headers ---- */
static void vj_quant_tables(vjpeg_t *j, uint32_t quality)
{
    uint32_t s = quality < 1u ? 1u : quality > 100u ? 100u : quality, t, i;
    s = s < 50u ? 5000u / s : 200u - 2u * s;
    for (t = 0; t < 2u; t++) {
        const uint8_t *base = t ? VJ_CQT : VJ_YQT;
        for (i = 0; i < 64u; i++) {
            uint32_t q = (base[i] * s + 50u) / 100u;
            q = q < 1u ? 1u : q > 255u ? 255u : q;
            j->qt[t][VJ_ZIGZAG[i]] = (uint8_t)q;
            j->recip[t][i] = (uint16_t)((65536u + 4u * q) / (8u * q));
        }
    }
}

static void vj_dht(vjpeg_t *j, uint32_t id, const uint8_t *nr, const uint8_t *val, uint32_t nval)
{
    uint32_t i;
    vj_u16(j, 0xFFC4u);
    vj_u16(j, 2u + 1u + 16u + nval);
    vj_byte(j, id);
    for (i = 0; i < 16u; i++)
        vj_byte(j, nr[i]);
    for (i = 0; i < nval; i++)
        vj_byte(j, val[i]);
}

static void vj_headers(vjpeg_t *j, uint32_t w, uint32_t h)
{
    uint32_t t, i;
    vj_u16(j, 0xFFD8u);                                     /* SOI */
    vj_u16(j, 0xFFE0u);                                     /* APP0 JFIF 1.01, no density, no thumbnail */
    vj_u16(j, 16u);
    vj_byte(j, 'J'); vj_byte(j, 'F'); vj_byte(j, 'I'); vj_byte(j, 'F'); vj_byte(j, 0);
    vj_byte(j, 1); vj_byte(j, 1); vj_byte(j, 0); vj_u16(j, 1); vj_u16(j, 1); vj_byte(j, 0); vj_byte(j, 0);
    for (t = 0; t < 2u; t++) {                              /* DQT, 8-bit, zigzag order */
        vj_u16(j, 0xFFDBu);
        vj_u16(j, 67u);
        vj_byte(j, t);
        for (i = 0; i < 64u; i++)
            vj_byte(j, j->qt[t][i]);
    }
    vj_u16(j, 0xFFC0u);                                     /* SOF0: 8 bit, 3 components, Y 2x2 (4:2:0) */
    vj_u16(j, 17u);
    vj_byte(j, 8);
    vj_u16(j, h);
    vj_u16(j, w);
    vj_byte(j, 3);
    vj_byte(j, 1); vj_byte(j, 0x22); vj_byte(j, 0);
    vj_byte(j, 2); vj_byte(j, 0x11); vj_byte(j, 1);
    vj_byte(j, 3); vj_byte(j, 0x11); vj_byte(j, 1);
    vj_dht(j, 0x00, VJ_YDC_NR, VJ_YDC_VAL, 12);
    vj_dht(j, 0x10, VJ_YAC_NR, VJ_YAC_VAL, 162);
    vj_dht(j, 0x01, VJ_CDC_NR, VJ_CDC_VAL, 12);
    vj_dht(j, 0x11, VJ_CAC_NR, VJ_CAC_VAL, 162);
    vj_u16(j, 0xFFDAu);                                     /* SOS */
    vj_u16(j, 12u);
    vj_byte(j, 3);
    vj_byte(j, 1); vj_byte(j, 0x00);
    vj_byte(j, 2); vj_byte(j, 0x11);
    vj_byte(j, 3); vj_byte(j, 0x11);
    vj_byte(j, 0); vj_byte(j, 63); vj_byte(j, 0);
}

/* Each call completes whole MCUs so main-loop polling can yield between them. */
int mj_begin(MjEncoder *j,uint32_t w,uint32_t h,uint32_t quality,uint8_t *out,uint32_t cap,MjPixel pixel,void *context){
    if(!j)return 0;
    j->state=0;
    if(!out||!pixel||!w||!h||w>MJ_MAX_W||h>MJ_MAX_H||quality<1u||quality>100u||!cap||cap>MJ_FRAME_CAP)return 0;
    j->out=out;j->cap=cap;j->pos=j->bitbuf=j->nbits=0;j->overflow=0;
    j->dc[0]=j->dc[1]=j->dc[2]=0;j->w=w;j->h=h;j->mx=j->my=0;j->pixel=pixel;j->context=context;
    vj_quant_tables(j,quality);vj_headers(j,w,h);
    j->state=j->overflow?3u:1u;return !j->overflow;
}
uint32_t mj_size(const MjEncoder *j){return j&&j->state==2u?j->pos:0;}
int mj_step(MjEncoder *j,uint32_t max_mcus){
    uint32_t by,bx,k;
    if(!j||j->state==0u||j->state==3u)return -1;
    if(j->state==2u)return 1;
    while(max_mcus--){
        for(by=0;by<16u;by++){
            uint32_t yy=j->my+by;if(yy>=j->h)yy=j->h-1u;
            for(bx=0;bx<16u;bx++){
                uint32_t xx=j->mx+bx;if(xx>=j->w)xx=j->w-1u;
                int32_t y,cb,cr;uint32_t c=(by>>1)*8u+(bx>>1);
                vjpeg_ycc_565(j->pixel(j->context,xx,yy),&y,&cb,&cr);
                j->y[(by>>3)*2u+(bx>>3)][(by&7u)*8u+(bx&7u)]=y-128;
                if(!(by&1u)&&!(bx&1u)){j->cb[c]=cb;j->cr[c]=cr;}
                else{j->cb[c]+=cb;j->cr[c]+=cr;}
            }
        }
        for(k=0;k<64u;k++){j->cb[k]=((j->cb[k]+2)>>2)-128;j->cr[k]=((j->cr[k]+2)>>2)-128;}
        for(k=0;k<4u;k++)vj_block(j,j->y[k],0,VJ_YDC,VJ_YAC);
        vj_block(j,j->cb,1,VJ_CDC,VJ_CAC);vj_block(j,j->cr,2,VJ_CDC,VJ_CAC);
        if(j->overflow){j->state=3;return -1;}
        j->mx+=16u;
        if(j->mx>=j->w){j->mx=0;j->my+=16u;}
        if(j->my>=j->h){
            vj_flush(j);vj_u16(j,0xFFD9u);j->state=j->overflow?3u:2u;
            return j->overflow?-1:1;
        }
    }
    return 0;
}

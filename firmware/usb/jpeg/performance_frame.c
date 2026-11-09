/* SPDX-License-Identifier: GPL-3.0-only */
#include "performance_frame.h"
#include "firmware/ui/singer_sprites.h"
#if MIKU_COMPACT_SPRITES
#include "firmware/ui/wardrobe_sprites.h"
#endif
uint16_t mj_perf_pixel(void *state,uint32_t x,uint32_t y){
    const PerfState *s=(const PerfState*)state;
    const PerfOutfit *o;int32_t sx,sy;uint32_t i;
    if(!s||x<20u||x>=140u||y>=120u)return 0;
    x=(x-20u)*2u;y*=2u;
    o=perf_outfit(s->outfit);
    if(!o||!o->scale||s->sprite>=o->frames)return perf_pixel(s,x,y);
    sx=(int32_t)x-o->x;sy=(int32_t)y-o->y;
    if(sx<0||sy<0||sx>=o->width*o->scale||sy>=o->height*o->scale)return perf_pixel(s,x,y);
#if MIKU_COMPACT_SPRITES
    {uint8_t index=0;
     if(!spr_pixel(WD_OUTFITS[s->outfit].stage,o->first_frame+s->sprite,(uint32_t)(sx/o->scale),(uint32_t)(sy/o->scale),&index))return perf_pixel(s,x,y);
     i=index;}
    return i?WD_OUTFITS[s->outfit].stage_palette[i]:perf_pixel(s,x,y);
#else
    {uint32_t p=(uint32_t)(sy/o->scale)*o->width+(uint32_t)(sx/o->scale),b;
     b=SINGER_STAGE[o->first_frame+s->sprite][p>>1];i=p&1u?b&15u:b>>4;}
    return i?SINGER_STAGE_PAL[i]:perf_pixel(s,x,y);
#endif
}

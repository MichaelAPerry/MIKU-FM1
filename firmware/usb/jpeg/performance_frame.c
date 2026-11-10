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

void mj_perf_cache_init(MjPerfCache *cache,const PerfState *scene){
    cache->scene=scene;cache->decoded_rows=0;stagefx_cache_init(&cache->effects);
    for(uint32_t i=0;i<12u;i++)cache->row_y[i]=255u;
}
uint16_t mj_perf_cached_pixel(void *context,uint32_t x,uint32_t y){
    MjPerfCache *cache=(MjPerfCache *)context;
#if MIKU_COMPACT_SPRITES
    const PerfState *s=cache->scene;
    const PerfOutfit *o;int32_t sx,sy;uint32_t row,slot,index;
    if(!s||x<20u||x>=140u||y>=120u)return 0;
    x=(x-20u)*2u;y*=2u;o=perf_outfit(s->outfit);
    if(!o||!o->scale||s->sprite>=o->frames)return perf_cached_pixel(s,&cache->effects,x,y);
    sx=(int32_t)x-o->x;sy=(int32_t)y-o->y;
    if(sx<0||sy<0||sx>=o->width*o->scale||sy>=o->height*o->scale)return perf_cached_pixel(s,&cache->effects,x,y);
    row=(uint32_t)sy/o->scale;slot=row%12u;
    if(cache->row_y[slot]!=row){
        if(!spr_decode_row(WD_OUTFITS[s->outfit].stage,o->first_frame+s->sprite,row,cache->rows[slot],61u))return perf_cached_pixel(s,&cache->effects,x,y);
        cache->row_y[slot]=(uint8_t)row;cache->decoded_rows++;
    }
    index=cache->rows[slot][(uint32_t)sx/o->scale];
    return index?WD_OUTFITS[s->outfit].stage_palette[index]:perf_cached_pixel(s,&cache->effects,x,y);
#else
    return mj_perf_pixel((void *)cache->scene,x,y);
#endif
}

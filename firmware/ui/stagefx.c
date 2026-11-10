/* SPDX-License-Identifier: GPL-3.0-only */
#include "stagefx.h"
#include "stagefx_sprites.h"
static uint16_t stagefx_sample(StageFxCache *cache,uint32_t mode,uint32_t age,uint32_t intensity,uint32_t x,uint32_t y){
    uint32_t n,frame,index;uint8_t pixel;
    if(cache&&(!cache->valid||cache->mode!=mode||cache->age!=age||cache->intensity!=intensity)){
        cache->valid=1;cache->mode=mode;cache->age=age;cache->intensity=intensity;
        for(n=0;n<3u;n++)cache->y[n]=~0u;
    }
    if(!mode||mode>=STFX_N||!intensity||x>=240u||y>=214u||age>=960u)return 0;
    if(mode==STFX_BEAMS){
        if(y>180u)return 0;
        return ((x+age/24u+y/3u)%80u<2u+intensity/32u)?0x05d7u:0;
    }
    frame=(age/240u)+4u*(mode-1u);
    /* Keep the centre/face clear; density is bounded by three sprite instances. */
    for(n=0;n<1u+intensity/48u&&n<3u;n++){
        uint32_t left=n==0u?8u:n==1u?196u:104u;
        uint32_t top=n==0u?28u:n==1u?62u:174u;
        if(mode==STFX_CONFETTI)top=(top+age/12u)%182u;
        if(mode==STFX_SMOKE)top=178u-age/32u;
        if(x<left||y<top||x>=left+32u||y>=top+32u)continue;
        index=0;pixel=0;
        if(cache){
            if(cache->y[n]!=y-top){
                if(!spr_decode_row(&FX_ATLAS,frame,y-top,cache->rows[n],32u))continue;
                cache->y[n]=y-top;
            }
            index=cache->rows[n][x-left];
        }else if(spr_pixel(&FX_ATLAS,frame,x-left,y-top,&pixel))index=pixel;
        if(index)return FX_PAL[index];
    }
    return 0;
}

void stagefx_cache_init(StageFxCache *cache){cache->valid=0;}
uint16_t stagefx_cached_pixel(StageFxCache *cache,uint32_t mode,uint32_t age,uint32_t intensity,uint32_t x,uint32_t y){
    return stagefx_sample(cache,mode,age,intensity,x,y);
}
uint16_t stagefx_pixel(uint32_t mode,uint32_t age,uint32_t intensity,uint32_t x,uint32_t y){
    return stagefx_sample(0,mode,age,intensity,x,y);
}

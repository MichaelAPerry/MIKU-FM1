/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/sprite_codec.h"
static uint32_t atlas_ok(const SprAtlas *a){
    return a&&a->width&&a->width<=255u&&a->height&&a->height<=255u&&a->frames&&a->frames<=255u&&a->row_count&&a->data_size&&a->row_ids&&a->offsets&&a->data;
}
static uint32_t bounds(const SprAtlas *a,uint32_t id,uint32_t *start,uint32_t *end){
    if(id>=a->row_count)return 0;
    *start=a->offsets[id];*end=a->offsets[id+1u];
    return *start<*end&&*end<=a->data_size;
}
static uint32_t direct_ok(const SprAtlas *a,uint32_t start,uint32_t end,uint32_t width){
    uint32_t n=0;
    if(start>=end)return 0;
    if(a->data[start]==0u){
        if(end-start!=1u+(width+1u)/2u)return 0;
        return !(width&1u)||!(a->data[end-1u]&15u);
    }
    if(a->data[start++]!=1u)return 0;
    while(start<end){n+=(a->data[start++]>>4)+1u;if(n>width)return 0;}
    return n==width;
}
static uint32_t row_ok(const SprAtlas *a,uint32_t id){
    uint32_t start,end,bs,be,base,x,n;
    if(!bounds(a,id,&start,&end))return 0;
    if(a->data[start]!=2u)return direct_ok(a,start,end,a->width);
    if(end-start<7u)return 0;
    base=a->data[start+1u]|((uint32_t)a->data[start+2u]<<8);
    x=a->data[start+3u];n=a->data[start+4u];
    if(!n||x+n>a->width||!bounds(a,base,&bs,&be))return 0;
    /* This also rejects self-references, cycles and multi-level deltas. */
    return direct_ok(a,bs,be,a->width)&&direct_ok(a,start+5u,end,n);
}
static uint8_t direct_pixel(const SprAtlas *a,uint32_t start,uint32_t x){
    if(!a->data[start]){uint32_t b=a->data[start+1u+(x>>1)];return (uint8_t)(x&1u?b&15u:b>>4);}
    start++;
    for(;;){uint32_t b=a->data[start++],n=(b>>4)+1u;if(x<n)return (uint8_t)(b&15u);x-=n;}
}
static uint8_t row_pixel(const SprAtlas *a,uint32_t id,uint32_t x){
    uint32_t start=a->offsets[id];
    if(a->data[start]==2u){
        uint32_t px=a->data[start+3u],n=a->data[start+4u];
        if(x>=px&&x<px+n)return direct_pixel(a,start+5u,x-px);
        id=a->data[start+1u]|((uint32_t)a->data[start+2u]<<8);start=a->offsets[id];
    }
    return direct_pixel(a,start,x);
}
static void direct_write(const SprAtlas *a,uint32_t start,uint32_t width,uint8_t *out){
    if(!a->data[start]){
        for(uint32_t x=0;x<width;x++){uint32_t b=a->data[start+1u+(x>>1)];out[x]=(uint8_t)(x&1u?b&15u:b>>4);}
    }else{
        uint32_t x=0;start++;
        while(x<width){uint32_t b=a->data[start++],n=(b>>4)+1u;while(n--)out[x++]=(uint8_t)(b&15u);}
    }
}
uint32_t spr_decode_row(const SprAtlas *a,uint32_t frame,uint32_t y,uint8_t *out,uint32_t capacity){
    uint32_t id;
    if(!atlas_ok(a)||!out||capacity<a->width||frame>=a->frames||y>=a->height)return 0;
    id=a->row_ids[frame*a->height+y];if(!row_ok(a,id))return 0;
    uint32_t start=a->offsets[id];
    if(a->data[start]==2u){
        uint32_t base=a->data[start+1u]|((uint32_t)a->data[start+2u]<<8);
        direct_write(a,a->offsets[base],a->width,out);
        direct_write(a,start+5u,a->data[start+4u],out+a->data[start+3u]);
    }else direct_write(a,start,a->width,out);
    return 1;
}
uint32_t spr_pixel(const SprAtlas *a,uint32_t frame,uint32_t x,uint32_t y,uint8_t *out){
    uint32_t id;
    if(!atlas_ok(a)||!out||frame>=a->frames||y>=a->height||x>=a->width)return 0;
    id=a->row_ids[frame*a->height+y];if(!row_ok(a,id))return 0;
    *out=row_pixel(a,id,x);return 1;
}
uint32_t spr_validate(const SprAtlas *a){
    if(!atlas_ok(a)||a->offsets[0]!=0u||a->offsets[a->row_count]!=a->data_size)return 0;
    for(uint32_t i=0;i<a->row_count;i++)if(!row_ok(a,i))return 0;
    for(uint32_t i=0;i<(uint32_t)a->frames*a->height;i++)if(a->row_ids[i]>=a->row_count)return 0;
    return 1;
}

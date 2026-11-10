/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/performance.h"
#include "stagefx.h"
#define PRGB(r,g,b) ((uint16_t)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3)))
static const PerfOutfit outfits[PERF_OUTFIT_N]=PERF_OUTFIT_ROWS;
const PerfOutfit *perf_outfit(uint32_t id){return id<PERF_OUTFIT_N?&outfits[id]:0;}
/* Shared geometry, distinct colours/layout variants per outfit: no bitmap bank. */
static const uint16_t style_colours[PERF_OUTFIT_N][3]={
 {0x2ed9,0x1188,0x08a5},{0xfbb6,0x48ad,0x18a7},{0x969f,0x294b,0x10c6},
 {0xfd4a,0x49ac,0x18a6},{0xa7ff,0x29ed,0x08c7},{0xf834,0x680d,0x1805},
 {0x47f0,0x19c9,0x08e5},{0x05ff,0x0230,0x0087},{0xff29,0x2a0d,0x1107},
 {0xb7ff,0x3a31,0x114a},{0xfe72,0x51cd,0x20a8},{0xcbbf,0x4010,0x1007}};
uint32_t perf_style(PerfState *s,uint32_t outfit){
 static const uint8_t settings[PERF_OUTFIT_N][3]={
 {1,1,2},{3,1,2},{2,1,2},{4,2,3},{2,1,5},{4,2,3},
 {3,1,3},{4,2,5},{2,1,2},{2,1,4},{3,1,3},{1,1,1}};
 if(!s||outfit>=PERF_OUTFIT_N)return 0;
 perf_select(s,outfit,settings[outfit][0],settings[outfit][1],0);
 s->effect=settings[outfit][2];s->intensity=80;s->effect_epoch=s->now_ms;return 1;
}
uint32_t perf_scene_encode(const PerfState *s,uint8_t out[3]){
 if(!s||!out||!perf_outfit(s->outfit)||s->background>=PERF_BG_N||s->frame>=PERF_FRAME_N||s->effect>=STFX_N||s->palette>=PERF_PALETTE_N||s->intensity>100)return 0;
 out[0]=(uint8_t)(s->outfit|(((s->intensity*15u+50u)/100u)<<4));
 out[1]=(uint8_t)(s->background|(s->frame<<3)|(s->effect<<5));out[2]=s->palette;return 1;
}
uint32_t perf_scene_decode(PerfState *s,const uint8_t in[3]){
 uint32_t outfit,bg,frame,fx;
 if(!s||!in)return 0;
 outfit=in[0]&15u;bg=in[1]&7u;frame=(in[1]>>3)&3u;fx=in[1]>>5;
 if(!perf_outfit(outfit)||bg>=PERF_BG_N||frame>=PERF_FRAME_N||fx>=STFX_N||in[2]>=PERF_PALETTE_N)return 0;
 perf_select(s,outfit,bg,frame,in[2]);s->intensity=(uint8_t)((in[0]>>4)*100u/15u);s->effect=(uint8_t)fx;s->effect_epoch=s->now_ms;return 1;
}
void perf_init(PerfState *s){
    if(!s)return;
    s->now_ms=s->bow_until=s->beat_q8=s->effect_epoch=0;
    s->effect=0;s->intensity=80;
    s->outfit=s->background=s->frame=s->palette=0;
    s->sprite=s->light=s->section=s->playing=s->initialized=s->bow_active=0;s->mouth=5;
}
uint32_t perf_select(PerfState *s,uint32_t outfit,uint32_t bg,uint32_t frame,uint32_t palette){
    if(!s||!perf_outfit(outfit)||bg>=PERF_BG_N||frame>=PERF_FRAME_N||palette>=PERF_PALETTE_N)return 0;
    s->outfit=(uint8_t)outfit;s->background=(uint8_t)bg;s->frame=(uint8_t)frame;s->palette=(uint8_t)palette;return 1;
}
uint32_t perf_update(PerfState *s,const PerfInput *in){
    uint32_t dirty=0,beat,f,light;uint8_t mouth;
    if(!s||!in)return 0;
    if(s->initialized&&s->playing&&!in->playing){s->bow_until=in->now_ms+1500u;s->bow_active=1;}
    if(s->bow_active&&(int32_t)(in->now_ms-s->bow_until)>=0)s->bow_active=0;
    if(s->effect&&in->playing&&(!s->playing||(in->beat_q8>>10)!=(s->beat_q8>>10)))s->effect_epoch=in->now_ms;
    beat=in->beat_q8>>8;
    if(in->playing){
        if(in->section==3u&&(beat&1u))f=6; /* existing SEC_CHORUS/ST_ARMS */
        else if(in->vocal_level>=40u)f=4u+(beat&1u);
        else f=beat&3u;
    }else if(s->bow_active)f=7;
    else if(in->vocal_level>=40u)f=4u+(in->now_ms/250u&1u);
    else f=in->now_ms/700u&3u;
    /* Bounded light from actual analysis levels + absolute beat pulse; no
       pseudo-audio or accumulated per-redraw animation increments. */
    light=(uint32_t)in->audio_level/2u+(uint32_t)in->transient/4u;
    if(in->playing)light+=(255u-(in->beat_q8&255u))/4u;
    if(light>255u)light=255u;
    mouth=in->vocal_level?in->mouth<=5u?in->mouth:5u:5u;
    if(!s->initialized||s->sprite!=f)dirty|=PERF_DIRTY_SPRITE;
    if(!s->initialized||s->mouth!=mouth)dirty|=PERF_DIRTY_MOUTH;
    if(!s->initialized||s->light!=(uint8_t)light)dirty|=PERF_DIRTY_STAGE;
    if(!s->initialized||s->playing!=(in->playing!=0)||s->section!=in->section)dirty|=PERF_DIRTY_STATUS;
    s->now_ms=in->now_ms;s->beat_q8=in->beat_q8;s->sprite=(uint8_t)f;s->mouth=mouth;
    s->light=(uint8_t)light;s->playing=in->playing!=0;s->section=in->section;s->initialized=1;
    return dirty;
}
static uint16_t perf_sample(const PerfState *s,StageFxCache *cache,uint32_t x,uint32_t y){
    uint16_t base=PRGB(12,20,40),accent,dim;uint32_t beam,grid;
    if(!s||x>=PERF_W||y>=PERF_SCENE_H)return 0;
    accent=s->palette==PERF_PALETTE_AMBER?PRGB(248,176,64):PRGB(40,208,200);
    dim=s->palette==PERF_PALETTE_AMBER?PRGB(48,32,24):PRGB(16,48,64);
    if(!s->palette&&s->outfit<PERF_OUTFIT_N&&s->background){accent=style_colours[s->outfit][0];dim=style_colours[s->outfit][1];base=style_colours[s->outfit][2];}
    if(s->background==PERF_BG_GRID){
        grid=(x%24u==0u)||(y>=142u&&(y-142u)%18u==0u);
        if(grid)base=dim;
        beam=x<24u||x>=216u;
        if(beam&&y<142u&&((x+y)/16u&3u)==((s->beat_q8>>8)&3u))base=s->light>96u?accent:dim;
    }else if(s->background==PERF_BG_HORIZON){
        uint32_t roof=98u+((x/20u*37u+17u+s->outfit*7u)%57u);
        base=s->palette?PRGB(16+y/16u,16,48+y/8u):style_colours[s->outfit<PERF_OUTFIT_N?s->outfit:0][2];
        if(y>=roof&&y<164u){base=PRGB(8,12,28);if(x%20u>4u&&x%20u<8u&&y%12u<3u)base=dim;}
        if(y>=164u)base=PRGB(16,24,48);
        if(y==142u||y==144u)base=accent;
        if((x==18u||x==221u)&&y>24u&&y<142u)base=s->light>96u?accent:dim;
        if(y>150u&&((x+2u*y)%48u<2u))base=dim;
    }
    if(s->background==PERF_BG_CONCERT){
        uint32_t side=x<120u?x:239u-x;
        base=style_colours[s->outfit<PERF_OUTFIT_N?s->outfit:0][2];
        if(side<30u)base=(side/6u&1u)?dim:base; /* reusable curtains */
        if(y>=16u&&y<24u)base=(x%24u<3u||y==16u||y==23u)?PRGB(104,112,136):PRGB(24,24,40);
        if(y>=28u&&y<158u){
            int32_t dx=(int32_t)x-(x<120u?45:194);
            if(dx<0)dx=-dx;
            if((uint32_t)dx<(y-24u)/4u)base=s->light>40u?dim:base;
        }
        if(y>=164u){base=(((x/24u)+(y-164u)/16u)&1u)?PRGB(24,32,56):PRGB(40,40,64);if(y==164u||y==166u)base=accent;}
        if(y>=25u&&y<29u&&(x%48u>=18u&&x%48u<30u))base=accent;
    }
    if(s->background==PERF_BG_LED){
        uint32_t cell=(x/20u+y/16u+s->outfit)%4u;
        base=style_colours[s->outfit<PERF_OUTFIT_N?s->outfit:0][2];
        if(x%20u>2u&&x%20u<18u&&y%16u>2u&&y%16u<13u)base=cell==((s->beat_q8>>8)&3u)?accent:dim;
        if(y>=170u)base=((x/30u+(y-170u)/12u)&1u)?PRGB(24,20,40):PRGB(40,24,48);
    }
    if(s->frame==PERF_FRAME_CORNERS){
        if(((x>=4u&&x<28u)||(x>=212u&&x<236u))&&(y==4u||y==209u))base=accent;
        if((x==4u||x==235u)&&((y>=4u&&y<28u)||(y>=186u&&y<210u)))base=accent;
    }else if(s->frame==PERF_FRAME_RAILS){
        if((x==3u||x==236u)&&y>=8u&&y<206u)base=dim;
        if(((x>=2u&&x<=4u)||(x>=235u&&x<=237u))&&y>=8u&&y<8u+s->light*198u/255u)base=accent;
    }
    {uint32_t age=s->now_ms-s->effect_epoch;uint16_t fx;
     if(s->playing&&s->effect==STFX_BEAMS)age%=960u;
     fx=cache?stagefx_cached_pixel(cache,s->effect,age,s->intensity,x,y):stagefx_pixel(s->effect,age,s->intensity,x,y);if(fx)base=fx;}
    return base;
}

uint16_t perf_cached_pixel(const PerfState *s,StageFxCache *cache,uint32_t x,uint32_t y){return perf_sample(s,cache,x,y);}
uint16_t perf_pixel(const PerfState *s,uint32_t x,uint32_t y){return perf_sample(s,0,x,y);}

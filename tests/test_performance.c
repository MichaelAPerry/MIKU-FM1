/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/performance.h"
#include "firmware/ui/singer_sprites.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);fails++;}}while(0)
int main(void){
    PerfState s,ref;PerfInput in={0};const PerfOutfit *o=perf_outfit(0);
    perf_init(&s);CHECK(o&&o->width==SINGER_STAGE_W&&o->height==SINGER_STAGE_H&&o->frames==ST_COUNT);CHECK(!perf_outfit(PERF_OUTFIT_N));
    CHECK(!perf_update(0,&in));CHECK(!perf_update(&s,0));
    for(unsigned y=0;y<PERF_SCENE_H;y++)for(unsigned x=0;x<PERF_W;x++)CHECK(perf_pixel(&s,x,y)==0x08a5u);
    CHECK(!perf_pixel(&s,240,0)&&!perf_pixel(&s,0,214));
    ref=s;CHECK(!perf_select(&s,PERF_OUTFIT_N,0,0,0));CHECK(!memcmp(&s,&ref,sizeof s));
    CHECK(!perf_select(&s,0,PERF_BG_N,0,0));CHECK(!memcmp(&s,&ref,sizeof s));
    CHECK(perf_update(&s,&in)==15u);CHECK(s.sprite==ST_IDLE0&&s.mouth==5);CHECK(!perf_update(&s,&in));
    in.playing=1;in.section=3;in.beat_q8=256;in.mouth=2;in.vocal_level=80;in.audio_level=255;in.transient=255;
    CHECK(perf_update(&s,&in)&PERF_DIRTY_STATUS);CHECK(s.sprite==ST_ARMS&&s.mouth==2&&s.light==253);
    in.section=2;perf_update(&s,&in);CHECK(s.sprite==ST_SING1);
    in.beat_q8=512;perf_update(&s,&in);CHECK(s.sprite==ST_SING0);
    in.vocal_level=0;perf_update(&s,&in);CHECK(s.sprite==ST_IDLE2&&s.mouth==5);
    /* Different redraw costs cannot change frame selection at the same timestamp. */
    perf_init(&ref);perf_update(&ref,&in);
    for(unsigned t=0;t<1000;t+=7){in.now_ms=t;in.beat_q8=t*256u/500u;perf_update(&s,&in);}
    in.now_ms=1000;in.beat_q8=512;perf_update(&s,&in);perf_update(&ref,&in);
    CHECK(s.sprite==ref.sprite&&s.mouth==ref.mouth&&s.light==ref.light);
    /* Stop/bow expiration spans uint32 clock rollover. */
    in.now_ms=0xffffff00u;perf_update(&s,&in);in.playing=0;perf_update(&s,&in);CHECK(s.sprite==ST_BOW);
    in.now_ms+=1499u;perf_update(&s,&in);CHECK(s.sprite==ST_BOW);
    in.now_ms++;perf_update(&s,&in);CHECK(s.sprite!=ST_BOW);
    in.vocal_level=90;in.mouth=255;perf_update(&s,&in);CHECK(s.mouth==5&&s.sprite>=ST_SING0&&s.sprite<=ST_SING1);
    /* Every candidate is finite and leaves caller's footer alone. */
    for(unsigned bg=0;bg<PERF_BG_N;bg++)for(unsigned fr=0;fr<PERF_FRAME_N;fr++)for(unsigned pal=0;pal<PERF_PALETTE_N;pal++){
        CHECK(perf_select(&s,0,bg,fr,pal));unsigned changed=0;
        for(unsigned y=0;y<PERF_SCENE_H;y++)for(unsigned x=0;x<PERF_W;x++)changed+=perf_pixel(&s,x,y)!=0x08a5u;
        CHECK(bg||fr?changed>0:changed==0);CHECK(!perf_pixel(&s,10,230));
    }
    /* Effects start on the musical event, then advance in absolute milliseconds. */
    perf_init(&s);s.effect=1;in.playing=1;in.now_ms=1000;in.beat_q8=0;perf_update(&s,&in);CHECK(s.effect_epoch==1000);
    in.now_ms=1240;in.beat_q8=64;perf_update(&s,&in);CHECK(s.effect_epoch==1000);
    in.now_ms=2000;in.beat_q8=1024;perf_update(&s,&in);CHECK(s.effect_epoch==2000);
    printf("performance: %s (state %zu bytes)\n",fails?"FAILED":"ok",sizeof s);return fails!=0;
}

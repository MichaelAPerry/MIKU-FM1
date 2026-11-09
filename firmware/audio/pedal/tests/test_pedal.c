/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/pedal/pedal.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define CHECK(x) do{if(!(x)){printf("pedal FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
    Pedal p,q;PedalSet s;int32_t in[512],out[512],gen[512],copy[512];uint32_t i;
    pedal_init(&p);pedal_default(&s);
    for(i=0;i<512;i++){in[i]=(i&1)? -12345:23456;gen[i]=32767;}
    s.bypass=1;s.drive=100;s.tone=0;
    CHECK(pedal_process(&p,&s,in,0,out,256)&&!memcmp(in,out,sizeof in));
    CHECK(p.peak==23456&&p.transient==23456&&p.connected&&p.mean_square>0);
    s.level=0;for(i=0;i<100;i++)CHECK(pedal_process(&p,&s,in,0,out,256));
    for(i=0;i<512;i++)CHECK(!out[i]);
    CHECK(p.peak==23456&&p.mean_square>0); /* input analysis is upstream of mute */
    s.bypass=0;s.level=127;s.wet=100;s.tone=0;s.drive=100;
    pedal_init(&p);pedal_init(&q);
    CHECK(pedal_process(&p,&s,in,0,out,256));
    memcpy(copy,in,sizeof in);CHECK(pedal_process(&q,&s,copy,0,copy,256));CHECK(!memcmp(copy,out,sizeof copy));
    CHECK(out[0]!=in[0]&&out[1]!=in[1]&&out[0]>0&&out[1]<0); /* independent stereo */
    CHECK(pedal_process(&p,&s,0,0,out,256));
    for(i=0;i<512;i++)CHECK(!out[i]);
    CHECK(!p.connected&&!p.peak&&!p.mean_square&&!p.filter[0]&&!p.filter[1]);
    s.bypass=1;s.synth=127;s.mix_headroom=50;pedal_init(&p);
    for(i=0;i<512;i++)in[i]=32767;
    CHECK(pedal_process(&p,&s,in,gen,out,256));
    CHECK(!p.clipped_output&&out[0]==32766);
    CHECK(pedal_process(&p,&s,0,gen,out,256)&&out[0]==32767); /* standalone synth survives disconnect */
    for(i=0;i<512;i++)in[i]=(i&1)?INT32_MIN:INT32_MAX;
    s.mix_headroom=100;pedal_init(&p);CHECK(pedal_process(&p,&s,in,gen,out,256));
    CHECK(p.clipped_input==512&&p.clipped_output==256&&p.peak==32768);
    q=p;memcpy(copy,out,sizeof out);
    CHECK(!pedal_process(&p,&s,in,gen,out,257)&&!memcmp(&p,&q,sizeof p)&&!memcmp(copy,out,sizeof out));
    CHECK(!pedal_process(&p,&s,in,gen,out,0));
    CHECK(!pedal_process(0,&s,in,gen,out,256));CHECK(!pedal_process(&p,0,in,gen,out,256));
    /* Invalid controls clamp; endpoints never overflow; effect states never leak between instances. */
    s.wet=s.drive=s.tone=s.level=s.synth=s.mix_headroom=255;s.bypass=0;
    pedal_init(&p);pedal_init(&q);
    CHECK(pedal_process(&p,&s,in,gen,out,1));CHECK(!q.frames&&!q.filter[0]);
    for(i=0;i<100;i++){CHECK(pedal_process(&p,&s,in,gen,out,256));for(unsigned j=0;j<512;j++)CHECK(out[j]>=-32768&&out[j]<=32767);}
    /* Render partitioning cannot alter DSP or gain ramps. */
    pedal_default(&s);s.drive=73;s.tone=35;s.wet=80;
    pedal_init(&p);pedal_init(&q);
    for(i=0;i<512;i++)in[i]=(int32_t)(i*101u%65536u)-32768;
    CHECK(pedal_process(&p,&s,in,0,out,256));
    for(i=0;i<256;i+=16)CHECK(pedal_process(&q,&s,in+i*2u,0,copy+i*2u,16));
    CHECK(!memcmp(out,copy,sizeof out));
    s.wet=20;s.level=80;
    CHECK(pedal_process(&p,&s,in,0,out,256));
    for(i=0;i<256;i+=16)CHECK(pedal_process(&q,&s,in+i*2u,0,copy+i*2u,16));
    CHECK(!memcmp(out,copy,sizeof out));
    puts("pedal: all ok");return 0;
}

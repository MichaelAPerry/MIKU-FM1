/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/pedal/pedal.h"
static uint32_t pd_bound(uint32_t x,uint32_t hi){return x>hi?hi:x;}
static int32_t pd_clip(int32_t x){return x>32767?32767:x< -32768? -32768:x;}
static int32_t pd_abs(int32_t x){return x<0?-x:x;}
static int32_t pd_smooth(int32_t x,int32_t target){
    int32_t d=target-x,s=d/32;
    return x+(s?s:d>0?1:d<0?-1:0);
}
/* Bounded rational shoulder, max internal multiply < 1.9e9 even at full drive/input. */
static int32_t pd_drive(int32_t x,uint32_t drive){
    int32_t a;
    if(!drive)return x;
    x=x*(int32_t)(100u+drive*3u)/100;
    a=pd_abs(x);
    if(a>16384){a-=16384;a=16384+a*16384/(16384+a);}
    return x<0?-a:a;
}
void pedal_default(PedalSet *s){
    s->wet=100;s->drive=0;s->tone=100;s->level=127;s->synth=0;s->bypass=0;s->mix_headroom=50;
}
void pedal_init(Pedal *p){
    p->filter[0]=p->filter[1]=p->wet_q8=p->level_q8=p->synth_q8=0;
    p->mean_square=p->clipped_input=p->clipped_output=p->frames=0;
    p->peak=p->transient=0;p->initialized=p->connected=0;
}
int pedal_process(Pedal *p,const PedalSet *s,const int32_t *input,const int32_t *synth,int32_t *out,uint32_t n){
    uint32_t i,k,peak=0,energy=0,drive,tone,head;int32_t wet,level,sgain;
    if(!p||!s||!out||!n||n>PEDAL_MAX_FRAMES)return 0;
    wet=s->bypass?0:(int32_t)pd_bound(s->wet,100)*256;
    level=(int32_t)pd_bound(s->level,127)*256;sgain=(int32_t)pd_bound(s->synth,127)*256;
    drive=pd_bound(s->drive,100);tone=pd_bound(s->tone,100);head=pd_bound(s->mix_headroom,100);
    if(!p->initialized){p->wet_q8=wet;p->level_q8=level;p->synth_q8=sgain;p->initialized=1;}
    if(!input){p->filter[0]=p->filter[1]=0;p->connected=0;}
    else p->connected=1;
    for(i=0;i<n;i++){
        int32_t dry[2],gen[2];
        for(k=0;k<2;k++){
            int32_t raw=input?input[2u*i+k]:0,a;
            dry[k]=pd_clip(raw);gen[k]=synth?pd_clip(synth[2u*i+k]):0;
            if(raw!=dry[k])p->clipped_input++;
            a=pd_abs(dry[k]);if((uint32_t)a>peak)peak=(uint32_t)a;
            energy+=(uint32_t)(dry[k]*dry[k])>>8;
        }
        p->wet_q8=pd_smooth(p->wet_q8,wet);p->level_q8=pd_smooth(p->level_q8,level);p->synth_q8=pd_smooth(p->synth_q8,sgain);
        for(k=0;k<2;k++){
            int32_t fx=input?pd_drive(dry[k],drive):0,x,g;
            if(input&&tone<100u){p->filter[k]+=(fx-p->filter[k])*(int32_t)(8u+tone*248u/100u)/256;fx=p->filter[k];}
            else p->filter[k]=fx;
            x=dry[k]+(fx-dry[k])*p->wet_q8/25600;
            x=x*p->level_q8/32512;g=gen[k]*p->synth_q8/32512;
            if(input&&synth&&p->synth_q8){x=x*(int32_t)head/100;g=g*(int32_t)head/100;}
            x+=g;
            if(x>32767||x< -32768)p->clipped_output++;
            out[2u*i+k]=pd_clip(x);
        }
    }
    p->transient=(uint16_t)(peak>p->peak?peak-p->peak:0);p->peak=(uint16_t)peak;
    p->mean_square=energy/(2u*n)*256u;p->frames+=n;
    return 1;
}

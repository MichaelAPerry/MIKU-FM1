/* SPDX-License-Identifier: Apache-2.0 */
/* DX7 review fixes: LFO delay ramp (F2), hush and carrier-audibility retirement (F3), steal fade (F5), L4 > 0 sustain (F6).
 * Includes dx7.c directly so the static LFO delay generator can be exercised. */
#include "firmware/audio/dx7/dx7.c"
#include "firmware/audio/dx7/bank.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SR 44100u
static DxSynth s;
static uint8_t patch[128];
static int32_t out[2*SR*3];
/* Algorithm 32 (index 31): parallel carriers, sine, no velocity scaling, like test_dx7.c. */
static void base_patch(unsigned r4,unsigned l4){
    dx_init(&s,4);memcpy(patch,s.patch,sizeof patch);
    patch[111]=0;patch[114]=patch[115]=0;patch[110]=31;
    for(unsigned o=0;o<6;o++){uint8_t *p=patch+17*o;p[0]=p[1]=p[2]=99;p[3]=(uint8_t)r4;p[4]=p[5]=p[6]=99;p[7]=(uint8_t)l4;p[12]=56;p[14]=o==5?99:0;p[15]=2;}
    assert(dx_patch_valid(patch));
}
static uint32_t carriers_quiet(const DxVoice *v){
    for(unsigned o=0;o<6;o++)if((dx_algorithms[v->algorithm][o]&7u)==4u&&v->gain[o]>=32768)return 0;
    return 1;
}
static int32_t peak(const int32_t *x,unsigned a,unsigned b){int32_t m=0;for(unsigned i=a;i<b;i++){int32_t v=abs(x[2*i]);if(v>m)m=v;}return m;}
static int32_t step(const int32_t *x,unsigned a,unsigned b){int32_t m=0;for(unsigned i=a+1;i<b;i++){int32_t v=abs(x[2*i]-x[2*i-2]);if(v>m)m=v;}return m;}
static void run(unsigned frames){assert(frames<=sizeof out/8);memset(out,0,sizeof out);dx_render(&s,out,frames);}
static void blocks(unsigned n){for(unsigned i=0;i<n;i++){int32_t o[2*DX_BLOCK]={0};dx_render(&s,o,DX_BLOCK);}}

static void test_lfo_delay(void){
    for(unsigned dly=0;dly<=99;dly++){
        uint8_t p[128]={0};DxLfo l;int32_t prev=0;unsigned full=0;
        p[112]=35;p[113]=(uint8_t)dly;dx_lfo_init(&l,p);
        for(unsigned b=0;b<45000;b++){ /* 45000 blocks of 64 frames = 65 s */
            int32_t d=dx_lfo_delay(&l);
            assert(d>=prev);                       /* the ramp never falls, in particular not after wrapping */
            if(d>=(1<<24)-(1<<16))full=1;
            if(full)assert(d>=(1<<24)-(1<<16));
            prev=d;
        }
        assert(full);
    }
}
static void test_hush_and_retire(void){
    /* A slow release is still a real release: R4=40 takes ~4.4 s and a voice retires only when its carriers are quiet. */
    base_patch(40,0);dx_init(&s,4);assert(dx_patch(&s,patch));dx_note_on(&s,60,100);run(SR/2);
    dx_note_off(&s,60);
    {unsigned n=0;while(dx_active(&s)&&n<20u*SR/DX_BLOCK){
        const DxVoice *v=s.voice;uint8_t prev=v->active;blocks(1);n++;
        if(prev==1&&v->active!=1){assert(!v->gated&&carriers_quiet(v));}
    }
    printf("R4=40 retired %.2f s after note-off\n",n*64.0/SR);assert(!dx_active(&s)&&n*64u>SR&&n*64u<8u*SR);}
    /* R4=0 keeps its (minutes-long, inherent) tail; SAVE uses dx_hush to end it within two blocks */
    base_patch(0,0);dx_init(&s,4);assert(dx_patch(&s,patch));dx_note_on(&s,60,100);run(SR/2);dx_note_off(&s,60);
    blocks(10u*SR/DX_BLOCK);assert(dx_active(&s)==1);
    dx_hush(&s);blocks(2);assert(!dx_active(&s));
    /* hush: within two blocks, no hard step, whatever the position inside the block */
    base_patch(40,0);
    for(unsigned lead=0;lead<DX_BLOCK;lead+=21){
        unsigned k,m;int32_t st;
        dx_init(&s,4);assert(dx_patch(&s,patch));dx_set_mix(&s,100,64);
        for(k=0;k<4;k++)dx_note_on(&s,(uint8_t)(60+3*k),100);
        run(SR/4+lead);dx_all_off(&s);run(64);
        dx_hush(&s);memset(out,0,sizeof out);dx_render(&s,out,2*DX_BLOCK);
        assert(!dx_active(&s));
        for(m=2*DX_BLOCK;m<2*DX_BLOCK+128;m++)assert(!out[2*m]);
        st=step(out,0,2*DX_BLOCK);
        printf("hush lead %u: peak %d, max step %d\n",lead,peak(out,0,2*DX_BLOCK),st);
        assert(st*4<=peak(out,0,2*DX_BLOCK)+1);
    }
    /* factory patches: release tails are never truncated while audible */
    for(unsigned k=0;k<8;k++){
        unsigned n=0;
        dx_init(&s,4);assert(dx_patch(&s,dx7_bank[k]));dx_set_mix(&s,100,64);dx_note_on(&s,60,100);blocks(SR/DX_BLOCK);dx_note_off(&s,60);
        while(dx_active(&s)&&n<30u*SR/DX_BLOCK){
            const DxVoice *v=s.voice;uint8_t prev=v->active;blocks(1);n++;
            if(prev==1&&v->active!=1)assert(!v->gated&&carriers_quiet(v));
        }
        assert(!dx_active(&s));
        printf("%-10s retired %.2f s after note-off\n",dx7_names[k],n*64.0/SR);
    }
}
static void test_steal_fade(void){
    static const uint8_t a[4]={60,64,67,71},b[4]={62,65,69,72};
    base_patch(50,0);                                 /* release tails of about a second */
    for(unsigned o=0;o<6;o++)patch[17*o+13]=7<<2;     /* velocity sensitivity: velocity 1 is nearly silent, 127 is neutral */
    for(unsigned c=0;c<2;c++){
        unsigned at=SR,k;int32_t tail,j;
        dx_init(&s,4);assert(dx_patch(&s,patch));dx_set_mix(&s,100,64);
        for(k=0;k<4;k++)dx_note_on(&s,a[k],127);
        memset(out,0,sizeof out);dx_render(&s,out,SR);
        for(k=0;k<4;k++)dx_note_off(&s,a[k]);
        dx_render(&s,out+2*at,4410+c*17);at+=4410+c*17;   /* the second pass steals mid-block */
        tail=peak(out,at-256,at);assert(tail>300);
        for(k=0;k<4;k++)dx_note_on(&s,b[k],1);           /* steals all four sounding tails */
        dx_render(&s,out+2*at,512);
        j=step(out,at-2,at+130);
        printf("steal: tail peak %d, max step across steal %d\n",tail,j);
        assert(j*5<=tail);                                /* a hard cut gives steps near the tail amplitude */
    }
}
static void test_l4_sustain(void){
    base_patch(70,99);                                /* carrier L3 = L4 = 99: keeps sounding after key-off */
    dx_init(&s,4);assert(dx_patch(&s,patch));dx_set_mix(&s,100,64);dx_note_on(&s,60,100);
    run(SR/4);dx_note_off(&s,60);
    blocks(10u*SR/DX_BLOCK);
    assert(dx_active(&s)==1);
    run(2048);assert(peak(out,0,2048)>100);
    dx_hush(&s);blocks(3);assert(!dx_active(&s));
    /* a sustaining released voice is only taken by stealing */
    dx_init(&s,1);assert(dx_patch(&s,patch));dx_note_on(&s,60,100);run(SR/4);dx_note_off(&s,60);run(SR/4);
    dx_note_on(&s,72,1);assert(dx_active(&s)==1&&s.voice[0].note==72);
}
int main(void){
    test_lfo_delay();test_hush_and_retire();test_steal_fade();test_l4_sustain();
    printf("sizeof(DxSynth)=%zu\n",sizeof(DxSynth));
    puts("dx7 fixes ok");return 0;
}

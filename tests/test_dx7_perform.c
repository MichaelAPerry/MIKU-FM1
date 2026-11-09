/* SPDX-License-Identifier: Apache-2.0 */
/* DX7 performance controls: mono legato/glide, pitch bend, sustain pedal and their interplay with stealing and chunking. */
#include "firmware/audio/dx7/dx7.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define SR 44100u
static DxSynth s;
static uint8_t patch[128];
static int32_t buf[2*SR];
/* One sine carrier (operator 6, algorithm 32), no velocity scaling, sustained at L3; others silent. */
static void sine_patch(void){
    dx_init(&s,1);memcpy(patch,s.patch,sizeof patch);
    patch[111]=0;patch[114]=patch[115]=0;patch[110]=31;
    for(unsigned o=0;o<6;o++){uint8_t *p=patch+17*o;p[0]=p[1]=p[2]=99;p[3]=70;p[4]=p[5]=p[6]=99;p[7]=0;p[12]=56;p[14]=o==5?99:0;p[15]=2;}
    assert(dx_patch_valid(patch));
}
static void fresh(uint32_t voices){sine_patch();dx_init(&s,voices);assert(dx_patch(&s,patch));}
static void run(uint32_t n){while(n){uint32_t m=n>256?256:n;memset(buf,0,sizeof buf);dx_render(&s,buf,m);n-=m;}}
/* Mean frequency in Hz over n frames from positive-going zero crossings (left channel). */
static double hz(uint32_t n){
    uint32_t i,c=0,first=0,last=0;memset(buf,0,sizeof buf);dx_render(&s,buf,n);
    for(i=1;i<n;i++)if(buf[2*(i-1)]<=0&&buf[2*i]>0){if(!c)first=i;last=i;c++;}
    return c>1?(double)(c-1)*SR/(double)(last-first):0;
}
static int near(double a,double b,double tol){return a>b*(1-tol)&&a<b*(1+tol);}
static void test_legato(void){
    DxVoice before;int32_t b0[6];unsigned o;
    fresh(1);dx_note_on(&s,69,100);run(2000);assert(near(hz(8192),440.0,0.01));
    before=s.voice[0];memcpy(b0,before.base,sizeof b0);
    dx_note_legato(&s,81,20,0);
    assert(s.voice[0].active==1&&s.voice[0].gated&&s.voice[0].note==81&&s.voice[0].midi==before.midi+12);
    for(o=0;o<6;o++){assert(!memcmp(&s.voice[0].env[o],&before.env[o],sizeof before.env[o]));assert(s.voice[0].phase[o]==before.phase[o]);assert(s.voice[0].base[o]==b0[o]+12*1398101);}
    assert(!memcmp(&s.voice[0].lfo,&before.lfo,sizeof before.lfo)&&!memcmp(&s.voice[0].pitch,&before.pitch,sizeof before.pitch));
    run(256);assert(near(hz(8192),880.0,0.01));
    assert(s.voice[0].env[5].output==before.env[5].output);        /* the original velocity / key scaling is kept */
    /* no gated voice: behaves as note_on */
    dx_all_off(&s);run(SR);assert(!dx_active(&s));
    dx_note_legato(&s,60,100,50);assert(dx_active(&s)==1&&s.voice[0].gated&&s.voice[0].note==60&&!s.voice[0].gleft);
    /* a released voice is not gated: legato retriggers it */
    dx_note_off(&s,60);dx_note_legato(&s,64,100,0);assert(s.voice[0].gated&&s.voice[0].note==64&&s.voice[0].env[5].stage==0);
    dx_note_legato(&s,64,0,0);assert(!s.voice[0].gated);          /* velocity 0 is a note-off */
}
static void test_glide(void){
    double prev,f;uint32_t i;
    fresh(1);dx_note_on(&s,57,100);run(1000);
    dx_note_legato(&s,69,100,100);          /* 100 ms = 4410 samples = 69 blocks */
    assert(s.voice[0].gleft==69);
    prev=hz(1024);assert(prev>220.0&&prev<440.0);
    for(i=0;i<3;i++){f=hz(1024);assert(f>prev);prev=f;}   /* monotone rise */
    run(4410);assert(s.voice[0].gleft==0&&s.voice[0].gofs==0);assert(near(hz(8192),440.0,0.01));
    /* log-linear: halfway through the glide the pitch is the midpoint in semitones */
    dx_note_legato(&s,45,100,0);run(512);
    dx_note_legato(&s,57,100,640);          /* 640 ms: 441 blocks */
    run(220*64);f=hz(1024);assert(near(f,110.0*1.4142,0.06));
    /* a new legato during a glide continues from the current pitch */
    dx_note_legato(&s,69,100,0);run(256);assert(near(hz(8192),440.0,0.01));
    /* fixed operator does not glide */
    fresh(1);patch[5*17+15]=(uint8_t)(patch[5*17+15]|1u);patch[5*17+16]=0;assert(dx_patch(&s,patch));
    dx_note_on(&s,60,100);{int32_t b=s.voice[0].base[5];dx_note_legato(&s,72,100,100);assert(s.voice[0].base[5]==b&&s.voice[0].fixed==32);run(SR/4);assert(s.voice[0].gofs==0);}
    /* patch edits (transpose) after note-on do not alter legato intervals */
    fresh(1);dx_note_on(&s,60,100);patch[117]=36;assert(dx_patch(&s,patch));run(300);dx_note_legato(&s,72,100,0);run(256);assert(near(hz(8192),523.25,0.01));
}
static void test_bend(void){
    fresh(1);dx_note_on(&s,69,100);run(1000);
    dx_set_bend(&s,8191);run(256);assert(near(hz(8192),440.0*1.1225,0.01));     /* +2 semitones */
    dx_set_bend(&s,-8192);run(256);assert(near(hz(8192),440.0/1.1225,0.01));
    dx_set_bend_range(&s,12);dx_set_bend(&s,-8192);run(256);assert(near(hz(8192),220.0,0.01));
    dx_set_bend(&s,0);run(256);assert(near(hz(8192),440.0,0.01));
    /* held and releasing notes follow; no retrigger */
    dx_set_bend_range(&s,2);dx_set_bend(&s,4096);assert(s.voice[0].env[5].stage==3);
    dx_note_off(&s,69);run(256);assert(near(hz(2048),440.0*1.0595,0.03));
    dx_set_bend(&s,99999);assert(s.bend==(int32_t)(((int64_t)8191*2*1398101)>>13));
    fresh(2);assert(s.bend_range==2&&s.bend==0&&!s.sustain);
}
static void test_sustain(void){
    uint32_t i;
    fresh(4);dx_set_sustain(&s,1);dx_note_on(&s,60,100);dx_note_on(&s,64,100);
    dx_note_off(&s,60);run(SR);assert(s.voice[0].gated&&!s.voice[0].keyed&&s.voice[1].keyed);
    dx_set_sustain(&s,0);assert(!s.voice[0].gated&&s.voice[1].gated);      /* only the key-up note releases */
    run(2*SR);assert(dx_active(&s)==1);
    dx_set_sustain(&s,1);dx_note_off(&s,64);dx_note_on(&s,67,100);dx_note_on(&s,70,100);dx_note_off(&s,70);
    assert(s.voice[1].gated&&!s.voice[1].keyed);
    dx_all_off(&s);for(i=0;i<4;i++)assert(!s.voice[i].gated);               /* panic ignores the pedal */
    run(5*SR);assert(!dx_active(&s));
    fresh(4);dx_set_sustain(&s,1);dx_note_on(&s,60,100);dx_note_off(&s,60);run(256);dx_hush(&s);run(512);assert(!dx_active(&s));
    /* stealing: released first, then pedal-held, then oldest */
    fresh(3);dx_note_on(&s,50,100);dx_note_on(&s,51,100);dx_note_on(&s,52,100);run(256);
    dx_set_sustain(&s,1);dx_note_off(&s,51);dx_note_on(&s,53,100);                 /* all gated: pedal-held 51 is stolen, not oldest 50 */
    assert(s.voice[1].note==53&&s.voice[0].note==50&&s.voice[2].note==52);
    dx_set_sustain(&s,0);dx_note_off(&s,52);dx_note_on(&s,54,100);               /* released 52 beats older held 50 */
    assert(s.voice[2].note==54&&s.voice[0].note==50);
    fresh(2);dx_note_on(&s,50,100);dx_note_on(&s,51,100);dx_set_sustain(&s,1);dx_note_off(&s,51);dx_note_on(&s,52,100);
    assert(s.voice[1].note==52);
    fresh(2);dx_note_on(&s,50,100);dx_note_on(&s,51,100);dx_set_sustain(&s,1);dx_note_off(&s,50);dx_note_off(&s,51);dx_note_on(&s,52,100);
    assert(s.voice[0].note==52);                                                 /* all pedal-held: oldest */
}
static int32_t ra[2*SR],rb[2*SR];
#define TOTAL (4352u*9u)
static void chunked(int32_t *o,uint32_t chunk){
    uint32_t pos=0,step=0;
    fresh(1);dx_note_on(&s,48,100);
    while(pos<TOTAL){                                     /* events every 4352 = 17*256 frames, identical for both chunk sizes */
        uint32_t next=pos+4352,n;
        switch(step++){case 0:dx_note_legato(&s,60,100,40);break;case 1:dx_set_bend(&s,6000);break;case 2:dx_set_sustain(&s,1);dx_note_off(&s,60);break;
        case 3:dx_note_legato(&s,67,100,300);dx_set_bend(&s,-3000);break;case 4:dx_set_bend_range(&s,7);break;case 5:dx_set_sustain(&s,0);break;
        default:dx_note_on(&s,50+(int)step,90);}
        while(pos<next){n=next-pos<chunk?next-pos:chunk;dx_render(&s,o+2*pos,n);pos+=n;}
    }
}
static void test_chunking(void){
    DxSynth sa;
    memset(ra,0,sizeof ra);memset(rb,0,sizeof rb);chunked(ra,17);sa=s;chunked(rb,256);
    assert(!memcmp(ra,rb,sizeof ra)&&!memcmp(&sa,&s,sizeof s));
}
int main(void){
    test_legato();test_glide();test_bend();test_sustain();test_chunking();
    printf("sizeof(DxSynth)=%zu\ndx7 perform ok\n",sizeof(DxSynth));return 0;
}

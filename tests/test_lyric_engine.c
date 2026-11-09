/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
#include "firmware/song/lyric_edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("lyric_engine FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static Song g,h;static Engine e;static uint8_t encoded[SONG_ENC_SIZE];
static void setup(void){
    Step a={{60,2,6,12}},b={{64,2,6,12}},r={{0,2,0,VF_WORD_END|VF_PHRASE_END}};
    sg_init(&g);g.loop=0;g.bpm=120;g.vx.human=g.vx.scoop=0;g.trk[0].vib=g.trk[0].porta=0;g.vx.atk=40;
    sg_set_step(&g,0,0,0,&a);sg_set_step(&g,0,0,2,&b);sg_set_step(&g,0,0,4,&r);
    sg_row_insert(&g,0);sg_row_set(&g,0,TR_VOCAL,0);
}
static void init(const Song *song){memset(&e,0,sizeof e);en_init(&e,song);for(unsigned i=0;i<NTRK;i++)e.pan[i]=64;en_post(&e,EN_PLAY,0,0,0);}
static void advance(uint32_t frames){while(frames){int32_t out[512]={0};uint32_t n=frames<256?frames:256;en_render(&e,out,n);frames-=n;}}
static void capture(const Song *song,int32_t *audio,uint32_t frames){init(song);for(uint32_t at=0;at<frames;){uint32_t n=frames-at<256?frames-at:256;en_render(&e,audio+at*2u,n);at+=n;}}
static int timing(void){
    Seq s;SqEvent ev[SQ_EVMAX];uint32_t clock=0,on=0,off=0,firstoff=0;
    setup();g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_FLAG]|=VF_SUSTAIN|VF_LEGATO;
    sq_init(&s);CHECK(sq_play(&s,&g,0,0));
    while(clock<30000){uint32_t n=sq_block(&s,&g,1,ev,SQ_EVMAX);
        for(uint32_t i=0;i<n;i++)if(ev[i].trk==TR_VOCAL){
            if(ev[i].type==SQ_NOTE_ON){on++;if(on==2){CHECK(clock==11025);CHECK(ev[i].aux&VF_SUSTAIN);}}
            if(ev[i].type==SQ_NOTE_OFF){if(!off)firstoff=clock;off++;}
        }clock++;
    }
    CHECK(on==2&&off==1&&firstoff==22050&&!s.vn&&!s.vsl);
    /* Missing neighbor still releases at declared duration +2; an unmarked legacy gap is not an immediate rest. */
    setup();g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_NOTE]=0;g.pat[0][0][2].v[SV_FLAG]=12;
    sq_init(&s);CHECK(sq_play(&s,&g,0,0));clock=0;firstoff=0;
    while(clock<12000){uint32_t n=sq_block(&s,&g,1,ev,SQ_EVMAX);for(uint32_t i=0;i<n;i++)if(ev[i].type==SQ_NOTE_OFF&&ev[i].trk==TR_VOCAL&&!firstoff)firstoff=clock;clock++;}
    CHECK(firstoff==11027);
    return 0;
}
static int articulation(void){
    int32_t before;uint32_t d;
    setup();init(&g);advance(10000);CHECK(!e.fv.gated);advance(1026);CHECK(e.fv.gated&&fv_cons(&e.fv)==FC_K);before=e.fv.env;
    setup();g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_FLAG]|=VF_SUSTAIN|VF_LEGATO;
    init(&g);advance(10000);CHECK(e.fv.gated);advance(1025);CHECK(e.fv.gated);{int32_t held=e.fv.env;advance(1);CHECK(e.fv.gated&&fv_cons(&e.fv)==FC_NONE&&e.fv.env>=held-1&&e.fv.env>before);}
    CHECK(e.vnote==64&&e.vh==FV_A&&e.mora==6);advance(11025);CHECK(!e.fv.gated);advance(44100);CHECK(!e.fv.gated&&e.level==0&&!e.vqn);
    /* Sustain follows the actual preceding vowel, even if an externally constructed step has inconsistent mora metadata. */
    setup();g.pat[0][0][0].v[SV_MORA]=7;g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_FLAG]|=VF_SUSTAIN;
    init(&g);advance(11026);CHECK(e.vh==FV_I&&fv_cons(&e.fv)==FC_NONE&&e.fv.vowel==FV_I);
    /* On-beat queue retains the sustain flag and gives it zero consonant pre-start, including harmony. */
    setup();g.vx.obeat=100;g.vx.hint=80;g.vx.hmix=100;g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_FLAG]|=VF_SUSTAIN|VF_LEGATO;
    d=g.vx.obeat*2911u/100u*(25u+g.vx.clen*3u/2u)/100u;
    init(&g);advance(11026);CHECK(e.vnote==60);advance(d-1);CHECK(e.vnote==60);advance(1);
    CHECK(e.vnote==64&&e.fv.gated&&e.fv2.gated&&fv_cons(&e.fv)==FC_NONE&&fv_cons(&e.fv2)==FC_NONE);
    advance(64);CHECK(e.fv.gated&&e.fv2.gated);advance(11025);CHECK(!e.fv.gated&&!e.fv2.gated);advance(44100);CHECK(!e.vqn&&e.level==0);
    return 0;
}
static int flushing(void){
    for(uint32_t ob=0;ob<=100;ob+=100){
        setup();g.loop=1;g.vx.obeat=(uint8_t)ob;g.vx.hint=80;g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;
        {Step last={{60,1,6,12|VF_LEGATO}};sg_set_step(&g,0,0,15,&last);}
        init(&g);advance(44100u*5u);CHECK(sq_playing(&e.sq));en_post(&e,EN_STOP,0,0,0);advance(44100);
        CHECK(!e.fv.gated&&!e.fv2.gated&&!e.vqn&&!e.sq.vn&&!e.sq.vsl);
        /* Stop while first note is still waiting for its pre-start. */
        init(&g);advance(1);en_post(&e,EN_STOP,0,0,0);advance(44100);CHECK(!e.fv.gated&&!e.fv2.gated&&!e.vqn);
        init(&g);advance(3000);e.lock=1;advance(1000);CHECK(e.ack);e.lock=0;en_post(&e,EN_STOP,0,0,0);advance(44100);CHECK(!e.fv.gated&&!e.vqn);
        init(&g);advance(3000);g.trk[0].mute=1;advance(44100);CHECK(!e.fv.gated&&!e.fv2.gated&&!e.vqn&&!e.sq.vsl);
    }
    return 0;
}
static int audio(void){
    const uint32_t frames=44100u*3u;int32_t *a=calloc(frames*2u,sizeof *a),*b=calloc(frames*2u,sizeof *b);uint32_t peak=0,different=0;
    CHECK(a&&b);setup();capture(&g,a,frames);h=g;
    for(unsigned s=0;s<NSTEP;s++)if(h.pat[0][0][s].v[SV_NOTE])h.pat[0][0][s].v[SV_FLAG]|=VF_WORD_END;
    capture(&h,b,frames);CHECK(!memcmp(a,b,frames*2u*sizeof *a)); /* word markers alone never change sound */
    memset(b,0,frames*2u*sizeof *b);g.pat[0][0][0].v[SV_FLAG]|=VF_LEGATO;g.pat[0][0][2].v[SV_FLAG]|=VF_SUSTAIN;
    CHECK(song_encode(&g,encoded,sizeof encoded)==SONG_ENC_SIZE&&song_decode(&h,encoded,sizeof encoded)==SIO_OK);
    capture(&h,b,frames);for(uint32_t i=0;i<frames*2u;i++){uint32_t v=(uint32_t)(b[i]<0?-b[i]:b[i]);if(v>peak)peak=v;different+=a[i]!=b[i];}
    CHECK(different>4410&&peak>1000&&peak<32767&&!e.fv.gated&&!e.vqn);
    memset(a,0,frames*2u*sizeof *a);capture(&g,a,frames);CHECK(!memcmp(a,b,frames*2u*sizeof *a));
    /* Legacy v3 ignored high bits; decoding it cannot turn new articulation on. */
    encoded[4]=3;{uint32_t c=sg_crc32(encoded,sizeof encoded-4);for(unsigned i=0;i<4;i++)encoded[sizeof encoded-4+i]=(uint8_t)(c>>(8*i));}
    CHECK(song_decode(&h,encoded,sizeof encoded)==SIO_OK);for(unsigned i=0;i<NSTEP;i++)g.pat[0][0][i].v[SV_FLAG]&=15;
    memset(a,0,frames*2u*sizeof *a);memset(b,0,frames*2u*sizeof *b);capture(&g,a,frames);capture(&h,b,frames);CHECK(!memcmp(a,b,frames*2u*sizeof *a));
    printf("lyric audio: peak %u, %u changed samples; v4 roundtrip and legacy sound exact\n",peak,different);free(a);free(b);return 0;
}
int main(void){CHECK(!timing());CHECK(!articulation());CHECK(!flushing());CHECK(!audio());puts("lyric_engine: all ok");return 0;}

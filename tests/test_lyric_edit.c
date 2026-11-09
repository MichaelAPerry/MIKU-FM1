/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/lyric_edit.h"
#include "firmware/song/mora.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("lyric_edit FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static Song g,before,h;
static uint8_t bytes[SONG_ENC_SIZE];
static void crc(uint8_t *b,uint32_t len){uint32_t c=sg_crc32(b,len-4),i;for(i=0;i<4;i++)b[len-4+i]=(uint8_t)(c>>(i*8));}
int main(void){
    LyUnit u[16],r[16],sent[16];uint8_t pitches[16]={60,62,64,0,67,0,69},readpitch[16];uint32_t n=0,rn=99,i;
    sg_init(&g);CHECK(ly_compile("mi-ku:2 _:4 |:2 a:3! / i",1,u,16,&n,0)==LY_OK&&n==7);
    before=g;
    CHECK(ly_apply(&g,0,u,n,pitches,60,12)==LE_OK);
    CHECK(!memcmp(&g.pat[1],&before.pat[1],sizeof g.pat-sizeof g.pat[0]));
    CHECK(g.pat[0][0][0].v[SV_FLAG]==(12|VF_LEGATO));
    CHECK(g.pat[0][0][1].v[SV_LEN]==2&&(g.pat[0][0][1].v[SV_FLAG]&(VF_WORD_END|VF_LEGATO))==(VF_WORD_END|VF_LEGATO));
    CHECK(g.pat[0][0][3].v[SV_NOTE]==64&&(g.pat[0][0][3].v[SV_FLAG]&VF_SUSTAIN));
    CHECK(ly_rest_step(&g.pat[0][0][7])&&(g.pat[0][0][7].v[SV_FLAG]&VF_PHRASE_END));
    CHECK(ly_read(&g,0,r,readpitch,16,&rn)==LE_OK&&rn==8); /* trailing gap retained */
    CHECK(r[3].flags&LY_PHRASE_END);CHECK(r[2].flags&LY_SUSTAIN);CHECK(r[7].flags&LY_REST);
    CHECK(!(r[4].flags&LY_SEPARATE)); /* after phrase rest, a new attack is already inherent */
    CHECK(!memcmp(readpitch,pitches,n));
    h=g;CHECK(ly_apply(&h,1,r,rn,readpitch,60,12)==LE_OK);
    CHECK(!memcmp(g.pat[0][0],h.pat[0][1],14u*sizeof(Step))); /* normalized trailing gap differs only in its marker */
    CHECK(song_encode(&g,bytes,sizeof bytes)==SONG_ENC_SIZE&&bytes[4]==4);
    CHECK(song_decode(&h,bytes,sizeof bytes)==SIO_OK&&!memcmp(&g,&h,sizeof g));
    /* All legacy versions accepted these high bits but ignored them: import cannot activate new articulation. */
    for(i=1;i<=3;i++){
        uint32_t len=i==1?SONG_ENC_V1:i==2?SONG_ENC_V2:SONG_ENC_V3;
        CHECK(song_encode(&g,bytes,sizeof bytes)==SONG_ENC_SIZE);bytes[4]=(uint8_t)i;
        bytes[6]=(uint8_t)len;bytes[7]=(uint8_t)(len>>8);bytes[8]=bytes[9]=0;crc(bytes,len);
        CHECK(song_decode(&h,bytes,len)==SIO_OK);
        for(uint32_t p=0;p<NPAT;p++)for(uint32_t s=0;s<NSTEP;s++)CHECK(!(h.pat[0][p][s].v[SV_FLAG]&240));
    }
    before=g;u[0].ticks=17;CHECK(ly_apply(&g,0,u,n,pitches,60,12)==LE_RANGE&&!memcmp(&before,&g,sizeof g));u[0].ticks=1;
    u[0].flags|=LY_SUSTAIN;CHECK(ly_apply(&g,0,u,n,pitches,60,12)==LE_SUSTAIN&&!memcmp(&before,&g,sizeof g));u[0].flags&=(uint8_t)~LY_SUSTAIN;
    u[0].ticks=16;CHECK(ly_apply(&g,0,u,n,pitches,60,12)==LE_CAPACITY&&!memcmp(&before,&g,sizeof g));u[0].ticks=1;
    u[0].flags|=128;CHECK(ly_apply(&g,0,u,n,pitches,60,12)==LE_FLAGS&&!memcmp(&before,&g,sizeof g));u[0].flags&=127;
    CHECK(ly_read(&g,0,r,readpitch,16,&rn)==LE_OK);memcpy(sent,r,sizeof r);rn=99;
    CHECK(ly_read(&g,0,r,readpitch,1,&rn)==LE_CAPACITY&&rn==99&&!memcmp(sent,r,sizeof r));
    h=g;h.pat[0][0][0].v[SV_LEN]=3;CHECK(ly_read(&h,0,r,readpitch,16,&rn)==LE_OVERLAP&&rn==99&&!memcmp(sent,r,sizeof r));
    h=g;h.pat[0][0][14].v[SV_LEN]=3;h.pat[0][0][14].v[SV_NOTE]=60;CHECK(ly_read(&h,0,r,readpitch,16,&rn)==LE_RANGE);
    /* Pattern undo includes lyric flags; applying identical lyrics to another melody retains destination pitches. */
    {Undo undo;un_init(&undo);CHECK(un_push_pat(&undo,&g,0,0));
     CHECK(ly_compile("a:2-i:2 u:2!",1,u,16,&n,0)==LY_OK);
     CHECK(ly_apply(&g,0,u,n,0,72,9)==LE_OK&&g.pat[0][0][0].v[SV_NOTE]==60&&g.pat[0][0][2].v[SV_NOTE]==72);
     CHECK(!(g.pat[0][0][2].v[SV_FLAG]&VF_LEGATO));CHECK(un_pop(&undo,&g)&&!memcmp(&g,&before,sizeof g));}
    {Step legacy={{0,4,6,12}};CHECK(!ly_rest_step(&legacy));}
    puts("lyric_edit: all ok");return 0;
}

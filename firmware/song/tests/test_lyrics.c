/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/lyrics.h"
#include "firmware/song/mora.h"
#include "firmware/audio/fvoice/fvoice.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){printf("lyrics FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void){
    LyUnit u[32]={{0}},before[32];uint32_t n=99,e=99;
    CHECK(ly_compile("mi-ku:2 _:4 |:2 a:3! / i",1,u,32,&n,&e)==LY_OK);
    CHECK(n==7&&u[0].mora==mora_find("mi")&&!(u[0].flags&LY_WORD_END));
    CHECK(u[1].ticks==2&&(u[1].flags&LY_WORD_END));
    CHECK(u[2].ticks==4&&(u[2].flags&LY_SUSTAIN)&&ly_consonant(&u[2])==MC_NONE&&ly_vowel(&u[2])==MV_U);
    CHECK((u[3].flags&(LY_REST|LY_PHRASE_END))==(LY_REST|LY_PHRASE_END));
    CHECK(u[4].flags&LY_SEPARATE);
    CHECK(ly_compile("MI-KU",1,u,32,&n,&e)==LY_OK&&n==2);
    memcpy(before,u,sizeof u);n=99;
    CHECK(ly_compile("mi kya",1,u,32,&n,&e)==LY_MORA&&e==3&&n==99&&!memcmp(u,before,sizeof u));
    CHECK(ly_compile("mi ku",1,u,1,&n,&e)==LY_CAPACITY&&n==99&&!memcmp(u,before,sizeof u));
    CHECK(ly_compile("_:2",1,u,32,&n,&e)==LY_MORA);
    CHECK(ly_compile("n _",1,u,32,&n,&e)==LY_MORA);
    CHECK(ly_compile("a / _",1,u,32,&n,&e)==LY_MORA);
    CHECK(ly_compile("a:0",1,u,32,&n,&e)==LY_DURATION);
    CHECK(ly_compile("a:65536",1,u,32,&n,&e)==LY_DURATION);
    CHECK(ly_compile("a:99999999999999",1,u,32,&n,&e)==LY_DURATION);
    CHECK(ly_compile("a:65535",1,u,32,&n,&e)==LY_OK&&u[0].ticks==65535);
    CHECK(ly_compile("a-",1,u,32,&n,&e)==LY_SYNTAX);
    CHECK(ly_compile("a--i",1,u,32,&n,&e)==LY_SYNTAX);
    CHECK(ly_compile("a_",1,u,32,&n,&e)==LY_SYNTAX);
    CHECK(ly_compile("a _!",1,u,32,&n,&e)==LY_SYNTAX);
    CHECK(ly_compile("mi-ku | a",1,0,0,&n,&e)==LY_OK&&n==4);
    CHECK(ly_compile(" \n",1,u,32,&n,&e)==LY_OK&&n==0);
    CHECK(ly_compile(0,1,u,32,&n,&e)==LY_ARGUMENT);
    CHECK(ly_compile("a",0,u,32,&n,&e)==LY_ARGUMENT);
    /* Actual device voice: legato syllables articulate, melisma retains the vowel without a consonant,
       rests release, and mouth animation reads the sounding voice rather than text estimates. */
    {
        FvVoice v; int32_t audio[32]; uint32_t i; int32_t held;
        CHECK(ly_compile("mi-ku _:2 |",1,u,32,&n,&e)==LY_OK);
        fv_init(&v);v.human=0;v.vib_depth=0;v.scoop=0;
        fv_note_on_mora(&v,60,(uint8_t)ly_consonant(&u[0]),(uint8_t)ly_vowel(&u[0]),110);
        CHECK(v.gated&&fv_mouth(&v)==5&&fv_cons(&v)==FC_M);
        for(i=0;i<1000;i++){memset(audio,0,sizeof audio);fv_render(&v,audio,16);}
        held=v.env;CHECK(held>0);
        fv_note_on_mora(&v,62,(uint8_t)ly_consonant(&u[1]),(uint8_t)ly_vowel(&u[1]),110);
        CHECK(v.gated&&v.env==held&&fv_cons(&v)==FC_K);
        for(i=0;i<1000;i++){memset(audio,0,sizeof audio);fv_render(&v,audio,16);}
        fv_note_on_mora(&v,64,(uint8_t)ly_consonant(&u[2]),(uint8_t)ly_vowel(&u[2]),110);
        CHECK(v.gated&&fv_cons(&v)==FC_NONE&&fv_mouth(&v)==FV_U);
        CHECK(u[3].flags&LY_REST);fv_note_off(&v);
        for(i=0;i<3000;i++){memset(audio,0,sizeof audio);fv_render(&v,audio,16);}
        CHECK(!v.gated&&fv_level(&v)==0&&fv_mouth(&v)==5);
    }
    puts("lyrics: all ok");return 0;
}

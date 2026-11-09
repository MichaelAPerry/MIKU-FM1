/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/performance_wire.h"
#include "firmware/song/mora.h"
static uint32_t pw_valid(const PwState *s){
    return s&&s->sequence<16384u&&s->mouth<6u&&s->vocal_level<128u&&s->mora<MORA_N&&
        s->section<6u&&s->row<64u&&s->step<16u&&s->playing<2u&&s->sprite<8u&&
        s->outfit<128u&&s->background<128u&&s->frame<128u&&s->palette<128u&&
        s->audio_level<128u&&s->transient<128u;
}
uint32_t pw_encode(uint8_t *o,uint32_t cap,const PwState *s){
    uint32_t i;uint8_t sum=0;
    if(!o||cap<PW_SIZE||!pw_valid(s))return 0;
    o[0]=0xf0;o[1]=0x7d;o[2]='M';o[3]='F';o[4]=1;
    o[5]=(uint8_t)(s->sequence&127u);o[6]=(uint8_t)(s->sequence>>7);
    for(i=0;i<5;i++)o[7+i]=(uint8_t)((s->sample>>(7u*i))&127u);
    o[12]=s->mouth;o[13]=s->vocal_level;o[14]=s->mora;o[15]=s->section;
    o[16]=s->row;o[17]=s->step;o[18]=s->playing;o[19]=s->sprite;
    o[20]=s->outfit;o[21]=s->background;o[22]=s->frame;o[23]=s->palette;
    o[24]=s->audio_level;o[25]=s->transient;
    for(i=1;i<26;i++)sum^=o[i];
    o[26]=sum;o[27]=0xf7;return PW_SIZE;
}
uint32_t pw_decode(PwState *out,const uint8_t *m,uint32_t n){
    PwState s;uint32_t i;uint8_t sum=0;
    if(!out||!m||n!=PW_SIZE||m[0]!=0xf0||m[1]!=0x7d||m[2]!='M'||m[3]!='F'||m[4]!=1||m[27]!=0xf7||m[11]>15u)return 0;
    for(i=1;i<27;i++){if(m[i]&128u)return 0;sum^=m[i];}if(sum)return 0;
    s.sequence=(uint16_t)(m[5]|((uint16_t)m[6]<<7));s.sample=0;
    for(i=0;i<5;i++)s.sample|=(uint32_t)m[7+i]<<(7u*i);
    s.mouth=m[12];s.vocal_level=m[13];s.mora=m[14];s.section=m[15];
    s.row=m[16];s.step=m[17];s.playing=m[18];s.sprite=m[19];
    s.outfit=m[20];s.background=m[21];s.frame=m[22];s.palette=m[23];
    s.audio_level=m[24];s.transient=m[25];
    if(!pw_valid(&s))return 0;
    /* Volatile scalar stores prevent old freestanding compilers from lowering
       a struct copy to unavailable libc memcpy; padding is never copied. */
    {volatile PwState *v=out;
     v->sample=s.sample;v->sequence=s.sequence;v->mouth=s.mouth;v->vocal_level=s.vocal_level;
     v->mora=s.mora;v->section=s.section;v->row=s.row;v->step=s.step;v->playing=s.playing;v->sprite=s.sprite;
     v->outfit=s.outfit;v->background=s.background;v->frame=s.frame;v->palette=s.palette;
     v->audio_level=s.audio_level;v->transient=s.transient;}
    return PW_SIZE;
}

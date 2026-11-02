/* SPDX-License-Identifier: MIT */
#include "shared/song_io.h"
#include <string.h>
static void wr16(uint8_t *p,uint16_t x){p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8);}
static void wr32(uint8_t *p,uint32_t x){wr16(p,(uint16_t)x);wr16(p+2,(uint16_t)(x>>16));}
static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t rd32(const uint8_t *p){return (uint32_t)rd16(p)|((uint32_t)rd16(p+2)<<16);}
static uint32_t crc32(const uint8_t *buf,size_t n){
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<n;i++){c^=buf[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);}
    return ~c;
}
size_t miku_song_encoded_size(const MikuSong *s){
    if(!s || s->version!=1 || s->bpm<20 || s->bpm>300 || s->pattern_count>MIKU_MAX_PATTERNS) return 0;
    size_t n=14; /* magic 4, version 2, bpm 2, count 1, mute 4, reserved 1 */
    for(uint8_t i=0;i<s->pattern_count;i++){
        const MikuPattern *p=&s->patterns[i];
        if(p->count>MIKU_MAX_NOTES) return 0;
        n+=2+(size_t)p->count*12u;
        for(uint16_t j=0;j<p->count;j++)if(!miku_note_valid(&p->notes[j]))return 0;
    }
    return n+4;
}
int miku_song_encode(const MikuSong *s,uint8_t *b,size_t cap,size_t *used){
    size_t n=miku_song_encoded_size(s);if(!n||!b||cap<n)return 0;
    memcpy(b,"MIK1",4);wr16(b+4,1);wr16(b+6,s->bpm);b[8]=s->pattern_count;
    for(int i=0;i<4;i++)b[9+i]=!!s->mute[i];
    b[13]=0;size_t o=14;
    for(uint8_t i=0;i<s->pattern_count;i++){
        const MikuPattern *p=&s->patterns[i];wr16(b+o,p->count);o+=2;
        for(uint16_t j=0;j<p->count;j++){
            const MikuNote *v=&p->notes[j];
            wr32(b+o,v->start_tick);wr32(b+o+4,v->duration_tick);
            b[o+8]=v->pitch;b[o+9]=v->velocity;b[o+10]=v->vowel;b[o+11]=v->track;o+=12;
        }
    }
    wr32(b+o,crc32(b,o));if(used)*used=n;return 1;
}
int miku_song_decode(MikuSong *s,const uint8_t *b,size_t len){
    if(!s||!b||len<18||memcmp(b,"MIK1",4)||rd16(b+4)!=1||b[8]>MIKU_MAX_PATTERNS||
       rd16(b+6)<20||rd16(b+6)>300 || b[13]!=0 || rd32(b+len-4)!=crc32(b,len-4)) return 0;
    /* large fixed MikuSong is caller-owned, write only after validation. */
    size_t o=14;uint8_t count=b[8];
    for(uint8_t i=0;i<count;i++){
        if(o>len-4||len-4-o<2)return 0;
        uint16_t nc=rd16(b+o);o+=2;
        if(nc>MIKU_MAX_NOTES || (size_t)nc>(len-4-o)/12)return 0;
        for(uint16_t j=0;j<nc;j++){
            MikuNote n={rd32(b+o),rd32(b+o+4),b[o+8],b[o+9],b[o+10],b[o+11]};
            if(!miku_note_valid(&n))return 0;o+=12;
        }
    }
    if(o!=len-4)return 0;
    memset(s,0,sizeof(*s));s->version=1;s->bpm=rd16(b+6);s->pattern_count=count;
    for(int i=0;i<4;i++)s->mute[i]=b[9+i];
    o=14;
    for(uint8_t i=0;i<count;i++){
        MikuPattern *p=&s->patterns[i];p->count=rd16(b+o);o+=2;
        for(uint16_t j=0;j<p->count;j++){
            p->notes[j]=(MikuNote){rd32(b+o),rd32(b+o+4),b[o+8],b[o+9],b[o+10],b[o+11]};o+=12;
        }
    }
    return 1;
}

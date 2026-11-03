/* SPDX-License-Identifier: GPL-3.0-only */
/* MKS2: "MKS2" u16 version(1) u32 total length | name[8] bpm swing master transpose loop nrows |
 * 4 x (level mute solo preset vib porta breath bright) | 64 x (pat[4] sect) | 4*16*16 x step[4] | u32 CRC32(IEEE) of all before. */
#include "firmware/song/song.h"
uint32_t sg_crc32(const uint8_t *p,uint32_t n){
    uint32_t c=0xFFFFFFFFu,i,k;
    for(i=0;i<n;i++){c^=p[i];for(k=0;k<8;k++)c=(c>>1)^(0xEDB88320u&(0u-(c&1u)));}
    return ~c;
}
static void w32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint32_t r32(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
uint32_t song_encode(const Song *g,uint8_t *b,uint32_t cap){
    uint32_t i,t,p,s,k,o=0;
    if(cap<SONG_ENC_SIZE)return 0;
    b[o++]='M';b[o++]='K';b[o++]='S';b[o++]='2';b[o++]=1;b[o++]=0;
    w32(b+o,SONG_ENC_SIZE);o+=4;
    for(i=0;i<8;i++)b[o++]=(uint8_t)g->name[i];
    b[o++]=g->bpm;b[o++]=g->swing;b[o++]=g->master;b[o++]=(uint8_t)g->transpose;b[o++]=g->loop;b[o++]=g->nrows;
    for(t=0;t<NTRK;t++){
        const Trk *x=&g->trk[t];
        b[o++]=x->level;b[o++]=x->mute;b[o++]=x->solo;b[o++]=x->preset;
        b[o++]=x->vib;b[o++]=x->porta;b[o++]=x->breath;b[o++]=x->bright;
    }
    for(i=0;i<NROW;i++){for(t=0;t<NTRK;t++)b[o++]=g->row[i].pat[t];b[o++]=g->row[i].sect;}
    for(t=0;t<NTRK;t++)for(p=0;p<NPAT;p++)for(s=0;s<NSTEP;s++)for(k=0;k<4;k++)b[o++]=g->pat[t][p][s].v[k];
    w32(b+o,sg_crc32(b,o));o+=4;
    return o;
}
/* One walk over a (CRC-checked) buffer: validates every field; also stores them when d != 0. */
static int parse(const uint8_t *b,Song *d){
    uint32_t i,t,p,s,k;
    const uint8_t *q=b+10;
    for(i=0;i<8;i++){
        uint8_t c=q[i];
        if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '))return SIO_FIELD;
    }
    q+=8;
    if(q[0]<40||q[0]>240||q[1]>75||q[2]>127||(int8_t)q[3]<-24||(int8_t)q[3]>24||q[4]>1||q[5]>NROW)return SIO_FIELD;
    q+=6;
    for(t=0;t<NTRK;t++,q+=8)
        if(q[0]>127||q[1]>1||q[2]>1||q[3]>7||q[4]>100||q[5]>100||q[6]>100||q[7]>100)return SIO_FIELD;
    for(i=0;i<NROW;i++,q+=5){
        for(t=0;t<NTRK;t++)if(q[t]>=NPAT&&q[t]!=ROW_OFF)return SIO_FIELD;
        if(q[4]>=SEC_N)return SIO_FIELD;
    }
    for(t=0;t<NTRK;t++)for(p=0;p<NPAT;p++)for(s=0;s<NSTEP;s++,q+=4){
        Step st;
        for(k=0;k<4;k++)st.v[k]=q[k];
        if(!sg_step_ok((int)t,&st))return SIO_FIELD;
    }
    if(!d)return SIO_OK;
    q=b+10;
    for(i=0;i<8;i++)d->name[i]=(char)q[i];
    q+=8;
    d->bpm=q[0];d->swing=q[1];d->master=q[2];d->transpose=(int8_t)q[3];d->loop=q[4];d->nrows=q[5];q+=6;
    for(t=0;t<NTRK;t++,q+=8){
        Trk *x=&d->trk[t];
        x->level=q[0];x->mute=q[1];x->solo=q[2];x->preset=q[3];x->vib=q[4];x->porta=q[5];x->breath=q[6];x->bright=q[7];
    }
    for(i=0;i<NROW;i++,q+=5){for(t=0;t<NTRK;t++)d->row[i].pat[t]=q[t];d->row[i].sect=q[4];}
    for(t=0;t<NTRK;t++)for(p=0;p<NPAT;p++)for(s=0;s<NSTEP;s++,q+=4)for(k=0;k<4;k++)d->pat[t][p][s].v[k]=q[k];
    return SIO_OK;
}
int song_decode(Song *g,const uint8_t *b,uint32_t len){
    int e;
    if(len!=SONG_ENC_SIZE)return SIO_SIZE;
    if(b[0]!='M'||b[1]!='K'||b[2]!='S'||b[3]!='2')return SIO_MAGIC;
    if(b[4]!=1||b[5]!=0)return SIO_VERSION;
    if(r32(b+6)!=len)return SIO_LEN;
    if(r32(b+len-4)!=sg_crc32(b,len-4))return SIO_CRC;
    e=parse(b,0);
    if(e)return e;
    return parse(b,g);
}

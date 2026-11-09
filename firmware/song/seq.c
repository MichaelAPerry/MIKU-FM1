/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/seq.h"
#include "firmware/song/mora.h"
#define N15 661500u                 /* 44100*60/4: frames per sixteenth = N15/bpm */
enum { ST_STOP, ST_PLAY, ST_FLUSH };
enum { K_NONE, K_VOFF, K_BOFF, K_COFF, K_STEP, K_STRUM, K_ARP };
typedef struct { Seq *s; const Song *g; SqEvent *ev; uint32_t max, n, pos; } C;
static const int8_t IV[10][4]={{0,4,7,0},{0,3,7,0},{0,4,7,10},{0,4,7,11},{0,3,7,10},{0,5,7,0},{0,2,7,0},{0,3,6,0},{0,4,8,0},{0,4,7,14}};
static const uint8_t IN[10]={3,3,4,4,4,3,3,3,3,4};
static void em(C *c,uint8_t type,uint8_t trk,uint8_t note,uint8_t vel,uint8_t aux,uint8_t mora){
    SqEvent *e;
    if(c->n>=c->max)return;
    e=&c->ev[c->n++];
    e->at=c->pos;e->type=type;e->trk=trk;e->note=note;e->vel=vel;e->aux=aux;e->mora=mora;
}
static uint8_t vmap(uint32_t v){return (uint8_t)(v<<3|7u);}
static int aud(const Song *g,int t){
    int i,so=0;
    for(i=0;i<NTRK;i++)so|=g->trk[i].solo;
    return !g->trk[t].mute && (!so||g->trk[t].solo);
}
static void tm_add(Seq *s,uint8_t k,int32_t due,uint8_t a,uint8_t b){
    int i;
    for(i=0;i<SQ_NTM;i++)if(s->tm[i].kind==K_NONE){
        s->tm[i].kind=k;s->tm[i].due=due<1?1:due;s->tm[i].a=a;s->tm[i].b=b;return;
    }
}
static void tm_cancel(Seq *s,uint8_t k){int i;for(i=0;i<SQ_NTM;i++)if(s->tm[i].kind==k)s->tm[i].kind=K_NONE;}
static int32_t swf(const Seq *s,const Song *g){return (int32_t)((N15/s->bpm)*g->swing/100u);}
/* frames from the sounding of a note starting on `step` to the end of its len, swing included */
static int32_t dur(const Seq *s,const Song *g,uint32_t step,uint32_t len){
    int32_t sw=swf(s,g),d=(int32_t)(len*N15/s->bpm);
    if((step+len)&1u)d+=sw;
    if(step&1u)d-=sw;
    return d<1?1:d;
}
static void cs_off(C *c){
    Seq *s=c->s;uint32_t i;
    for(i=0;i<s->ncs;i++)em(c,SQ_NOTE_OFF,TR_CHORDS,s->cs[i],0,0,0);
    s->ncs=0;
}
static void chord_kill(C *c){
    Seq *s=c->s;
    cs_off(c);tm_cancel(s,K_COFF);tm_cancel(s,K_STRUM);tm_cancel(s,K_ARP);s->ap_on=0;
}
static void cs_on(C *c,uint8_t n){
    Seq *s=c->s;
    em(c,SQ_NOTE_ON,TR_CHORDS,n,s->cvel,0,0);
    if(s->ncs<8)s->cs[s->ncs++]=n;
}
static uint32_t arp_ix(uint32_t mode,uint32_t n,uint32_t i){
    uint32_t cyc,j;
    if(mode==CM_UP)return i%n;
    if(mode==CM_DOWN)return n-1u-i%n;
    if(n<2)return 0;
    cyc=2u*n-2u;j=i%cyc;
    return j<n?j:cyc-j;
}
static void arp_tick(C *c){
    Seq *s=c->s;
    if(!s->ap_on)return;
    cs_off(c);
    cs_on(c,s->cn[arp_ix(s->ap_mode,s->ncn,s->ap_i)]);
    s->ap_i++;
    if(--s->ap_left==0)s->ap_on=0;
}
static void arp_go(C *c){
    Seq *s=c->s;
    arp_tick(c);
    if(s->ap_on&&s->ap_fast)tm_add(s,K_ARP,(int32_t)(s->slen/2u),0,0);
}
/* voicing: the chord inversion nearest (voice-leading) to the previous chord, within root-6..root+16 */
static void voicing(Seq *s,int root,int q){
    int n=IN[q],have=0,bc=0,ks,k,i,m;
    uint8_t out[4];
    for(ks=0;ks<2*n-1;ks++){
        int cost=0,j,oc,lo,ok=1;
        k=(ks&1)?(ks+1)/2:-(ks/2);        /* 0 1 -1 2 -2 ... */
        if(k>=n||k<=-n)continue;
        if(!s->npv&&k)continue;
        for(i=0;i<n;i++){
            j=i+k;oc=j>=0?j/n:-((-j+n-1)/n);
            out[i]=0;
            {int v=root+IV[q][j-oc*n]+12*oc;if(v<1||v>127)ok=0;else out[i]=(uint8_t)v;}
        }
        if(!ok)continue;
        lo=out[0];
        if(lo<root-6||out[n-1]>root+16)continue;
        m=n<(int)s->npv?n:(int)s->npv;
        for(i=0;i<m;i++){int d=(int)out[i]-(int)s->pv[i];cost+=d<0?-d:d;}
        cost+=4*(n>(int)s->npv?n-(int)s->npv:(int)s->npv-n);
        if(!have||cost<bc){
            have=1;bc=cost;
            for(i=0;i<n;i++)s->cn[i]=out[i];
        }
    }
    s->ncn=have?(uint8_t)n:0;
    for(i=0;i<s->ncn;i++)s->pv[i]=s->cn[i];
    s->npv=s->ncn;
}
static void chord_start(C *c,const Step *st,uint32_t step){
    Seq *s=c->s;const Song *g=c->g;
    int root=(int)st->v[SC_ROOT]+g->transpose;
    uint32_t mode=(uint32_t)st->v[SC_TYPE]>>4,len=st->v[SC_LEN];
    chord_kill(c);
    if(root<1||root>127)return;
    voicing(s,root,st->v[SC_TYPE]&15);
    if(!s->ncn)return;
    s->cvel=vmap(st->v[SC_VEL]);
    tm_add(s,K_COFF,dur(s,g,step,len),0,0);
    if(mode==CM_BLOCK){uint32_t i;for(i=0;i<s->ncn;i++)cs_on(c,s->cn[i]);}
    else if(mode==CM_STRUM){
        cs_on(c,s->cn[0]);s->sm_i=1;
        if(s->ncn>1)tm_add(s,K_STRUM,(int32_t)SQ_STRUM,0,0);
    }else{
        s->ap_fast=mode>=CM_UP32;s->ap_mode=(uint8_t)(mode-(s->ap_fast?4u:0u));
        s->ap_i=0;s->ap_left=(uint16_t)(len*(s->ap_fast?2u:1u));s->ap_on=1;
        arp_go(c);
    }
}
static void vocal_start(C *c,const Step *st,uint32_t step){
    Seq *s=c->s;const Song *g=c->g;
    int n=(int)st->v[SV_NOTE]+g->transpose;
    uint32_t mo=st->v[SV_MORA],fl=st->v[SV_FLAG],authored=fl&240u,aux,i;
    int32_t d=dur(s,g,step,st->v[SV_LEN]);
    if(n<1||n>127)return;
    if(mo>=MORA_N)mo=0;
    if(!authored){const Step *pat=st-step;for(i=0;i<NSTEP;i++)authored|=pat[i].v[SV_FLAG]&240u;}
    aux=mora_vowel(mo)|(fl&VF_SUSTAIN);
    if(authored&&!s->vsl)aux|=SQ_VOCAL_REATTACK;
    if(s->vn&&!s->vsl)em(c,SQ_NOTE_OFF,TR_VOCAL,s->vn,0,0,0);
    tm_cancel(s,K_VOFF);
    em(c,SQ_NOTE_ON,TR_VOCAL,(uint8_t)n,vmap(fl&15u),(uint8_t)aux,(uint8_t)mo);
    s->vn=(uint8_t)n;s->vsl=(fl&VF_LEGATO)&&!(fl&VF_PHRASE_END);
    tm_add(s,K_VOFF,s->vsl?d+2:d*85/100,0,0);
}
static void bass_start(C *c,const Step *st,uint32_t step){
    Seq *s=c->s;const Song *g=c->g;
    int n=(int)st->v[SB_NOTE]+g->transpose;
    uint32_t fl=st->v[SB_FLAG];
    uint8_t aux=(uint8_t)(fl&1u),vel=vmap(st->v[SB_VEL]);
    int32_t d=dur(s,g,step,st->v[SB_LEN]);
    if(n<1||n>127)return;
    if(s->bn){if(s->bsl)aux|=2;else em(c,SQ_NOTE_OFF,TR_BASS,s->bn,0,0,0);}
    tm_cancel(s,K_BOFF);
    if(fl&1u)vel=vel<=111?(uint8_t)(vel+16):127;
    em(c,SQ_NOTE_ON,TR_BASS,(uint8_t)n,vel,aux,0);
    s->bn=(uint8_t)n;s->bsl=(fl&2u)!=0;
    tm_add(s,K_BOFF,s->bsl?d+2:d*85/100,0,0);
}
static void voices_off(C *c){
    Seq *s=c->s;
    if(s->vn)em(c,SQ_NOTE_OFF,TR_VOCAL,s->vn,0,0,0);
    if(s->bn)em(c,SQ_NOTE_OFF,TR_BASS,s->bn,0,0,0);
    s->vn=s->bn=s->bsl=s->vsl=0;
    chord_kill(c);
    tm_cancel(s,K_VOFF);tm_cancel(s,K_BOFF);
}
static void process_step(C *c,uint32_t row,uint32_t step){
    Seq *s=c->s;const Song *g=c->g;
    const Row *r;
    uint32_t t;
    em(c,SQ_STEP,0,(uint8_t)row,0,(uint8_t)step,0);
    if(row>=g->nrows||row>=NROW)return;
    r=&g->row[row];
    for(t=0;t<NTRK;t++){
        uint32_t p=r->pat[t];
        const Step *st=p<NPAT?&g->pat[t][p][step]:0;
        if(!aud(g,(int)t))continue;
        if(t==TR_VOCAL){
            if(st&&st->v[SV_NOTE])vocal_start(c,st,step);
            else if(st&&st->v[SV_LEN]&&(st->v[SV_FLAG]&(VF_WORD_END|VF_PHRASE_END))){
                /* Explicit v4 lyric pause; legacy empty-step metadata is not a release command. */
                if(s->vn)em(c,SQ_NOTE_OFF,TR_VOCAL,s->vn,0,0,0);
                s->vn=s->vsl=0;tm_cancel(s,K_VOFF);
            }
        }
        else if(t==TR_CHORDS){if(st&&st->v[SC_ROOT])chord_start(c,st,step);else arp_go(c);}
        else if(t==TR_BASS){if(st&&st->v[SB_NOTE])bass_start(c,st,step);}
        else if(st){
            uint32_t b;
            for(b=0;b<6;b++)if(st->v[SD_MASK]&(1u<<b))em(c,SQ_DRUM,TR_DRUMS,(uint8_t)b,(st->v[SD_ACC]>>b)&1u?127:96,0,0);
        }
    }
}
static void grid_fire(C *c){
    Seq *s=c->s;const Song *g=c->g;
    uint32_t q,r,row=s->row,step=s->step;
    int32_t sw;
    if(row==0xFFu){voices_off(c);s->state=ST_STOP;return;}
    q=N15/s->bpm;r=N15%s->bpm;
    s->rem+=r;s->slen=q;
    if(s->rem>=s->bpm){s->rem-=s->bpm;s->slen++;}
    s->to_grid=(int32_t)s->slen;
    sw=swf(s,g);
    if((step&1u)&&sw>0)tm_add(s,K_STEP,sw,(uint8_t)row,(uint8_t)step);
    else process_step(c,row,step);
    if(++step==NSTEP){
        step=0;
        if(!s->rowloop){
            row++;
            if(row>=g->nrows)row=g->loop?0u:0xFFu;
        }
    }
    s->row=(uint8_t)row;s->step=(uint8_t)step;
}
static void fire(C *c,SqTm t){
    Seq *s=c->s;
    switch(t.kind){
    case K_VOFF: if(s->vn)em(c,SQ_NOTE_OFF,TR_VOCAL,s->vn,0,0,0);s->vn=s->vsl=0;break;
    case K_BOFF: if(s->bn)em(c,SQ_NOTE_OFF,TR_BASS,s->bn,0,0,0);s->bn=s->bsl=0;break;
    case K_COFF: chord_kill(c);break;
    case K_STEP: process_step(c,t.a,t.b);break;
    case K_STRUM:
        cs_on(c,s->cn[s->sm_i++]);
        if(s->sm_i<s->ncn)tm_add(s,K_STRUM,(int32_t)SQ_STRUM,0,0);
        break;
    case K_ARP: arp_tick(c);break;
    }
}
static void pass(C *c,uint8_t k0,uint8_t k1){
    int i;
    for(i=0;i<SQ_NTM;i++){
        SqTm t=c->s->tm[i];
        if(t.kind>=k0&&t.kind<=k1&&t.due==0){c->s->tm[i].kind=K_NONE;fire(c,t);}
    }
}
void sq_init(Seq *s){
    uint32_t i;
    s->state=ST_STOP;s->rowloop=0;s->row=0;s->step=0;s->fl=0;s->bpm=120;s->ap_left=0;s->rem=0;s->slen=0;s->to_grid=0;
    s->vn=s->bn=s->bsl=s->vsl=s->ncs=s->ncn=s->npv=s->cvel=s->ap_on=s->ap_mode=s->ap_i=s->ap_fast=s->sm_i=0;
    for(i=0;i<8;i++)s->cs[i]=s->cn[i]=s->pv[i]=0;
    for(i=0;i<SQ_NTM;i++)s->tm[i].kind=K_NONE;
}
int sq_playing(const Seq *s){return s->state==ST_PLAY;}
int sq_play(Seq *s,const Song *g,uint32_t row,int row_loop){
    uint32_t i;
    if(row>=g->nrows||g->bpm<40||g->bpm>240)return 0;
    s->fl=(uint8_t)(s->vn||s->bn||s->ncs);    /* restart: release what is still sounding first */
    s->bpm=g->bpm;s->rem=s->bpm/2u;s->slen=N15/s->bpm;s->to_grid=0;
    s->row=(uint8_t)row;s->step=0;s->rowloop=row_loop!=0;s->npv=0;s->ap_on=0;
    for(i=0;i<SQ_NTM;i++)s->tm[i].kind=K_NONE;
    s->state=ST_PLAY;
    return 1;
}
void sq_stop(Seq *s){if(s->state==ST_PLAY)s->state=ST_FLUSH;}
int sq_set_bpm(Seq *s,uint32_t b){
    uint32_t i,o=s->bpm;
    if(b<40||b>240)return 0;
    if(s->state==ST_PLAY){
        s->to_grid=(int32_t)((uint32_t)s->to_grid*o/b);
        for(i=0;i<SQ_NTM;i++)if(s->tm[i].kind)s->tm[i].due=(int32_t)((uint32_t)s->tm[i].due*o/b);
        s->slen=s->slen*o/b;
    }
    s->rem=s->rem*b/o;s->bpm=(uint16_t)b;
    return 1;
}
uint32_t sq_block(Seq *s,const Song *g,uint32_t frames,SqEvent *ev,uint32_t max){
    C c;
    int32_t left;
    uint32_t t;
    int i;
    c.s=s;c.g=g;c.ev=ev;c.max=max;c.n=0;c.pos=0;
    if(s->state==ST_FLUSH){voices_off(&c);s->state=ST_STOP;}
    if(s->state!=ST_PLAY||!frames)return c.n;
    if(s->fl){voices_off(&c);s->fl=0;}
    for(t=0;t<NTRK;t++)if(!aud(g,(int)t)){
        if(t==TR_VOCAL&&s->vn){em(&c,SQ_NOTE_OFF,0,s->vn,0,0,0);s->vn=s->vsl=0;tm_cancel(s,K_VOFF);}
        if(t==TR_BASS&&s->bn){em(&c,SQ_NOTE_OFF,2,s->bn,0,0,0);s->bn=s->bsl=0;tm_cancel(s,K_BOFF);}
        if(t==TR_CHORDS)chord_kill(&c);
    }
    left=(int32_t)frames;
    while(s->state==ST_PLAY){
        int32_t d=s->to_grid;
        for(i=0;i<SQ_NTM;i++)if(s->tm[i].kind&&s->tm[i].due<d)d=s->tm[i].due;
        if(d>=left)d=left;
        s->to_grid-=d;
        for(i=0;i<SQ_NTM;i++)if(s->tm[i].kind)s->tm[i].due-=d;
        left-=d;c.pos+=(uint32_t)d;
        if(!left)break;
        pass(&c,K_VOFF,K_COFF);
        if(s->to_grid==0)grid_fire(&c);
        pass(&c,K_STEP,K_STEP);
        pass(&c,K_STRUM,K_ARP);
    }
    return c.n;
}

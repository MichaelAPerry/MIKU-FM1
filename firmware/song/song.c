/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/song.h"
#include "firmware/song/mora.h"
void sg_init(Song *g){
    static const char nm[8]={'N','E','W',' ','S','O','N','G'};
    static const uint8_t lv[NTRK]={110,80,100,100};
    uint32_t i,t,p,s,k;
    for(i=0;i<8;i++)g->name[i]=nm[i];
    g->bpm=120;g->swing=0;g->master=100;g->loop=1;g->nrows=0;g->transpose=0;
    for(t=0;t<NTRK;t++){
        Trk *k2=&g->trk[t];
        k2->level=lv[t];k2->mute=0;k2->solo=0;k2->preset=0;
        k2->vib=35;k2->porta=10;k2->breath=8;k2->bright=79;
    }
    sg_voice_default(&g->vx);
    for(i=0;i<NROW;i++){for(t=0;t<NTRK;t++)g->row[i].pat[t]=ROW_OFF;g->row[i].sect=SEC_NONE;}
    for(t=0;t<NTRK;t++)for(p=0;p<NPAT;p++)for(s=0;s<NSTEP;s++)for(k=0;k<4;k++)g->pat[t][p][s].v[k]=0;
}
void sg_voice_default(Voice *v){
    static const uint8_t d[16]={2,12,40,63,50,20,0,0,0,0,0,VC_MIKU,VL_JP,0,0,0};   /* = fvoice's 0.5 settings: attack 8 ms, release 60, scoop 40, human 80, 5.5 Hz, 200 ms */
    uint8_t *p=(uint8_t *)v;
    uint32_t i;
    for(i=0;i<16;i++)p[i]=d[i];
}
int sg_step_ok(int trk, const Step *s){
    const uint8_t *v=s->v;
    switch(trk){
    case TR_VOCAL: return v[0]<=127 && v[1]<=64 && (v[0]==0||v[1]>=1) && v[2]<MORA_N;
    case TR_CHORDS: return v[0]<=127 && (v[1]&15)<=CQ_ADD9 && (v[1]>>4)<=CM_UPDOWN32 && v[2]<=64 && (v[0]==0||v[2]>=1) && v[3]<=15;
    case TR_BASS: return v[0]<=127 && v[1]<=64 && (v[0]==0||v[1]>=1) && (v[2]&0xFCu)==0 && v[3]<=15;
    case TR_DRUMS: return (v[0]&0xC0u)==0 && (v[1]&0xC0u)==0 && !v[2] && !v[3];
    }
    return 0;
}
static int pok(int trk,int pat){return trk>=0 && trk<NTRK && pat>=0 && pat<NPAT;}
static int sok(int trk,int pat,int st){return pok(trk,pat) && st>=0 && st<NSTEP;}
static void cp(Step *d,const Step *s){uint32_t k;for(k=0;k<4;k++)d->v[k]=s->v[k];}
static void zs(Step *d){uint32_t k;for(k=0;k<4;k++)d->v[k]=0;}
int sg_set_step(Song *g,int trk,int pat,int step,const Step *s){
    if(!sok(trk,pat,step)||!sg_step_ok(trk,s))return 0;
    cp(&g->pat[trk][pat][step],s);return 1;
}
int sg_clear_step(Song *g,int trk,int pat,int step){
    if(!sok(trk,pat,step))return 0;
    zs(&g->pat[trk][pat][step]);return 1;
}
int sg_copy(const Song *g,int trk,int pat,int from,int n,Step *out){
    int i;
    if(!sok(trk,pat,from)||n<1||from+n>NSTEP||!out)return 0;
    for(i=0;i<n;i++)cp(&out[i],&g->pat[trk][pat][from+i]);
    return 1;
}
int sg_paste(Song *g,int trk,int pat,int at,const Step *in,int n){
    int i;
    if(!sok(trk,pat,at)||n<1||at+n>NSTEP||!in)return 0;
    for(i=0;i<n;i++)if(!sg_step_ok(trk,&in[i]))return 0;
    for(i=0;i<n;i++)cp(&g->pat[trk][pat][at+i],&in[i]);
    return 1;
}
int sg_pat_empty(const Song *g,int trk,int pat){
    int i;
    if(!pok(trk,pat))return 0;
    for(i=0;i<NSTEP;i++)if(g->pat[trk][pat][i].v[0])return 0;
    return 1;
}
int sg_clear_pat(Song *g,int trk,int pat){
    int i;
    if(!pok(trk,pat))return 0;
    for(i=0;i<NSTEP;i++)zs(&g->pat[trk][pat][i]);
    return 1;
}
int sg_find_free(const Song *g,int trk){
    int p,r;
    if(trk<0||trk>=NTRK)return -1;
    for(p=0;p<NPAT;p++){
        int used=0;
        if(!sg_pat_empty(g,trk,p))continue;
        for(r=0;r<NROW;r++)if(g->row[r].pat[trk]==p)used=1;
        if(!used)return p;
    }
    return -1;
}
int sg_dup_pat(Song *g,int trk,int src){
    int d,i;
    if(!pok(trk,src))return -1;
    d=sg_find_free(g,trk);
    if(d<0)return -1;
    for(i=0;i<NSTEP;i++)cp(&g->pat[trk][d][i],&g->pat[trk][src][i]);
    return d;
}
int sg_transpose(Song *g,int trk,int pat,int semis){
    int i;
    if(!pok(trk,pat)||trk==TR_DRUMS||semis<-127||semis>127)return 0;
    for(i=0;i<NSTEP;i++){
        int n=g->pat[trk][pat][i].v[0];
        if(n&&(n+semis<1||n+semis>127))return 0;
    }
    for(i=0;i<NSTEP;i++){
        Step *s=&g->pat[trk][pat][i];
        if(s->v[0])s->v[0]=(uint8_t)(s->v[0]+semis);
    }
    return 1;
}
static void rcp(Row *d,const Row *s){uint32_t k;for(k=0;k<NTRK;k++)d->pat[k]=s->pat[k];d->sect=s->sect;}
static void rblank(Row *d){uint32_t k;for(k=0;k<NTRK;k++)d->pat[k]=ROW_OFF;d->sect=SEC_NONE;}
int sg_row_insert(Song *g,int at){
    int i;
    if(at<0||at>g->nrows||g->nrows>=NROW)return 0;
    for(i=NROW-1;i>at;i--)rcp(&g->row[i],&g->row[i-1]);
    rblank(&g->row[at]);g->nrows++;return 1;
}
int sg_row_delete(Song *g,int at){
    int i;
    if(at<0||at>=g->nrows)return 0;
    for(i=at;i<NROW-1;i++)rcp(&g->row[i],&g->row[i+1]);
    rblank(&g->row[NROW-1]);g->nrows--;return 1;
}
int sg_row_dup(Song *g,int at){
    if(at<0||at>=g->nrows||g->nrows>=NROW)return 0;
    sg_row_insert(g,at+1);
    rcp(&g->row[at+1],&g->row[at]);return 1;
}
int sg_row_set(Song *g,int row,int trk,int pat){
    if(row<0||row>=NROW||trk<0||trk>=NTRK||!((pat>=0&&pat<NPAT)||pat==ROW_OFF))return 0;
    g->row[row].pat[trk]=(uint8_t)pat;return 1;
}
int sg_row_sect(Song *g,int row,int sect){
    if(row<0||row>=NROW||sect<0||sect>=SEC_N)return 0;
    g->row[row].sect=(uint8_t)sect;return 1;
}
/* ---- undo */
void un_init(Undo *u){u->head=0;u->cnt=0;}
static UnEnt *un_new(Undo *u){
    UnEnt *e=&u->e[u->head];
    u->head=(uint8_t)((u->head+1u)%UN_N);
    if(u->cnt<UN_N)u->cnt++;
    return e;
}
int un_push_pat(Undo *u,const Song *g,int trk,int pat){
    UnEnt *e;int i,k;
    if(!pok(trk,pat))return 0;
    e=un_new(u);e->kind=1;e->trk=(uint8_t)trk;e->pat=(uint8_t)pat;
    for(i=0;i<NSTEP;i++)for(k=0;k<4;k++)e->d[i*4+k]=g->pat[trk][pat][i].v[k];
    return 1;
}
int un_push_rows(Undo *u,const Song *g){
    UnEnt *e;int i,k;
    e=un_new(u);e->kind=2;e->nrows=g->nrows;
    for(i=0;i<NROW;i++){for(k=0;k<NTRK;k++)e->d[i*5+k]=g->row[i].pat[k];e->d[i*5+4]=g->row[i].sect;}
    return 1;
}
int un_pop(Undo *u,Song *g){
    UnEnt *e;int i,k;
    if(!u->cnt)return 0;
    u->head=(uint8_t)((u->head+UN_N-1u)%UN_N);u->cnt--;
    e=&u->e[u->head];
    if(e->kind==1){
        for(i=0;i<NSTEP;i++)for(k=0;k<4;k++)g->pat[e->trk][e->pat][i].v[k]=e->d[i*4+k];
    }else{
        g->nrows=e->nrows;
        for(i=0;i<NROW;i++){for(k=0;k<NTRK;k++)g->row[i].pat[k]=e->d[i*5+k];g->row[i].sect=e->d[i*5+4];}
    }
    e->kind=0;return 1;
}

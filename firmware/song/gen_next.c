/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/gen_next.h"
#include "firmware/song/mora.h"
/* No global RNG, libc, allocation or float. Candidate styles are review material, not an approved lineup. */
typedef struct {
    const char *name;
    char tag[3];
    uint8_t bpm, pre[3], degree[4], form[8], bars[8];
    uint16_t kick, clap, hats, bass, vocal, chord;
} GxStyle;
static const GxStyle gx_style[GX_N] = {
    {"HYPERPOP",{'H','Y','P'},164,{5,3,1},{0,5,3,4},
     {SEC_INTRO,SEC_VERSE,SEC_BRIDGE,SEC_CHORUS,SEC_BRIDGE,SEC_CHORUS,SEC_OUTRO,0},{1,4,2,4,2,4,1,0},
     0x2941,0x1010,0xD555,0x6951,0x1555,0x4949},
    {"FUTURE BASS",{'F','U','T'},148,{3,1,0},{5,3,0,4},
     {SEC_INTRO,SEC_VERSE,SEC_BRIDGE,SEC_CHORUS,SEC_VERSE,SEC_BRIDGE,SEC_CHORUS,SEC_OUTRO},{2,4,2,4,2,2,4,2},
     0x0481,0x0100,0x5454,0x2491,0x1145,0x2491},
    {"JERSEY CLUB",{'J','R','S'},140,{4,1,0},{0,0,5,6},
     {SEC_INTRO,SEC_CHORUS,SEC_VERSE,SEC_CHORUS,SEC_BRIDGE,SEC_CHORUS,SEC_OUTRO,0},{1,4,2,4,2,4,1,0},
     0x4949,0x1010,0x4444,0x4949,0x4949,0x0404},
    {"DIGICORE",{'D','I','G'},172,{2,2,1},{0,6,5,3},
     {SEC_INTRO,SEC_VERSE,SEC_CHORUS,SEC_BRIDGE,SEC_CHORUS,SEC_VERSE,SEC_OUTRO,0},{1,3,4,1,4,3,1,0},
     0x2181,0x1010,0xA955,0xA591,0x2565,0xA449},
    {"IDOL POP",{'I','D','L'},132,{1,0,0},{0,4,5,3},
     {SEC_INTRO,SEC_VERSE,SEC_BRIDGE,SEC_CHORUS,SEC_VERSE,SEC_BRIDGE,SEC_CHORUS,SEC_OUTRO},{2,4,2,4,4,2,4,2},
     0x1111,0x1010,0x5555,0x5555,0x1555,0x1515}
};
static const uint8_t gx_scale[2][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10}};
static const uint8_t gx_quality[2][7]={{CQ_MAJ,CQ_MIN,CQ_MIN,CQ_MAJ,CQ_MAJ,CQ_MIN,CQ_DIM},
    {CQ_MIN,CQ_DIM,CQ_MAJ,CQ_MIN,CQ_MIN,CQ_MAJ,CQ_MAJ}};
static uint32_t gx_rand(uint32_t *r) { *r ^= *r<<13; *r ^= *r>>17; *r ^= *r<<5; return *r; }
static uint32_t gx_seed(const GenNextCfg *c,int t,uint32_t key,uint32_t var) {
    uint32_t x=(uint32_t)c->seed*0x9e3779b1u ^ (c->style+1u)*0x85ebca6bu ^ (uint32_t)(t+1)*0xc2b2ae35u ^ key*0x27d4eb2fu ^ var*0x165667b1u;
    return x?x:1u;
}
static int gx_valid(const GenNextCfg *c) { return c && c->style<GX_N && c->key<12 && c->minor<2 && c->lock<16 && c->energy<=100 && c->complexity<=100; }
const char *gen_next_style(uint32_t s) { return s<GX_N?gx_style[s].name:""; }
static int gx_role(const Row *rows,int row) {
    int s=rows[row].sect;
    return s==SEC_CHORUS?2:s==SEC_INTRO||s==SEC_OUTRO||s==SEC_BRIDGE?0:1;
}
static int gx_bar(const Row *rows,int row) { int n=0; while(row>0 && rows[row-1].sect==rows[row].sect){n++;row--;} return n; }
static int gx_degree(const Row *rows,const GenNextCfg *c,int row) {
    uint32_t rotation=(c->seed>>2)&3u;
    int bar=gx_bar(rows,row),d=gx_style[c->style].degree[(bar+rotation)&3];
    if(rows[row].sect==SEC_OUTRO)d=0;
    return d;
}
static void gx_chord(const Song *g,const Row *rows,const GenNextCfg *c,int row,int *root,int *quality) {
    int r=row,i,d=gx_degree(rows,c,row);
    *root=48+(c->key+gx_scale[c->minor][d])%12; *quality=gx_quality[c->minor][d];
    if(c->style==GX_FUTURE_BASS || c->style==GX_IDOL_POP)
        *quality=*quality==CQ_MAJ?CQ_MAJ7:*quality==CQ_MIN?CQ_MIN7:*quality;
    if(c->lock&(1u<<TR_CHORDS)) {
        for(;r>=0;r--) { uint32_t p=rows[r].pat[TR_CHORDS]; if(p>=NPAT)continue;
            for(i=0;i<NSTEP;i++)if(g->pat[TR_CHORDS][p][i].v[0]) {
                *root=g->pat[TR_CHORDS][p][i].v[0];*quality=g->pat[TR_CHORDS][p][i].v[1]&15;return;
            }
        }
    }
}
static uint32_t gx_key(const Song *g,const Row *rows,int nrows,const GenNextCfg *c,int row,int t) {
    int root,q,role=gx_role(rows,row);gx_chord(g,rows,c,row,&root,&q);
    /* Four roots x three energy roles <=12 contexts in native arrangements. Actual locked chord qualities join key. */
    return t==TR_DRUMS?(uint32_t)(role*2+(row+1>=nrows || rows[row+1].sect!=rows[row].sect)):(uint32_t)(role*4096+root*16+q);
}
static void gx_put(Step *pat,int t,int s,int a,int b,int c,int d) {
    Step v={{(uint8_t)a,(uint8_t)b,(uint8_t)c,(uint8_t)d}};
    if(s>=0 && s<NSTEP && sg_step_ok(t,&v))pat[s]=v;
}
static uint16_t gx_mask(uint16_t mask,int role,const GenNextCfg *c) {
    if(role==0)return mask&0x1111u;
    if(role==1 || c->energy<35)return mask&0x5555u;
    return mask;
}
static int gx_next(uint16_t m,int step) { int i;for(i=step+1;i<16;i++)if((m>>i)&1)return i;return 16; }
static void gx_write(Step *pat,const Song *g,const Row *rows,int nrows,const GenNextCfg *c,int row,int t,uint32_t var) {
    const GxStyle *st=&gx_style[c->style];
    int i,root,q,role=gx_role(rows,row),level=role==2?13:11;
    uint32_t rng=gx_seed(c,t,gx_key(g,rows,nrows,c,row,t),var),r=gx_rand(&rng);
    uint16_t m;
    for(i=0;i<16;i++) { int j;for(j=0;j<4;j++)pat[i].v[j]=0; }
    gx_chord(g,rows,c,row,&root,&q);
    if(t==TR_CHORDS) {
        m=gx_mask(st->chord,role,c); if(!m)m=1;
        if(var)m^=(uint16_t)(1u<<(2u+(var%6u)*2u));
        if(role==0) { gx_put(pat,t,0,root,(CM_STRUM<<4)|q,16,8);return; }
        for(i=0;i<16;i++)if((m>>i)&1) {
            int mode=c->style==GX_DIGICORE?CM_UP32:CM_BLOCK;
            int len=c->style==GX_FUTURE_BASS?gx_next(m,i)-i:2;
            gx_put(pat,t,i,root,(mode<<4)|q,len,level);
        }
    } else if(t==TR_BASS) {
        m=gx_mask(st->bass,role,c);if(!m)m=1;
        if(var)m|=(uint16_t)(1u<<((var*3u+5u)&15u));
        for(i=0;i<16;i++)if((m>>i)&1) {
            int off=0,n=24+root%12,len=gx_next(m,i)-i,flag=(i%4==0?BF_ACCENT:0);
            if(c->style==GX_HYPERPOP || c->style==GX_DIGICORE)off=(i&4)?12:0;
            if(c->style==GX_IDOL_POP && (i&6)==6)off=7;
            if(c->style==GX_JERSEY)len=1;
            if(c->style==GX_HYPERPOP && len>2)len=2;
            if(c->style==GX_FUTURE_BASS && (i==7||i==13))flag|=BF_SLIDE;
            gx_put(pat,t,i,n+off,len,flag,level);
        }
    } else if(t==TR_DRUMS) {
        uint16_t kick=gx_mask(st->kick,role,c),clap=role?st->clap:0,hats=gx_mask(st->hats,role,c);
        if(c->complexity>65 && role==2)hats|=(c->style==GX_JERSEY?0xC000u:0xE000u);
        if(c->style==GX_DIGICORE && role==2) { kick&=0x7FFFu;hats^=(uint16_t)(1u<<(r%16u)); }
        if(role && (row+1>=nrows || rows[row+1].sect!=rows[row].sect)) {
            hats|=c->complexity>50?0xF000u:0x5000u;
            if(c->style==GX_HYPERPOP || c->style==GX_DIGICORE){clap|=0x5000u;kick&=0x3FFFu;}
            if(c->style==GX_FUTURE_BASS)kick&=0x0FFFu;
        }
        if(var){kick|=(uint16_t)(1u<<((var*3u+7u)&15u));hats^=(uint16_t)(1u<<((var*5u+3u)&15u));}
        for(i=0;i<16;i++) {
            int hit=0,accent=0;
            if((kick>>i)&1){hit|=D_KICK;if(!(i&3))accent|=D_KICK;}
            if((clap>>i)&1){hit|=D_CLAP;if(c->style!=GX_JERSEY)hit|=D_SNARE;accent|=D_CLAP;}
            if((hats>>i)&1)hit|=D_CHAT;
            if(role==2 && i==14 && c->energy>65)hit|=D_OHAT;
            if(role==2 && c->complexity>50 && i==(int)(r%4u+11u))hit|=D_PERC;
            if(hit)gx_put(pat,t,i,hit,accent,0,0);
        }
    } else {
        static const int8_t motif[GX_N][8]={{0,7,12,7,3,7,10,7},{0,2,7,5,3,2,0,7},{0,0,7,0,3,0,7,0},{0,12,2,10,7,3,12,7},{0,2,4,7,5,4,2,0}};
        static const char *const syllable[8]={"a","ka","mi","ku","ra","na","yo","o"};
        int idx=0,third=q==CQ_MIN||q==CQ_MIN7||q==CQ_DIM?3:4;
        if(role==0)return; /* sparse breakdown/prechorus: instruments carry the transition */
        m=gx_mask(st->vocal,role,c);
        if(c->complexity<30)m&=0x1111u;
        if(var)m|=(uint16_t)(1u<<((var*3u+1u)&15u));
        for(i=0;i<16;i++)if((m>>i)&1) {
            int off=motif[c->style][(idx+(c->seed&3u))&7],len=gx_next(m,i)-i;
            if(off==3||off==4)off=third;
            if(i==0||i==8)off=(idx&1)?7:0;
            if(var && (idx&1))off=(off+(int)(var%3u)*2)%13;
            if(role==2 && c->style!=GX_JERSEY)off+=12;
            if(c->style==GX_JERSEY || c->style==GX_DIGICORE)len=1;
            gx_put(pat,t,i,48+root%12+off,len,(int)mora_find(syllable[(idx+(r&3u))&7]),level);
            idx++;
        }
    }
}
int gen_next_song_ws(Song *g,const GenNextCfg *c,GenNextWorkspace *w) {
    int r,t,i,j;
    if(!g || !gx_valid(c) || !w)return 0;
    /* Reject overlapping caller workspace rather than destroying input during preflight. */
    { uintptr_t ws=(uintptr_t)w,gs=(uintptr_t)g,cs=(uintptr_t)c;
      if((ws<=gs?gs-ws<sizeof *w:ws-gs<sizeof *g) ||
         (ws<=cs?cs-ws<sizeof *w:ws-cs<sizeof *c))return 0; }
    if(c->lock==15)return 1;
    w->nrows=g->nrows;
    if(w->nrows>NROW)return 0;
    for(t=0;t<NTRK;t++)w->counts[t]=0;
    for(r=0;r<w->nrows;r++){
        w->row[r].sect=g->row[r].sect;
        for(t=0;t<NTRK;t++)w->row[r].pat[t]=g->row[r].pat[t];
    }
    if(!c->lock) {
        w->nrows=0;
        for(i=0;i<8;i++)for(j=0;j<gx_style[c->style].bars[i];j++) {
            r=w->nrows++;w->row[r].sect=gx_style[c->style].form[i];
            for(t=0;t<NTRK;t++)w->row[r].pat[t]=ROW_OFF;
        }
    }
    /* Preflight all capacities before touching g. Never replace an unrelated harmony on overflow. */
    for(t=0;t<NTRK;t++)if(!(c->lock>>t&1))for(r=0;r<w->nrows;r++) {
        uint32_t key=gx_key(g,w->row,w->nrows,c,r,t);
        for(i=0;i<w->counts[t] && w->keys[t][i]!=key;i++){}
        if(i==w->counts[t]) { if(i==NPAT)return 0;w->keys[t][w->counts[t]++]=key; }
        w->index[t][r]=(uint8_t)i;
    }
    if(!c->lock) {
        uint32_t seed=c->seed%10000u;
        g->nrows=w->nrows;
        for(r=0;r<g->nrows;r++){
            g->row[r].sect=w->row[r].sect;
            for(t=0;t<NTRK;t++)g->row[r].pat[t]=w->row[r].pat[t];
        }
        for(i=0;i<3;i++)g->name[i]=gx_style[c->style].tag[i];
        g->name[3]=' ';
        for(i=7;i>=4;i--){g->name[i]=(char)('0'+seed%10u);seed/=10u;}
        g->bpm=gx_style[c->style].bpm;g->swing=0;g->loop=1;g->transpose=0;
        g->master=80; /* candidate mix headroom; locked-arrangement edits keep the owner mix */
    }
    for(t=0;t<NTRK;t++)if(!(c->lock>>t&1)) {
        uint32_t seen=0;
        if(t)g->trk[t].preset=gx_style[c->style].pre[t-1];
        for(i=0;i<NPAT;i++)sg_clear_pat(g,t,i);
        for(r=0;r<g->nrows;r++) {
            i=w->index[t][r];g->row[r].pat[t]=(uint8_t)i;
            if(!(seen>>i&1)){gx_write(g->pat[t][i],g,w->row,w->nrows,c,r,t,0);seen|=1u<<i;}
            if(t==TR_VOCAL && gx_role(w->row,r)==0)g->row[r].pat[t]=ROW_OFF;
        }
    }
    return 1;
}
#ifndef GEN_NEXT_NO_STACK_WRAPPER
int gen_next_song(Song *g,const GenNextCfg *c){GenNextWorkspace w;return gen_next_song_ws(g,c,&w);}
#endif
int gen_next_phrase(Song *g,const GenNextCfg *c,int row,int t,uint32_t var) {
    Step pat[NSTEP];uint32_t p;
    if(!g || !gx_valid(c) || row<0 || row>=g->nrows || t<0 || t>=NTRK || (c->lock>>t&1))return 0;
    p=g->row[row].pat[t];if(p>=NPAT)return 0;
    gx_write(pat,g,g->row,g->nrows,c,row,t,var);
    for(int i=0;i<NSTEP;i++)g->pat[t][p][i]=pat[i];
    return 1;
}

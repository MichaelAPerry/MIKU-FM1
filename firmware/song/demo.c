/* SPDX-License-Identifier: GPL-3.0-only */
/* 「おとのはこ」 (assets/songs/otonohako.md): 148 BPM, C major, 12 rows. */
#include "firmware/song/song.h"
#include "firmware/song/mora.h"
static void put(Song *g,int t,int p,int s,uint8_t a,uint8_t b,uint8_t c,uint8_t d){
    Step x;
    x.v[0]=a;x.v[1]=b;x.v[2]=c;x.v[3]=d;
    sg_set_step(g,t,p,s,&x);
}
/* one vocal bar: space-separated romaji, eighth notes from step 0, the last mora of a 7-mora line fills the bar */
static void line(Song *g,int p,const char *m,const uint8_t *n){
    int i=0,s=0;
    while(*m){
        char w[4];
        int k=0;
        while(*m&&*m!=' '&&k<3)w[k++]=*m++;
        w[k]=0;
        while(*m==' ')m++;
        put(g,TR_VOCAL,p,s,n[i],(uint8_t)(!*m&&s==12?4:2),(uint8_t)mora_find(w),12);
        i++;s+=2;
    }
}
static void chord(Song *g,int p,int root,int q,int mode,int s,int len){
    put(g,TR_CHORDS,p,s,(uint8_t)root,(uint8_t)(mode<<4|q),(uint8_t)len,11);
}
static void bass(Song *g,int p,int r,int slide){
    static const uint8_t o[8]={0,0,12,0,0,0,12,0};
    int i;
    for(i=0;i<8;i++)put(g,TR_BASS,p,i*2,(uint8_t)(r+o[i]),2,(uint8_t)((i%4==0?BF_ACCENT:0)|(slide&&i==7?BF_SLIDE:0)),12);
}
static void drums(Song *g,int p,int kick,int snare,int clap,int hat,int ohat){   /* bit masks over the 16 steps */
    int s;
    for(s=0;s<16;s++){
        int b=1<<s,m=0,a=0;
        if(kick&b)m|=D_KICK;
        if(snare&b)m|=D_SNARE;
        if(clap&b)m|=D_CLAP;
        if(hat&b)m|=D_CHAT;
        if(ohat&b)m|=D_OHAT;
        if(snare&b)a|=D_SNARE;
        if(s%8==0&&(kick&b))a|=D_KICK;
        put(g,TR_DRUMS,p,s,(uint8_t)m,(uint8_t)a,0,0);
    }
}
static void row(Song *g,int r,int sect,int v,int c,int b,int d){
    int *x[4];
    int i;
    x[0]=&v;x[1]=&c;x[2]=&b;x[3]=&d;
    sg_row_insert(g,r);
    for(i=0;i<4;i++)sg_row_set(g,r,i,*x[i]);
    sg_row_sect(g,r,sect);
}
void song_demo(Song *g){
    static const uint8_t nt[8][8]={
        {67,67,69,72,69,67,64,67},{69,69,72,74,76,74,72},{72,72,74,76,74,72,69},{67,69,72,69,67,64,67},
        {76,76,79,76,76,79,81},{79,79,77,76,74,76,72},{76,76,77,79,81,79,76},{74,76,77,76,74,71,72}};
    static const char *const ly[8]={"o to no ha ko no na ka","wa ta shi wa ma tsu yo","ki mi ga ka ku u ta","ko e ni su ru ka ra",
        "u ta e u ta e yo","ki mi no u ta na ra","so ra ma de to do ke","e ga o ga hi ka ru"};
    static const char nm[8]={'O','T','O','N','O','H','A','K'};
    static const uint8_t cr[8]={48,53,57,55,53,55,52,48};   /* C F Am G | F G Em | C */
    static const uint8_t cq[8]={CQ_MAJ,CQ_MAJ,CQ_MIN,CQ_MAJ,CQ_MAJ,CQ_MAJ,CQ_MIN,CQ_MAJ};
    static const uint8_t br[5]={36,41,45,43,40};            /* C F A G E */
    int i;
    sg_init(g);
    for(i=0;i<8;i++)g->name[i]=nm[i];
    g->bpm=148;g->loop=1;
    g->trk[TR_VOCAL].preset=0;g->trk[TR_CHORDS].preset=1;g->trk[TR_BASS].preset=2;g->trk[TR_DRUMS].preset=3;
    for(i=0;i<8;i++)line(g,i,ly[i],nt[i]);
    /* chords: 0-3 verse C F Am G arp up-down 16ths; 4-6 chorus F G Em stab + arp; 7 final C strum; 8 intro C arp up 32nds */
    for(i=0;i<4;i++)chord(g,i,cr[i],cq[i],CM_UPDOWN,0,16);
    for(i=0;i<3;i++){
        int p=4+i,r=cr[4+i],q=cq[4+i];
        chord(g,p,r,q,CM_BLOCK,0,2);chord(g,p,r,q,CM_UPDOWN,2,6);chord(g,p,r,q,CM_BLOCK,8,2);chord(g,p,r,q,CM_UP,10,6);
    }
    chord(g,7,48,CQ_MAJ,CM_STRUM,0,16);
    chord(g,8,48,CQ_MAJ,CM_UP32,0,16);
    /* bass: 0-4 C F A G E; 5 = G sliding into the chorus; 6 = C hold */
    for(i=0;i<5;i++)bass(g,i,br[i],0);
    bass(g,5,br[3],1);
    put(g,TR_BASS,6,0,36,16,BF_ACCENT,13);
    /* drums: 0 verse, 1 verse fill, 2 chorus, 3 intro, 4 soft outro, 5 end */
    drums(g,0,0x1111,0x1010,0,0x5555,0);
    drums(g,1,0x0111,0x9010|0x2000|0x4000,0,0x0555,0);
    drums(g,2,0x1111,0x1010,0x1010,0xFFFF&~0x4444,0x4444);
    drums(g,3,0x1111,0,0x1010,0x5555,0);
    drums(g,4,0x0101,0,0,0x1010,0);
    drums(g,5,0x0001,0,0,0,0x0001);
    row(g,0,SEC_INTRO,ROW_OFF,8,0,3);row(g,1,SEC_INTRO,ROW_OFF,3,3,3);
    row(g,2,SEC_VERSE,0,0,0,0);row(g,3,SEC_VERSE,1,1,1,0);row(g,4,SEC_VERSE,2,2,2,0);row(g,5,SEC_VERSE,3,3,5,1);
    row(g,6,SEC_CHORUS,4,4,1,2);row(g,7,SEC_CHORUS,5,5,3,2);row(g,8,SEC_CHORUS,6,6,4,2);row(g,9,SEC_CHORUS,7,5,3,2);
    row(g,10,SEC_OUTRO,ROW_OFF,0,0,4);row(g,11,SEC_OUTRO,ROW_OFF,7,6,5);
}

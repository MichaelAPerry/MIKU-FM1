/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "firmware/song/song.h"
#include "firmware/song/seq.h"
#include "firmware/song/mora.h"
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#c);fails++;}}while(0)
static Song A,B;
static uint8_t buf[SONG_ENC_SIZE+16];
static uint32_t rs=12345;
static uint32_t rnd(void){rs=rs*1664525u+1013904223u;return rs>>8;}
static void fill_random(Song *g){
    int t,p,s,i;
    sg_init(g);
    for(i=0;i<8;i++){const char *cs="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";g->name[i]=cs[rnd()%37];}
    g->bpm=(uint8_t)(40+rnd()%201);g->swing=(uint8_t)(rnd()%76);g->master=(uint8_t)(rnd()%128);g->loop=rnd()&1;
    g->nrows=(uint8_t)(rnd()%(NROW+1));g->transpose=(int8_t)((int)(rnd()%49)-24);
    for(t=0;t<NTRK;t++){Trk *k=&g->trk[t];k->level=rnd()%128;k->mute=rnd()&1;k->solo=rnd()&1;k->preset=rnd()%8;k->vib=rnd()%101;k->porta=rnd()%101;k->breath=rnd()%101;k->bright=rnd()%101;}
    for(i=0;i<NROW;i++){for(t=0;t<NTRK;t++)g->row[i].pat[t]=(rnd()%5==0)?ROW_OFF:(uint8_t)(rnd()%NPAT);g->row[i].sect=rnd()%SEC_N;}
    for(t=0;t<NTRK;t++)for(p=0;p<NPAT;p++)for(s=0;s<NSTEP;s++){
        Step x;int k;
        do{for(k=0;k<4;k++)x.v[k]=(uint8_t)rnd();
            if(t==TR_VOCAL){x.v[0]%=128;x.v[1]%=65;x.v[2]%=MORA_N;}
            if(t==TR_CHORDS){x.v[0]%=128;x.v[1]=(uint8_t)((rnd()%8)<<4|rnd()%10);x.v[2]%=65;x.v[3]%=16;}
            if(t==TR_BASS){x.v[0]%=128;x.v[1]%=65;x.v[2]&=3;x.v[3]%=16;}
            if(t==TR_DRUMS){x.v[0]&=63;x.v[1]&=63;x.v[2]=x.v[3]=0;}
        }while(!sg_step_ok(t,&x));
        g->pat[t][p][s]=x;
    }
}
static void test_io(void){
    uint32_t n,i;
    int r;
    Song sent;
    song_demo(&A);
    n=song_encode(&A,buf,sizeof buf);
    printf("MKS2 encoded size: %u bytes (slot 16384)\n",n);
    CHECK(n==SONG_ENC_SIZE&&n<16384);
    CHECK(song_encode(&A,buf,n-1)==0);
    memset(&B,0x55,sizeof B);
    CHECK(song_decode(&B,buf,n)==SIO_OK);
    CHECK(!memcmp(&A,&B,sizeof A));
    for(r=0;r<20;r++){
        fill_random(&A);
        n=song_encode(&A,buf,sizeof buf);
        memset(&B,0xA5,sizeof B);
        CHECK(song_decode(&B,buf,n)==SIO_OK&&!memcmp(&A,&B,sizeof A));
    }
    song_demo(&A);
    n=song_encode(&A,buf,sizeof buf);
    memset(&sent,0x77,sizeof sent);
    for(i=0;i<n;i++){
        int e;
        uint8_t o=buf[i];
        B=sent;
        buf[i]^=(uint8_t)(1u<<(i&7));
        e=song_decode(&B,buf,n);
        CHECK(e!=SIO_OK&&!memcmp(&B,&sent,sizeof B));
        buf[i]=(uint8_t)(o^0xFF);
        e=song_decode(&B,buf,n);
        CHECK(e!=SIO_OK&&!memcmp(&B,&sent,sizeof B));
        buf[i]=o;
    }
    for(i=0;i<n;i++){B=sent;CHECK(song_decode(&B,buf,i)!=SIO_OK&&!memcmp(&B,&sent,sizeof B));}
    B=sent;CHECK(song_decode(&B,buf,n+1)!=SIO_OK);
    /* a field out of bounds with a valid CRC is refused too */
    buf[10]='a';{uint32_t c=sg_crc32(buf,n-4);buf[n-4]=(uint8_t)c;buf[n-3]=(uint8_t)(c>>8);buf[n-2]=(uint8_t)(c>>16);buf[n-1]=(uint8_t)(c>>24);}
    B=sent;CHECK(song_decode(&B,buf,n)==SIO_FIELD&&!memcmp(&B,&sent,sizeof B));
}
/* ---- sequencer helpers */
typedef struct { uint32_t t; SqEvent e; } Ev;
static Ev evs[200000];
static uint32_t nev;
static uint64_t now;
static void run(Seq *s,const Song *g,uint32_t total,uint32_t blk0){
    uint32_t done=0,k=0;
    while(done<total){
        SqEvent b[SQ_EVMAX];
        uint32_t f=blk0+(k++%5)*37u,n,i;
        if(f>total-done)f=total-done;
        n=sq_block(s,g,f,b,SQ_EVMAX);
        for(i=0;i<n;i++){CHECK(b[i].at<f);if(nev<200000){evs[nev].t=(uint32_t)now+b[i].at;evs[nev++].e=b[i];}}
        now+=f;done+=f;
    }
}
static double sf(uint32_t bpm){return 44100.0*60.0/(bpm*4.0);}
static void timing(uint32_t bpm,uint32_t swing){
    Seq s;Song g;
    uint32_t i,steps=0,tot;
    song_demo(&g);g.bpm=(uint8_t)bpm;g.swing=(uint8_t)swing;
    sq_init(&s);nev=0;now=0;
    CHECK(sq_play(&s,&g,0,0));
    tot=(uint32_t)(sf(bpm)*16*64-20);
    run(&s,&g,tot,256);
    for(i=0;i<nev;i++)if(evs[i].e.type==SQ_STEP){
        double ex=floor(steps*sf(bpm)+0.5)+((steps&1)?floor(floor(sf(bpm))*swing/100.0):0);
        double d=fabs((double)evs[i].t-ex);
        if(d>(swing?2.0:1.0)){printf("bpm %u step %u at %u expected %.0f\n",bpm,steps,evs[i].t,ex);fails++;break;}
        CHECK(evs[i].e.note==(steps/16)%12&&evs[i].e.aux==steps%16);
        steps++;
    }
    CHECK(steps==1024);
}
typedef struct { int vn,bn,cs[128],ok; } M;
static void matchm(M *m,const Ev *e,int n){
    int i;
    for(i=0;i<n;i++){
        const SqEvent *x=&e[i].e;
        if(x->type==SQ_NOTE_ON){
            if(x->trk==TR_VOCAL){if(m->vn)m->ok=0;m->vn=x->note;}
            else if(x->trk==TR_BASS){if(m->bn&&!(x->aux&2))m->ok=0;m->bn=x->note;}
            else m->cs[x->note]++;
        }else if(x->type==SQ_NOTE_OFF){
            if(x->trk==TR_VOCAL){if(m->vn!=x->note)m->ok=0;m->vn=0;}
            else if(x->trk==TR_BASS){if(m->bn!=x->note)m->ok=0;m->bn=0;}
            else{if(m->cs[x->note]<=0)m->ok=0;else m->cs[x->note]--;}
        }
    }
}
static int msum(const M *m){int i,s=m->vn+m->bn;for(i=0;i<128;i++)s+=m->cs[i];return s;}
static void test_seq(void){
    Seq s;Song g;
    M m;
    uint32_t i,n,last;
    int before;
    timing(148,0);timing(40,0);timing(240,0);timing(148,50);timing(100,75);
    /* bpm change: intervals follow the new tempo, the straddling one lies between */
    song_demo(&g);g.bpm=100;sq_init(&s);nev=0;now=0;sq_play(&s,&g,0,0);
    run(&s,&g,(uint32_t)(sf(100)*8.3),256);
    CHECK(sq_set_bpm(&s,200));CHECK(!sq_set_bpm(&s,39));CHECK(!sq_set_bpm(&s,241));
    run(&s,&g,(uint32_t)(sf(200)*40),256);
    {
        uint32_t prev=0,k=0,cross=0;
        for(i=0;i<nev;i++)if(evs[i].e.type==SQ_STEP){
            if(k){
                double d=evs[i].t-prev;
                if(evs[i].t<(uint32_t)(sf(100)*8.3)+1){CHECK(fabs(d-sf(100))<=1.5);}
                else if(!cross){cross=1;CHECK(d>=sf(200)-1.5&&d<=sf(100)+1.5);}
                else CHECK(fabs(d-sf(200))<=1.5);
            }
            prev=evs[i].t;k++;
        }
        CHECK(cross&&k>40);
    }
    /* every on has an off; stop flushes */
    song_demo(&g);sq_init(&s);nev=0;now=0;sq_play(&s,&g,0,0);
    run(&s,&g,(uint32_t)(sf(148)*16*3+1000),512);
    memset(&m,0,sizeof m);m.ok=1;matchm(&m,evs,(int)nev);
    CHECK(m.ok&&msum(&m)>0);
    before=(int)nev;
    sq_stop(&s);run(&s,&g,2000,512);
    matchm(&m,evs+before,(int)nev-before);
    CHECK(m.ok&&msum(&m)==0);
    CHECK(nev>(uint32_t)before&&evs[before].t==(uint32_t)now-2000);
    n=nev;run(&s,&g,5000,512);CHECK(nev==n&&!sq_playing(&s));
    /* non-looping song ends by itself with everything off */
    g.loop=0;sq_init(&s);nev=0;now=0;sq_play(&s,&g,0,0);
    run(&s,&g,(uint32_t)(sf(148)*16*12+9000),700);
    memset(&m,0,sizeof m);m.ok=1;matchm(&m,evs,(int)nev);
    CHECK(m.ok&&msum(&m)==0&&!sq_playing(&s));
    last=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_STEP)last++;
    CHECK(last==192);
    /* full-length matching over looped play, incl. slides and restart */
    g.loop=1;sq_init(&s);nev=0;now=0;sq_play(&s,&g,0,0);
    run(&s,&g,(uint32_t)(sf(148)*16*40),1000);
    sq_play(&s,&g,6,1);run(&s,&g,(uint32_t)(sf(148)*16*5),1000);
    {int rows_ok=1;uint32_t k;for(k=nev;k>0&&k+80>nev;k--)if(evs[k-1].e.type==SQ_STEP&&evs[k-1].e.note!=6)rows_ok=0;CHECK(rows_ok);}
    sq_stop(&s);run(&s,&g,100,100);
    memset(&m,0,sizeof m);m.ok=1;matchm(&m,evs,(int)nev);
    CHECK(m.ok&&msum(&m)==0);
    /* slide: the verse's last bass note glides into the chorus (no off, aux bit 1) */
    {int sl=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON&&evs[i].e.trk==TR_BASS&&(evs[i].e.aux&2))sl++;CHECK(sl>=1);}
    /* mute / solo */
    song_demo(&g);g.trk[TR_VOCAL].mute=1;sq_init(&s);nev=0;now=0;sq_play(&s,&g,2,0);
    run(&s,&g,(uint32_t)(sf(148)*16*4),1000);
    {int v=0,o=0;for(i=0;i<nev;i++){if(evs[i].e.type==SQ_NOTE_ON&&evs[i].e.trk==TR_VOCAL)v++;if(evs[i].e.type==SQ_NOTE_ON||evs[i].e.type==SQ_DRUM)o++;}CHECK(!v&&o>20);}
    g.trk[TR_VOCAL].mute=0;g.trk[TR_BASS].solo=1;sq_init(&s);nev=0;now=0;sq_play(&s,&g,2,0);
    run(&s,&g,(uint32_t)(sf(148)*16*4),1000);
    {int b=0,o=0;for(i=0;i<nev;i++){if(evs[i].e.type==SQ_NOTE_ON&&evs[i].e.trk==TR_BASS)b++;else if(evs[i].e.type==SQ_NOTE_ON||evs[i].e.type==SQ_DRUM)o++;}CHECK(b>20&&!o);}
    /* muting mid-play releases the sounding notes */
    g.trk[TR_BASS].solo=0;sq_init(&s);nev=0;now=0;sq_play(&s,&g,2,0);
    run(&s,&g,(uint32_t)(sf(148)*20),500);
    g.trk[TR_VOCAL].mute=1;g.trk[TR_CHORDS].mute=1;g.trk[TR_BASS].mute=1;
    run(&s,&g,500,500);
    memset(&m,0,sizeof m);m.ok=1;matchm(&m,evs,(int)nev);CHECK(m.ok&&msum(&m)==0);
}
static void chordsong(Song *g,int root,int q,int mode,int len){
    sg_init(g);g->bpm=120;
    {Step x={{(uint8_t)root,(uint8_t)(mode<<4|q),(uint8_t)len,12}};CHECK(sg_set_step(g,TR_CHORDS,0,0,&x));}
    sg_row_insert(g,0);sg_row_set(g,0,TR_CHORDS,0);
}
static uint32_t collect(const Song *g,uint32_t frames){
    Seq s;sq_init(&s);nev=0;now=0;sq_play(&s,g,0,0);run(&s,g,frames,frames);
    return nev;
}
static void test_chords(void){
    Song g;Seq s;
    uint32_t i,k;
    static const int iv[10][4]={{0,4,7,-1},{0,3,7,-1},{0,4,7,10},{0,4,7,11},{0,3,7,10},{0,5,7,-1},{0,2,7,-1},{0,3,6,-1},{0,4,8,-1},{0,4,7,14}};
    int q;
    chordsong(&g,60,CQ_MAJ,CM_BLOCK,4);collect(&g,3000);
    {int on[4],n=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON){CHECK(evs[i].t==0);on[n++]=evs[i].e.note;}
     CHECK(n==3&&on[0]==60&&on[1]==64&&on[2]==67);}
    for(q=0;q<10;q++){
        int n=0,on[8];
        chordsong(&g,60,q,CM_BLOCK,1);collect(&g,300);
        for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON)on[n++]=evs[i].e.note;
        for(k=0;k<4;k++)if(iv[q][k]>=0){CHECK((int)k<n&&on[k]==60+iv[q][k]);}
    }
    chordsong(&g,60,CQ_MAJ,CM_UP,4);collect(&g,(uint32_t)(sf(120)*4-5));
    {int on[8],n=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON)on[n++]=evs[i].e.note;
     CHECK(n==4&&on[0]==60&&on[1]==64&&on[2]==67&&on[3]==60);}
    chordsong(&g,60,CQ_MAJ,CM_DOWN,4);collect(&g,(uint32_t)(sf(120)*4-5));
    {int on[8],n=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON)on[n++]=evs[i].e.note;
     CHECK(n==4&&on[0]==67&&on[1]==64&&on[2]==60&&on[3]==67);}
    chordsong(&g,60,CQ_MAJ,CM_UPDOWN,8);collect(&g,(uint32_t)(sf(120)*8-5));
    {static const int ex[8]={60,64,67,64,60,64,67,64};int on[16],n=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON)on[n++]=evs[i].e.note;
     CHECK(n==8);for(k=0;k<8&&k<(uint32_t)n;k++)CHECK(on[k]==ex[k]);}
    chordsong(&g,60,CQ_MAJ,CM_UP32,2);collect(&g,(uint32_t)(sf(120)*2-5));
    {int n=0;uint32_t t0=0,t1=0;for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON){if(n==0)t0=evs[i].t;if(n==1)t1=evs[i].t;n++;}
     CHECK(n==4&&fabs((double)(t1-t0)-sf(120)/2)<=2);}
    chordsong(&g,60,CQ_MAJ,CM_STRUM,4);collect(&g,3000);
    {int n=0;uint32_t tt[4]={0};for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON){CHECK(evs[i].e.note==60+(n==0?0:n==1?4:7));tt[n++]=evs[i].t;}
     CHECK(n==3&&tt[1]==SQ_STRUM&&tt[2]==2*SQ_STRUM);}
    /* chord off at len, transposed by the song */
    chordsong(&g,60,CQ_MAJ,CM_BLOCK,4);g.transpose=2;collect(&g,(uint32_t)(sf(120)*4+10));
    {int on=0,off=0;uint32_t t=0;for(i=0;i<nev;i++){if(evs[i].e.type==SQ_NOTE_ON){on++;CHECK(evs[i].e.note>=62);}if(evs[i].e.type==SQ_NOTE_OFF){off++;t=evs[i].t;}}
     CHECK(on==3&&off==3&&fabs((double)t-sf(120)*4)<=2);}
    /* voice leading: C then F stays close (F A C, not F A C an octave up) */
    sg_init(&g);g.bpm=120;
    {Step a={{60,0,16,12}},b={{65,0,16,12}};sg_set_step(&g,TR_CHORDS,0,0,&a);sg_set_step(&g,TR_CHORDS,1,0,&b);}
    sg_row_insert(&g,0);sg_row_insert(&g,1);sg_row_set(&g,0,TR_CHORDS,0);sg_row_set(&g,1,TR_CHORDS,1);
    collect(&g,(uint32_t)(sf(120)*32));
    {int n=0,on[8];for(i=0;i<nev;i++)if(evs[i].e.type==SQ_NOTE_ON)on[n++]=evs[i].e.note;
     CHECK(n==6&&on[3]==60&&on[4]==65&&on[5]==69);
     for(k=3;k<6&&k<(uint32_t)n;k++)CHECK(on[k]-on[k-3]<=6);}
    (void)s;
}
static void test_demo(void){
    static const uint8_t nt[8][8]={{67,67,69,72,69,67,64,67},{69,69,72,74,76,74,72},{72,72,74,76,74,72,69},{67,69,72,69,67,64,67},
        {76,76,79,76,76,79,81},{79,79,77,76,74,76,72},{76,76,77,79,81,79,76},{74,76,77,76,74,71,72}};
    static const char *const vw[8]={"otonohanaka","","","","","","",""};
    static const int nm[8]={8,7,7,7,7,7,7,7};
    static const char v[8][9]={"oooaooaa","aaiaauo","iiaauua","oeiuuaa","uaeuaeo","iiouaaa","oaaeooe","eaoaiau"};
    int r,i,k;
    Song g;
    (void)vw;
    song_demo(&g);
    CHECK(g.nrows==12&&g.bpm==148&&g.loop==1);
    CHECK(g.row[0].sect==SEC_INTRO&&g.row[2].sect==SEC_VERSE&&g.row[6].sect==SEC_CHORUS&&g.row[10].sect==SEC_OUTRO);
    {int ln=0;
     for(r=0;r<12;r++){
        uint8_t p=g.row[r].pat[TR_VOCAL];
        if(p==ROW_OFF){CHECK(r<2||r>=10);continue;}
        CHECK(p==ln);
        for(i=0,k=0;i<NSTEP;i++){
            const Step *s=&g.pat[TR_VOCAL][p][i];
            if(!s->v[SV_NOTE])continue;
            CHECK(s->v[SV_NOTE]==nt[ln][k]&&i==2*k);
            CHECK(s->v[SV_LEN]==((k==nm[ln]-1&&nm[ln]==7)?4:2));
            CHECK(mora_vowel(s->v[SV_MORA])==(uint32_t)(strchr("aiueo",v[ln][k])-"aiueo"));
            k++;
        }
        CHECK(k==nm[ln]);
        ln++;
     }
     CHECK(ln==8);}
    CHECK(song_encode(&g,buf,sizeof buf)==SONG_ENC_SIZE&&song_decode(&B,buf,SONG_ENC_SIZE)==SIO_OK&&!memcmp(&g,&B,sizeof g));
    /* the song plays: 12 rows, then loops */
    {Seq s;uint32_t steps=0;int voc=0,drm=0;
     sq_init(&s);nev=0;now=0;sq_play(&s,&g,0,0);run(&s,&g,(uint32_t)(sf(148)*16*13),1000);
     for(i=0;i<(int)nev;i++){if(evs[i].e.type==SQ_STEP)steps++;if(evs[i].e.type==SQ_NOTE_ON&&evs[i].e.trk==TR_VOCAL)voc++;if(evs[i].e.type==SQ_DRUM)drm++;}
     CHECK(steps==208&&voc==57&&drm>100);
     (void)voc;}
    /* mora table */
    CHECK(mora_find("ka")==6&&mora_find("KA")==6&&mora_find("n")==46&&mora_find("zzz")==0&&mora_find("")==0&&mora_find("kaa")==0);
    CHECK(mora_vowel(mora_find("shi"))==MV_I&&mora_cons(mora_find("shi"))==MC_SH&&mora_vowel(46)==MV_N&&mora_cons(mora_find("tsu"))==MC_TS);
    for(i=1;i<MORA_N;i++)CHECK(mora_find(mora_str((uint32_t)i))==(uint32_t)i);
}
static void test_edit(void){
    Song g;Undo u;Step x={{60,4,0,9}},buf2[4];
    int p,i;
    sg_init(&g);
    CHECK(sg_set_step(&g,TR_VOCAL,0,3,&x));
    CHECK(!sg_set_step(&g,TR_VOCAL,16,3,&x)&&!sg_set_step(&g,TR_VOCAL,0,16,&x)&&!sg_set_step(&g,4,0,0,&x));
    x.v[1]=65;CHECK(!sg_set_step(&g,TR_VOCAL,0,4,&x));x.v[1]=0;CHECK(!sg_set_step(&g,TR_VOCAL,0,4,&x));
    x.v[0]=128;x.v[1]=4;CHECK(!sg_set_step(&g,TR_VOCAL,0,4,&x));
    x.v[0]=60;x.v[2]=MORA_N;CHECK(!sg_set_step(&g,TR_VOCAL,0,4,&x));
    CHECK(g.pat[TR_VOCAL][0][3].v[0]==60);
    x.v[2]=0;
    CHECK(sg_copy(&g,TR_VOCAL,0,2,4,buf2)&&buf2[1].v[0]==60&&!sg_copy(&g,TR_VOCAL,0,14,4,buf2));
    CHECK(sg_paste(&g,TR_VOCAL,1,10,buf2,4)&&g.pat[TR_VOCAL][1][11].v[0]==60&&!sg_paste(&g,TR_VOCAL,1,14,buf2,4));
    CHECK(sg_clear_step(&g,TR_VOCAL,1,11)&&g.pat[TR_VOCAL][1][11].v[0]==0);
    CHECK(sg_transpose(&g,TR_VOCAL,0,12)&&g.pat[TR_VOCAL][0][3].v[0]==72&&!sg_transpose(&g,TR_VOCAL,0,60)&&g.pat[TR_VOCAL][0][3].v[0]==72);
    CHECK(!sg_transpose(&g,TR_DRUMS,0,1));
    CHECK(sg_find_free(&g,TR_VOCAL)==1);
    p=sg_dup_pat(&g,TR_VOCAL,0);CHECK(p==1&&g.pat[TR_VOCAL][1][3].v[0]==72);
    CHECK(sg_find_free(&g,TR_VOCAL)==2);
    sg_row_insert(&g,0);sg_row_set(&g,0,TR_VOCAL,2);
    CHECK(sg_find_free(&g,TR_VOCAL)==3);                 /* 2 is empty but used by a row */
    for(i=0;i<NPAT;i++)sg_dup_pat(&g,TR_VOCAL,0);
    CHECK(sg_dup_pat(&g,TR_VOCAL,0)==-1);
    CHECK(sg_clear_pat(&g,TR_VOCAL,2)&&sg_pat_empty(&g,TR_VOCAL,2));
    /* rows */
    sg_init(&g);
    CHECK(sg_row_insert(&g,0)&&sg_row_insert(&g,1)&&!sg_row_insert(&g,3)&&g.nrows==2);
    sg_row_set(&g,0,TR_BASS,5);sg_row_sect(&g,0,SEC_VERSE);
    CHECK(sg_row_dup(&g,0)&&g.nrows==3&&g.row[1].pat[TR_BASS]==5&&g.row[1].sect==SEC_VERSE&&g.row[2].pat[TR_BASS]==ROW_OFF);
    CHECK(sg_row_delete(&g,0)&&g.nrows==2&&g.row[0].pat[TR_BASS]==5&&!sg_row_delete(&g,2));
    CHECK(!sg_row_set(&g,0,TR_BASS,16)&&!sg_row_sect(&g,0,SEC_N)&&sg_row_set(&g,0,TR_BASS,ROW_OFF));
    while(g.nrows<NROW)sg_row_insert(&g,0);
    CHECK(!sg_row_insert(&g,0)&&!sg_row_dup(&g,0));
    /* undo */
    sg_init(&g);un_init(&u);
    x.v[0]=60;x.v[1]=4;x.v[2]=0;x.v[3]=9;
    sg_set_step(&g,TR_VOCAL,5,0,&x);
    CHECK(un_push_pat(&u,&g,TR_VOCAL,5));
    sg_clear_pat(&g,TR_VOCAL,5);CHECK(g.pat[TR_VOCAL][5][0].v[0]==0);
    CHECK(un_pop(&u,&g)&&g.pat[TR_VOCAL][5][0].v[0]==60&&!un_pop(&u,&g));
    sg_row_insert(&g,0);sg_row_set(&g,0,TR_VOCAL,5);
    un_push_rows(&u,&g);sg_row_delete(&g,0);CHECK(g.nrows==0);
    CHECK(un_pop(&u,&g)&&g.nrows==1&&g.row[0].pat[TR_VOCAL]==5);
    for(i=0;i<12;i++){x.v[0]=(uint8_t)(10+i);sg_set_step(&g,TR_VOCAL,0,0,&x);un_push_pat(&u,&g,TR_VOCAL,0);}
    for(i=0;i<UN_N;i++)CHECK(un_pop(&u,&g)&&g.pat[TR_VOCAL][0][0].v[0]==21-i);
    CHECK(!un_pop(&u,&g));
    printf("sizeof Song %zu  Seq %zu  Undo %zu\n",sizeof(Song),sizeof(Seq),sizeof(Undo));
}
int main(void){
    test_io();test_seq();test_chords();test_demo();test_edit();
    if(fails)printf("%d FAILURES\n",fails);else printf("song2: all tests passed\n");
    return fails!=0;
}

/* SPDX-License-Identifier: GPL-3.0-only
 * MIKU-FM1 native: a singing songwriter for the FM-1 on Dinghy. Four tracks (VOCAL, CHORDS, BASS, DRUMS), patterns of 16 steps,
 * an arrangement of rows, 8 song slots. Pages: STAGE (the singer), SONG, PATTERN, MIXER, GEN (the song generator). Unofficial fan project, not a voicebank.
 *
 * Threads. The ISR (app_audio -> en_render) owns the sequencer and the instruments and READS the song; the main loop edits it.
 * - Single-step edits (st_put) are safe while playing: a step is 4 bytes, byte 0 is "is there anything" (note / mask). They are
 *   written as volatile bytes in an order where any interleaving with the ISR's reads is a valid step: setting writes bytes 1..3
 *   first and byte 0 last, clearing writes byte 0 first. Every byte is range-checked beforehand (sg_step_ok), so each byte
 *   the ISR can see is individually in range, and a step is only played when byte 0 is set. Row pattern/section/mixer fields are
 *   single bytes: also atomic.
 * - Structural edits (row insert/delete/duplicate, pattern transpose, undo, song load/new) run between lk() and ulk(): the
 *   ISR then stops running the sequencer (instruments keep sounding their tails) and acknowledges; the main loop waits for it.
 * - Commands to the ISR (play, stop, bpm, live notes) go through en_post(): a ring of one-word commands, one producer. */
#define APP_NAME "MIKU-FM"
#include "platform.c"
#include "firmware/audio/fvoice/fvoice.c"
#include "firmware/ui/singer_sprites.h"
#include "firmware/app/engine.h"
/* the modules' private statics share names with each other (and with platform.c): give each file its own */
#define cpy ch_cpy
#define zero ch_zero
#define pre ch_pre
#define block ch_block
#include "firmware/audio/fmband/fmband_ch.c"
#undef cpy
#undef zero
#undef pre
#undef block
#define cpy bs_cpy
#define zero bs_zero
#define pre bs_pre
#define block bs_block
#include "firmware/audio/fmband/fmband_bs.c"
#undef cpy
#undef zero
#undef pre
#undef block
#define cpy dr_cpy
#define zero dr_zero
#define pre dr_pre
#define block dr_block
#include "firmware/audio/fmband/fmband_dr.c"
#undef cpy
#undef zero
#undef pre
#undef block
#include "firmware/audio/fmband/fmband_tables.c"
#include "firmware/audio/fmband/fmband_mx.c"
#define put dm_put
#define line dm_line
#define chord dm_chord
#define bass dm_bass
#define drums dm_drums
#define row dm_row
#include "firmware/song/demo.c"
#undef put
#undef line
#undef chord
#undef bass
#undef drums
#undef row
#define C SqC
#define IN SqIN
#define IV SqIV
#define em sq_em
#define pass sq_pass
#define fire sq_fire
#define dur sq_dur
#define aud sq_aud
#include "firmware/song/seq.c"
#undef C
#undef IN
#undef IV
#include "firmware/song/song.c"
#include "firmware/song/song_io.c"
#include "firmware/song/mora.c"
#include "firmware/song/gen.c"
#include "firmware/app/engine.c"
#define BUDGET_TICKS (5805u*FM1_TICKS_PER_US)       /* 256 frames at 44.1 kHz */
#define BG RGB(7,17,34)
#define FACE_Y 184u
enum { PG_STAGE, PG_SONG, PG_PATTERN, PG_MIXER, PG_GEN, PG_N };
static Engine en;
static Song sg;
static Undo un;
static Song gbak;                                   /* GEN: the song before the last take (GLO drops the take) */
static GenCfg gcf={GS_POP,0,GK_MAJOR,0,1};
static uint32_t take,take_mod,gvar;
static uint8_t enc[SONG_ENC_SIZE];                 /* slot I/O buffer */
static struct { uint32_t magic; uint8_t ver, slot, pan[NTRK], pad[2]; } pf;   /* settings: 0.3 saved 37 bytes, so its data never loads here */
#define PF_MAGIC 0x4D4B5632u
static volatile uint32_t pub_peak;
static Step clip[NSTEP];
static uint32_t clip_n,clip_t,anchor,arp_c;
static int32_t oct;
static uint32_t page,trk,crow,cstep,top,modified,playing,ppos,prev_keys,dirty,full,cpu_ms,cpu_pct,last_act,
    pend,pend_t,menu,menu_slot,sv_t,sv_c,sv_on,toast_until,toast_err,pv_off,u_kind,u_trk,u_pat,u_ms,lk_t0,
    face_cur,face_ms,blink_until,next_blink,happy_until;
static const char *toast_s;
static uint32_t ed_len[3]={2,8,2},ed_vel=12,ed_mora,ed_q,ed_mode,ed_bf;
static uint8_t kn[27],hw[2u*SINGER_FACE_H];
static uint16_t pw[16],c_bg,c_disc;
static const uint16_t tcol[NTRK]={RGB(247,123,180),RGB(35,182,176),RGB(249,207,94),RGB(120,150,250)};
static const char *const tname[NTRK]={"VOCAL","CHORDS","BASS","DRUMS"};
static const char *const sname[SEC_N]={"","INTRO","VERSE","CHORUS","BRIDGE","OUTRO"};
static const char *const cqn[10]={"MAJ","MIN","7","MAJ7","MIN7","SUS4","SUS2","DIM","AUG","ADD9"};
static const char *const cmn[8]={"BLOCK","ARP UP","ARP DN","ARP UD","STRUM","UP 32","DN 32","UD 32"};
static const char *const dnn[6]={"KICK","SNARE","CLAP","CHAT","OHAT","PERC"};
static const char *const pnm[NTRK][8]={{"VOICE"},{"BELLS","E.PIANO","CHIP ARP","PAD","ORGAN","PLUCK","MARIMBA","CLAV"},{"FM BASS","SUB","CHIP SQ","ACID"},{"808","CHIP"}};
static const uint8_t kbase[NTRK]={53,41,29,0};     /* key 0 (F3) plays this note (before OCT) */
static const uint8_t npst[NTRK]={1,8,4,2};
static void app_audio(int32_t *out,uint32_t n){
    static uint32_t pk;
    uint32_t t0=fm1_ticks(),c;
    if(!en.up)en_init(&en,&sg);
    en_render(&en,out,n);
    c=fm1_ticks()-t0;
    if(c>pk)pk=c;else pk-=pk>>6;
    pub_peak=pk;
}
/* ---- small helpers */
static void num(char *s,uint32_t n){
    char t[6];uint32_t i=0,j=0;
    do{t[i++]=(char)('0'+n%10u);n/=10u;}while(n&&i<5u);
    while(i)s[j++]=t[--i];
    s[j]=0;
}
static void cat(char *d,const char *s){while(*d)d++;while((*d++=*s++)!=0);}
static void catn(char *d,uint32_t n){char t[8];num(t,n);cat(d,t);}
static int32_t cl(int32_t v,int32_t lo,int32_t hi){return v<lo?lo:v>hi?hi:v;}
static int32_t wr(int32_t v,int32_t n){v%=n;return v<0?v+n:v;}
static void nname(char *s,uint32_t n){
    static const char *const nm[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    cat(s,nm[n%12u]);catn(s,n>=12u?n/12u-1u:0);
}
static void mk(uint32_t m){dirty|=m;}
static void toast(const char *s,uint32_t err){toast_s=s;toast_err=err;toast_until=fm1_ms+1500u;mk(0x21u);}
static uint32_t idle_w(void){platform_poll();return fm1_ms-lk_t0<40u;}
static void lk(void){lk_t0=fm1_ms;en_lock(&en,idle_w);}
static void ulk(void){lk_t0=fm1_ms;en_unlock(&en,idle_w);}
static void wait_ms(uint32_t ms){uint32_t t=fm1_ms;while(fm1_ms-t<ms)platform_poll();}
/* ---- song access and editing */
static uint32_t pat_of(void){return crow<sg.nrows?sg.row[crow].pat[trk]:ROW_OFF;}
static void st_put(uint32_t t,uint32_t p,uint32_t i,const Step *s){
    volatile uint8_t *d=sg.pat[t][p][i].v;
    uint32_t k;
    if(!sg_step_ok((int)t,s))return;
    if(s->v[0]){for(k=1;k<4;k++)d[k]=s->v[k];d[0]=s->v[0];}
    else{d[0]=0;for(k=1;k<4;k++)d[k]=s->v[k];}
    modified=1;
}
static void u_pushpat(uint32_t t,uint32_t p){            /* undo snapshot BEFORE the edit; a burst of edits on one pattern is one undo */
    if(!(u_kind==1 && u_trk==t && u_pat==p && fm1_ms-u_ms<2000u))un_push_pat(&un,&sg,(int)t,(int)p);
    u_kind=1;u_trk=t;u_pat=p;u_ms=fm1_ms;
}
static void u_pushrows(void){
    if(!(u_kind==2 && fm1_ms-u_ms<2000u))un_push_rows(&un,&sg);
    u_kind=2;u_ms=fm1_ms;
}
static int32_t ensure(void){                              /* the pattern at (cursor row, track); creates the row / a free pattern if needed */
    uint32_t p;
    if(!sg.nrows){u_pushrows();lk();sg_row_insert(&sg,0);ulk();crow=0;}
    if(crow>=sg.nrows)crow=sg.nrows-1u;
    p=sg.row[crow].pat[trk];
    if(p==ROW_OFF){
        int f=sg_find_free(&sg,(int)trk);
        if(f<0){toast("NO FREE PATTERN",1);return -1;}
        u_pushrows();sg_row_set(&sg,(int)crow,(int)trk,f);p=(uint32_t)f;modified=1;mk(0x1Fu);
    }
    return (int32_t)p;
}
static void kwrite(uint32_t k,uint32_t note){
    int32_t p=ensure();
    Step s,*o;
    if(p<0)return;
    o=&sg.pat[trk][p][cstep];
    s.v[0]=s.v[1]=s.v[2]=s.v[3]=0;
    if(trk==TR_VOCAL){s.v[0]=(uint8_t)note;s.v[1]=(uint8_t)ed_len[0];s.v[2]=(uint8_t)ed_mora;s.v[3]=(uint8_t)ed_vel;}
    else if(trk==TR_CHORDS){s.v[0]=(uint8_t)note;s.v[1]=(uint8_t)(ed_mode<<4|ed_q);s.v[2]=(uint8_t)ed_len[1];s.v[3]=11;}
    else if(trk==TR_BASS){s.v[0]=(uint8_t)note;s.v[1]=(uint8_t)ed_len[2];s.v[2]=(uint8_t)ed_bf;s.v[3]=(uint8_t)ed_vel;}
    else{
        uint32_t i=k%6u,m=o->v[0],a=o->v[1];
        if(k>11u)return;
        if(k<6u){m^=1u<<i;a&=m;}else{m|=1u<<i;a^=1u<<i;}
        s.v[0]=(uint8_t)m;s.v[1]=(uint8_t)a;
    }
    u_pushpat(trk,(uint32_t)p);st_put(trk,(uint32_t)p,cstep,&s);mk(0x1Eu);
}
static void ctl(uint32_t w,int32_t d){                    /* PATTERN: w 0 length, 1 velocity / chord mode, 2 mora / quality / bass flags */
    int32_t p=(int32_t)pat_of();
    Step s,*o=0;
    uint32_t has;
    if(trk==TR_DRUMS)return;
    if(p!=(int32_t)ROW_OFF)o=&sg.pat[trk][p][cstep];
    has=o && o->v[0];
    if(has)s=*o;
    if(w==0){ed_len[trk]=(uint32_t)cl((int32_t)ed_len[trk]+d,1,64);if(has)s.v[trk==TR_CHORDS?2:1]=(uint8_t)ed_len[trk];}
    else if(w==1){
        if(trk==TR_CHORDS){ed_mode=(uint32_t)wr((int32_t)ed_mode+d,8);if(has)s.v[1]=(uint8_t)(ed_mode<<4|(s.v[1]&15u));}
        else{ed_vel=(uint32_t)cl((int32_t)ed_vel+d,0,15);if(has)s.v[3]=(uint8_t)((s.v[3]&0xF0u)|ed_vel);}
    }else if(trk==TR_VOCAL){
        ed_mora=(uint32_t)wr((int32_t)ed_mora+d,MORA_N);if(has)s.v[2]=(uint8_t)ed_mora;
        if(!playing && !prev_keys){en_post(&en,EN_ON,0,has?o->v[0]:67u,ed_mora);pv_off=fm1_ms+300u;}
    }else if(trk==TR_CHORDS){ed_q=(uint32_t)wr((int32_t)ed_q+d,10);if(has)s.v[1]=(uint8_t)((s.v[1]&0xF0u)|ed_q);}
    else{ed_bf=(uint32_t)wr((int32_t)ed_bf+d,4);if(has)s.v[2]=(uint8_t)ed_bf;}
    if(has){u_pushpat(trk,(uint32_t)p);st_put(trk,(uint32_t)p,cstep,&s);}
    mk(0x1Fu);
}
static void do_undo(void){
    uint32_t r;
    lk();r=(uint32_t)un_pop(&un,&sg);ulk();
    u_kind=0;
    if(sg.nrows && crow>=sg.nrows)crow=sg.nrows-1u;
    if(r)modified=1;
    toast(r?"UNDO":"NOTHING TO UNDO",!r);mk(0x3Fu);
}
/* ---- storage */
static void pf_defaults(void){
    static const uint8_t pn[NTRK]={64,44,64,64};
    uint32_t i;
    pf.magic=PF_MAGIC;pf.ver=1;pf.slot=0;
    for(i=0;i<NTRK;i++)pf.pan[i]=pn[i];
}
static void pf_apply(void){uint32_t i;for(i=0;i<NTRK;i++)en.pan[i]=pf.pan[i];}
static void after_load(void){
    crow=cstep=top=0;un_init(&un);u_kind=0;modified=0;take=0;
    en_post(&en,EN_PANIC,0,0,0);
    mk(0x3Fu);full=1;
}
static void stop_wait(void){if(playing){en_post(&en,EN_STOP,0,0,0);wait_ms(15);}}
static uint32_t slot_name(uint32_t s,char *nm){            /* 1 = a valid song is in the slot */
    uint32_t i;
    nm[0]=0;
    if(slot_load(s,enc,SONG_ENC_SIZE)||enc[0]!='M'||enc[1]!='K'||enc[2]!='S'||enc[3]!='2')return 0;
    for(i=0;i<8u;i++)nm[i]=(char)enc[10+i];
    nm[8]=0;
    return 1;
}
static void do_save(uint32_t s){
    uint32_t ok,t=fm1_ms;
    if(playing){toast("STOP FIRST",1);return;}
    en_post(&en,EN_PANIC,0,0,0);
    while(en.act && fm1_ms-t<800u)platform_poll();      /* erase only when silent */
    ok=song_encode(&sg,enc,sizeof enc)==SONG_ENC_SIZE && slot_save(s,enc,SONG_ENC_SIZE)==0;
    if(ok){pf.slot=(uint8_t)s;ok=settings_save(&pf,sizeof pf)==0;}
    if(ok)modified=0;
    toast(ok?"SAVED":"SAVE FAILED",!ok);
}
static void do_load(uint32_t s){
    stop_wait();
    if(slot_load(s,enc,SONG_ENC_SIZE)){toast("EMPTY SLOT",1);return;}
    lk();
    {int r=song_decode(&sg,enc,SONG_ENC_SIZE);ulk();
     if(r!=SIO_OK){toast("LOAD FAILED",1);return;}}
    pf.slot=(uint8_t)s;after_load();toast("LOADED",0);
}
static void do_new(void){
    stop_wait();lk();
    sg_init(&sg);sg_row_insert(&sg,0);
    {uint32_t t;for(t=0;t<NTRK;t++)sg_row_set(&sg,0,(int)t,0);}
    ulk();after_load();toast("NEW SONG",0);
}
static void do_demo(void){stop_wait();lk();song_demo(&sg);ulk();after_load();toast("DEMO LOADED",0);}
static void guarded(uint32_t act,uint32_t arg){            /* actions that drop the current song need a second press when it is unsaved */
    if(modified && !(pend==act && (int32_t)(fm1_ms-pend_t)<0)){pend=act;pend_t=fm1_ms+3000u;toast("UNSAVED! AGAIN",1);return;}
    pend=0;
    if(act==1)do_load(arg);else if(act==2)do_new();else do_demo();
}
/* ---- face: 4bpp sprite rows, 2x, into the canvas buffer, blitted (the panel wants bytes swapped: swap16) */
#define FX 30u
#define FY 2u
static void face_draw(uint32_t f,uint32_t sx,uint32_t sy,uint32_t w,uint32_t h){
    const uint8_t *d=SINGER_FACES[f];
    uint32_t cr=CV_MAX/(4u*w),r0,r,c,k;
    cv_oy=0;
    for(r0=sy;r0<sy+h;r0+=cr){
        uint32_t nr=sy+h-r0<cr?sy+h-r0:cr;
        lcd_sync();
        for(r=0;r<nr;r++){
            uint16_t *o=cv_px+r*2u*2u*w;
            int32_t hh=hw[2u*(r0+r)];
            for(c=0;c<w;c++){
                uint32_t x=sx+c,b=d[((r0+r)*SINGER_FACE_W+x)>>1],i=(x&1u)?b&15u:b>>4;
                int32_t dx=(int32_t)(2u*x)-89;
                uint16_t p=i?pw[i]:((dx<0?-dx:dx)<=hh?c_disc:c_bg);
                o[2u*c]=o[2u*c+1u]=p;
            }
            for(k=0;k<2u*w;k++)o[2u*w+k]=o[k];
        }
        lcd_blit(FX+2u*sx,FY+2u*r0,2u*w,2u*nr,cv_px);
    }
}
static void face_set(uint32_t f){
    uint32_t a=face_cur,x0,y0,x1,y1;
    const uint8_t *p=SINGER_FACE_DIRTY[a],*q=SINGER_FACE_DIRTY[f];
    if(f==a)return;
    face_cur=f;
    if(page)return;
    if(!p[2]){x0=q[0];y0=q[1];x1=x0+q[2];y1=y0+q[3];}
    else if(!q[2]){x0=p[0];y0=p[1];x1=x0+p[2];y1=y0+p[3];}
    else{
        x0=p[0]<q[0]?p[0]:q[0];y0=p[1]<q[1]?p[1]:q[1];
        x1=p[0]+p[2]>q[0]+q[2]?p[0]+p[2]:q[0]+q[2];y1=p[1]+p[3]>q[1]+q[3]?p[1]+p[3]:q[1]+q[3];
    }
    face_draw(f,x0,y0,x1-x0,y1-y0);
}
static void face_update(void){
    static const uint8_t vf[5]={SF_A,SF_I,SF_U,SF_E,SF_O};
    static uint32_t lip_until;
    uint32_t now=fm1_ms,m=en.mouth,w;
    if(m==5u && en.level>=6u)lip_until=now+70u;              /* lips shut while sounding (m b p closure, the n hum): hold so a 40 ms face update sees it */
    if((int32_t)(now-happy_until)<0)w=SF_HAPPY;
    else if((int32_t)(now-lip_until)<0)w=SF_N;
    else if(m<5u)w=en.level>=100u?vf[m]:SF_N;
    else if((int32_t)(now-blink_until)<0)w=SF_BLINK;
    else if((int32_t)(now-next_blink)>=0){
        blink_until=now+120u;next_blink=now+2500u+(rng()>>16)%2500u;w=SF_BLINK;
    }else w=SF_NEUTRAL;
    if(w!=face_cur && (uint32_t)(now-face_ms)>=40u){face_ms=now;face_set(w);}
}
static void stage_init(void){
    uint32_t i,v,h;
    for(i=0;i<16;i++)pw[i]=swap16(SINGER_FACE_PAL[i]);
    c_bg=swap16(RGB(12,20,40));c_disc=swap16(RGB(40,66,120));
    for(i=0;i<2u*SINGER_FACE_H;i++){        /* half-width of a disc (r=90) behind the head, per output row */
        int32_t dy=(int32_t)i-90;
        v=(uint32_t)(8100-dy*dy);h=0;
        while((h+1u)*(h+1u)<=v)h++;
        hw[i]=(uint8_t)h;
    }
}
/* ---- pages. The screen below the face is drawn in bands of 48 lines (bit n of `dirty` = band n; bit 5 = the STAGE strip) */
static uint32_t shown_row(void){return playing?(ppos>>8&255u):crow;}
static void band_begin(uint32_t b){cv_begin(240,48,BG);cv_oy=-(int32_t)(b*48u);}
static void band_end(uint32_t b){cv_oy=0;cv_blit(0,b*48u);}
static void txt(int32_t x,int32_t y,const felucca_font_t *f,const char *s,uint16_t c,int al){
    int32_t w=text_w(f,s);
    cv_text(al==1?x-w/2:al==2?x-w:x,y,f,s,c);
}
static void cell(int32_t x,int32_t y,int32_t w,int32_t h,const char *s,uint16_t fg,uint16_t bg){
    cv_rect(x,y,w,h,bg);txt(x+w/2,y+(h-16)/2,&FONT_S,s,fg,1);
}
static void head(const char *name,const char *l2,uint16_t c2){
    char s[12];
    uint32_t t=fm1_ms;
    if(toast_s && (int32_t)(t-toast_until)<0)txt(4,0,&FONT_L,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),0);
    else{
        txt(4,0,&FONT_L,name,RGB(64,222,209),0);
        s[0]=0;cat(s,tname[trk]);txt(236,8,&FONT_S,s,tcol[trk],2);
    }
    txt(4,32,&FONT_S,l2,c2,0);
}
static void sname_slot(char *d){
    uint32_t i;
    d[0]=0;catn(d,(uint32_t)pf.slot+1u);cat(d," ");
    for(i=0;i<8u;i++){char c[2]={sg.name[i],0};cat(d,c);}
}
static void pg_song(uint32_t b){
    char s[28];
    uint32_t i,r;
    if(!b){
        sname_slot(s);cat(s,modified?"*":"");
        head("SONG",s,RGB(150,160,190));
        s[0]=0;catn(s,sg.nrows);cat(s,sg.loop?" ROWS LOOP":" ROWS");txt(236,32,&FONT_S,s,RGB(249,207,94),2);
        return;
    }
    if(crow<top)top=crow;
    if(crow>=top+8u)top=crow-7u;
    if(!sg.nrows){txt(120,100,&FONT_S,"EMPTY SONG",RGB(150,160,190),1);txt(120,124,&FONT_S,"EDIT ADDS A ROW",RGB(100,110,140),1);return;}
    for(i=0;i<2u;i++){
        int32_t y=(int32_t)(48u+(b-1u)*48u+i*24u);
        r=top+(b-1u)*2u+i;
        if(r>=sg.nrows)continue;
        if(r==crow)cv_rect(0,y,240,24,RGB(24,40,76));
        s[0]=0;if(r<9u)cat(s," ");catn(s,r+1u);
        txt(6,y+4,&FONT_S,s,(playing && (ppos>>8&255u)==r)?RGB(249,207,94):RGB(150,160,190),0);
        if(playing && (ppos>>8&255u)==r)cv_rect(0,y+2,3,20,RGB(249,207,94));
        txt(34,y+4,&FONT_S,sname[sg.row[r].sect],RGB(64,222,209),0);
        {uint32_t t;for(t=0;t<NTRK;t++){
            uint32_t p=sg.row[r].pat[t];
            s[0]=0;if(p==ROW_OFF)cat(s,"--");else catn(s,p+1u);
            if(r==crow && t==trk)cell(110+(int32_t)t*32,y+1,30,22,s,BG,tcol[t]);
            else cell(110+(int32_t)t*32,y+1,30,22,s,p==ROW_OFF?RGB(80,90,120):tcol[t],RGB(14,26,50));
        }}
    }
}
static void roll(void){
    uint32_t p=pat_of(),i,lo=127,hi=0,ph=playing && (ppos>>8&255u)==crow?(ppos&255u):99u;
    int32_t cw=15;
    for(i=0;i<NSTEP;i++){                                  /* beats and the cursor column */
        cv_rect((int32_t)i*cw,52,cw-1,136,(i&3u)?RGB(12,24,46):RGB(16,30,58));
        if(i==cstep)cv_rect((int32_t)i*cw,52,cw-1,136,RGB(34,52,96));
    }
    if(p==ROW_OFF){txt(120,100,&FONT_S,"SILENT HERE",RGB(150,160,190),1);txt(120,124,&FONT_S,"PLAY A KEY OR PRESETS",RGB(100,110,140),1);}
    else if(trk==TR_DRUMS){
        for(i=0;i<NSTEP;i++){
            const Step *s=&sg.pat[3][p][i];
            uint32_t k;
            for(k=0;k<6u;k++){
                int32_t y=54+(int32_t)k*22;
                if(s->v[0]&(1u<<k)){
                    cv_rect((int32_t)i*cw,y,cw-1,20,tcol[3]);
                    if(s->v[1]&(1u<<k))cv_rect((int32_t)i*cw+4,y+6,7,8,C_WHITE);
                }else cv_rect((int32_t)i*cw+2,y+7,cw-5,6,RGB(30,40,70));
            }
        }
    }else{
        for(i=0;i<NSTEP;i++){uint32_t n=sg.pat[trk][p][i].v[0];if(n){if(n<lo)lo=n;if(n>hi)hi=n;}}
        if(hi<lo){lo=60;hi=72;}
        if(hi-lo<11u)hi=lo+11u;
        for(i=0;i<NSTEP;i++){
            const Step *s=&sg.pat[trk][p][i];
            uint32_t n=s->v[0],ln=trk==TR_CHORDS?s->v[2]:s->v[1];
            int32_t y,w;
            if(!n)continue;
            y=176-(int32_t)((n-lo)*116u/(hi-lo));
            w=(int32_t)ln*cw-1;
            if((int32_t)i*cw+w>240)w=240-(int32_t)i*cw;
            cv_rect((int32_t)i*cw,y,w,8,tcol[trk]);
            cv_rect((int32_t)i*cw,y,3,8,i==cstep?C_WHITE:RGB(16,31,50));
        }
    }
    if(ph<NSTEP)cv_rect((int32_t)ph*cw,52,cw-1,3,RGB(249,207,94));
    for(i=0;i<NSTEP;i++)if(i==cstep)cv_rect((int32_t)i*cw,185,cw-1,3,RGB(247,123,180));
}
static void pg_pattern(uint32_t b){
    char s[30];
    uint32_t p=pat_of();
    if(!b){
        s[0]=0;cat(s,"R");catn(s,crow+1u);cat(s," P");if(p==ROW_OFF)cat(s,"--");else catn(s,p+1u);
        cat(s," ");cat(s,crow<sg.nrows?sname[sg.row[crow].sect]:"");
        head("PATTERN",s,RGB(150,160,190));
        s[0]=0;cat(s,"STEP ");catn(s,cstep+1u);txt(236,32,&FONT_S,s,RGB(249,207,94),2);
        return;
    }
    if(b<4u){roll();return;}
    {
        const Step *o=p==ROW_OFF?0:&sg.pat[trk][p][cstep];
        s[0]=0;
        if(trk==TR_DRUMS){
            uint32_t k,m=o?o->v[0]:0;
            for(k=0;k<6u;k++)if(m&(1u<<k)){cat(s,dnn[k]);cat(s,(o->v[1]>>k)&1u?"! ":" ");}
            if(!m)cat(s,"EMPTY STEP");
            txt(4,196,&FONT_S,s,tcol[3],0);
            txt(4,218,&FONT_S,"KEYS 1-6 HIT  7-12 ACCENT",RGB(100,110,140),0);
        }else{
            if(o && o->v[0]){
                nname(s,o->v[0]);cat(s," L");
                if(trk==TR_VOCAL){catn(s,o->v[1]);cat(s," ");cat(s,mora_str(o->v[2]));cat(s," V");catn(s,o->v[3]&15u);}
                else if(trk==TR_CHORDS){catn(s,o->v[2]);cat(s," ");cat(s,cqn[o->v[1]&15u]);cat(s," ");cat(s,cmn[o->v[1]>>4]);}
                else{catn(s,o->v[1]);cat(s,o->v[2]&1u?" ACC":"");cat(s,o->v[2]&2u?" SLIDE":"");}
            }else cat(s,"REST");
            txt(4,196,&FONT_S,s,tcol[trk],0);
            s[0]=0;
            if(trk==TR_VOCAL){cat(s,"NEXT: ");cat(s,ed_mora?mora_str(ed_mora):"(A)");cat(s," L");catn(s,ed_len[0]);}
            else if(trk==TR_CHORDS){cat(s,"NEXT: ");cat(s,cqn[ed_q]);cat(s," ");cat(s,cmn[ed_mode]);}
            else{cat(s,"NEXT: L");catn(s,ed_len[2]);cat(s,ed_bf&1u?" ACC":"");cat(s,ed_bf&2u?" SLIDE":"");}
            txt(4,218,&FONT_S,s,RGB(100,110,140),0);
        }
    }
}
static void pg_mixer(uint32_t b){
    char s[30];
    uint32_t t,e,pn;
    if(!b){
        s[0]=0;cat(s,"BPM ");catn(s,sg.bpm);cat(s,"  SWING ");catn(s,sg.swing);cat(s,"  MST ");catn(s,sg.master);
        head("MIXER",s,RGB(249,207,94));
        return;
    }
    t=b-1u;
    {
        const Trk *k=&sg.trk[t];
        int32_t y=(int32_t)b*48;
        uint32_t mu=k->mute;
        e=en_pre(t,k->preset);pn=pf.pan[t];
        if(t==trk)cv_rect(0,y,240,47,RGB(24,40,76));
        txt(6,y+4,&FONT_S,tname[t],mu?RGB(90,100,130):tcol[t],0);
        txt(88,y+4,&FONT_S,pnm[t][e],RGB(150,160,190),0);
        if(mu)cell(196,y+3,18,18,"M",BG,RGB(255,110,110));
        if(k->solo)cell(216,y+3,18,18,"S",BG,RGB(249,207,94));
        cv_rect(6,y+30,110,10,RGB(30,40,70));
        cv_rect(6,y+30,(int32_t)k->level*110/127,10,mu?RGB(90,100,130):tcol[t]);
        cv_rect(130,y+34,100,2,RGB(60,70,100));
        cv_rect(180,y+28,2,14,RGB(60,70,100));
        cv_rect(130+(int32_t)pn*98/127,y+28,3,14,C_WHITE);
    }
}
static void pg_stage(void){
    char s[14]="BPM ",c[12]="CPU ",up[6];
    const char *ms="";
    uint32_t i,r=shown_row(),x=playing?(ppos&255u):cstep,vp=r<sg.nrows?sg.row[r].pat[0]:ROW_OFF;
    if(en.act&1u)ms=mora_str(en.mora);                     /* what is sung now, else the vocal step under the cursor */
    else if(!playing && vp<NPAT && sg.pat[0][vp][x].v[0])ms=mora_str(sg.pat[0][vp][x].v[2]);
    for(i=0;i<5u&&ms[i];i++)up[i]=(char)(ms[i]-32);
    up[i]=0;
    num(s+4,sg.bpm);num(c+4,cpu_pct);cat(c,"%");
    if(oct){cat(s,oct>0?" OCT+":" OCT-");catn(s,(uint32_t)(oct>0?oct:-oct));}
    cv_begin(240,56,RGB(12,20,40));
    if(toast_s && (int32_t)(fm1_ms-toast_until)<0)txt(120,0,&FONT_L,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),1);
    else cv_text(120-text_w(&FONT_L,up)/2,0,&FONT_L,up,C_WHITE);
    cv_text(4,14,&FONT_S,s,RGB(249,207,94));
    cv_text(236-text_w(&FONT_S,c),14,&FONT_S,c,RGB(150,160,190));
    s[0]=0;cat(s,"R");catn(s,r+1u);cat(s," ");cat(s,r<sg.nrows?sname[sg.row[r].sect]:"");
    cv_text(4,38,&FONT_S,s,RGB(150,160,190));
    cv_text(4,2,&FONT_S,"STAGE",RGB(64,222,209));
    for(i=0;i<NTRK;i++)cv_rect(236-(int32_t)(NTRK-i)*16,40,12,12,(en.act>>i)&1u?tcol[i]:RGB(49,54,78));
    cv_blit(0,FACE_Y);
}
/* ---- GEN: a take is one generate (REC) or one new phrase (ARP); EDIT keeps it, GLO drops it (back to the song before it) */
static void gen_go(uint32_t phrase){
    uint32_t pl=playing,r=1;
    if(phrase && (gcf.lock>>trk&1u)){toast("TRACK LOCKED",1);return;}
    if(!phrase)stop_wait();
    gbak=sg;take_mod=modified;take=1;un_init(&un);u_kind=0;
    lk();
    if(phrase)r=(uint32_t)gen_phrase(&sg,&gcf,(int)crow,(int)trk,++gvar);
    else{gen_song(&sg,&gcf);gvar=0;}
    ulk();
    if(!r){take=0;toast("NO FREE PATTERN",1);return;}
    modified=1;
    if(!phrase){crow=top=0;if(pl)en_post(&en,EN_PLAY,0,0,0);}
    if(crow>=sg.nrows)crow=sg.nrows?sg.nrows-1u:0;
    toast(phrase?"NEW PHRASE":"NEW SONG",0);mk(0x1Fu);
}
static void gen_drop(void){
    uint32_t pl=playing;
    if(!take){toast("NO TAKE TO DROP",1);return;}
    stop_wait();
    lk();sg=gbak;ulk();
    take=0;modified=take_mod;
    if(crow>=sg.nrows)crow=sg.nrows?sg.nrows-1u:0;
    if(pl && sg.nrows)en_post(&en,EN_PLAY,crow,0,0);
    toast("DROPPED",0);mk(0x1Fu);
}
static void pg_gen(uint32_t b){
    static const char *const kn12[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    char s[30];
    uint32_t t;
    if(!b){
        head("GEN",take?"EDIT KEEP   GLO DROP":"REC SONG   ARP PHRASE",take?RGB(249,207,94):RGB(150,160,190));
        s[0]=0;cat(s,"ROW ");catn(s,crow+1u);txt(236,32,&FONT_S,s,RGB(249,207,94),2);
        return;
    }
    if(b==1u){
        txt(4,52,&FONT_S,"STYLE  K1",RGB(100,110,140),0);
        txt(4,66,&FONT_L,gen_style(gcf.style),RGB(64,222,209),0);
        txt(236,52,&FONT_S,"KEY K2  K3",RGB(100,110,140),2);
        s[0]=0;cat(s,kn12[gcf.key%12u]);cat(s,gcf.scale?" MIN":" MAJ");
        txt(236,66,&FONT_L,s,RGB(249,207,94),2);
    }else if(b==2u){
        txt(4,100,&FONT_S,"SEED  K4  ALG x100",RGB(100,110,140),0);
        s[0]=0;catn(s,gcf.seed);txt(4,114,&FONT_L,s,C_WHITE,0);
        s[0]=0;cat(s,"BPM ");catn(s,sg.bpm);txt(236,124,&FONT_S,s,RGB(249,207,94),2);
    }else if(b==3u){
        txt(4,146,&FONT_S,"PRESS TRACK AGAIN: LOCK",RGB(100,110,140),0);
        for(t=0;t<NTRK;t++){
            uint32_t lkd=gcf.lock>>t&1u;
            int32_t x=4+(int32_t)t*59;
            if(t==trk)cv_rect(x-2,164,59,26,C_WHITE);
            cell(x,166,55,22,tname[t],lkd?RGB(150,160,190):BG,lkd?RGB(49,54,78):tcol[t]);
        }
    }else{
        s[0]=0;
        for(t=0;t<NTRK;t++)if(gcf.lock>>t&1u){if(s[0])cat(s," ");cat(s,tname[t]);}
        txt(4,196,&FONT_S,s[0]?"LOCKED:":"NOTHING LOCKED",RGB(150,160,190),0);
        if(s[0])txt(4,216,&FONT_S,s,RGB(255,110,110),0);
    }
}
static void redraw(void){
    uint32_t b;
    if(full){
        full=0;
        if(!page){lcd_fill(0,0,240,240,RGB(12,20,40));face_draw(face_cur,0,0,SINGER_FACE_W,SINGER_FACE_H);dirty|=0x20u;}
        else dirty|=0x1Fu;
    }
    if(!page){if(dirty&0x20u){dirty&=~0x20u;pg_stage();}dirty&=~0x1Fu;return;}
    dirty&=~0x20u;
    for(b=0;b<5u;b++)if(dirty&(1u<<b)){
        dirty&=~(1u<<b);
        band_begin(b);
        if(page==PG_SONG)pg_song(b);else if(page==PG_PATTERN)pg_pattern(b);else if(page==PG_GEN)pg_gen(b);else pg_mixer(b);
        band_end(b);
        return;                                            /* one band per pass: the main loop polls between */
    }
}
/* ---- input */
static void stop_play(void){if(playing){en_post(&en,EN_STOP,0,0,0);}happy_until=0;mk(0x3Fu);}
static void tweak(uint8_t *v,int32_t d,int32_t lo,int32_t hi){*v=(uint8_t)cl((int32_t)*v+d,lo,hi);modified=1;}
static void do_play(void){
    if(playing){stop_play();return;}
    if(!sg.nrows){toast("NO ROWS",1);return;}
    if(crow>=sg.nrows)crow=sg.nrows-1u;
    en_post(&en,EN_PLAY,crow,page==PG_PATTERN,0);
    happy_until=fm1_ms+500u;
}
static void menu_draw(void){
    char s[24],nm[10];
    uint32_t ok=slot_name(menu_slot,nm);
    cv_oy=0;
    lcd_fill(0,0,240,240,BG);
    s[0]=0;cat(s,"SLOT ");catn(s,menu_slot+1u);
    draw_text_box(0,20,240,&FONT_L,s,RGB(64,222,209),1);
    draw_text_box(0,70,240,&FONT_L,ok?nm:"EMPTY",ok?C_WHITE:RGB(100,110,140),1);
    draw_text_box(0,118,240,&FONT_S,menu_slot==pf.slot?"(CURRENT)":"",RGB(249,207,94),1);
    draw_text_box(0,146,240,&FONT_S,"SELECT: SLOT",RGB(150,160,190),1);
    draw_text_box(0,166,240,&FONT_S,"FX LOAD   SCL NEW   ENV DEMO",RGB(150,160,190),1);
    draw_text_box(0,190,240,&FONT_S,modified?"UNSAVED CHANGES":"",RGB(255,110,110),1);
    draw_text_box(0,214,240,&FONT_S,"RELEASE SAVE: SAVE HERE",RGB(100,110,140),1);
}
static void app_main(void){
    uint32_t i;
    pf_defaults();
    {uint32_t k;if(settings_load(&pf,sizeof pf)!=0 || pf.magic!=PF_MAGIC || pf.ver!=1 || pf.slot>=NSLOT)pf_defaults();
     for(k=0;k<NTRK;k++)if(pf.pan[k]>127)pf.pan[k]=64;}
    song_demo(&sg);
    if(slot_load(pf.slot,enc,SONG_ENC_SIZE)==0)song_decode(&sg,enc,SONG_ENC_SIZE);   /* untouched (= the demo) when it fails */
    un_init(&un);
    while(!en.up)platform_poll();
    pf_apply();
    stage_init();
    next_blink=fm1_ms+3000u;full=1;
    fm1_led_key(BTN_FX,1);
    for(;;){
        uint32_t released,b,keys,down,now,tb,sh,oldpos=ppos;
        int32_t dS,dA,dP,dK[4];
        platform_poll();
        now=fm1_ms;
        ppos=en.pos;playing=ppos>>16&1u;
        b=fm1_input_edges(&released);
        dS=fm1_enc_take(ENC_SELECT);dA=fm1_enc_take(ENC_ALGORITHM);dP=fm1_enc_take(ENC_PRESETS);
        for(i=0;i<4;i++)dK[i]=fm1_enc_take(ENC_KNOB1+i);
        keys=fm1_in.notes;down=keys&~prev_keys;
        /* SAVE: tap = save to the slot; held = the slot menu (SELECT: slot, FX load, SCL new, ENV demo) */
        sh=(fm1_in.buttons>>BTN_SAVE)&1u;
        if((sh||(b&(1u<<BTN_SAVE))) && !sv_on){sv_on=1;sv_t=now;sv_c=0;menu_slot=pf.slot;}   /* (own state: a bounce must not restart it) */
        if(sh){
            if(!menu && (now-sv_t>=350u || dS || dP || (b&((1u<<BTN_FX)|(1u<<BTN_SEL)|(1u<<BTN_ENV))))){menu=1;sv_c=1;menu_draw();}
            if(menu){
                if(dS||dP){menu_slot=(uint32_t)wr((int32_t)menu_slot+dS+dP,(int32_t)NSLOT);menu_draw();}
                if(b&(1u<<BTN_FX)){guarded(1,menu_slot);menu_draw();}
                if(b&(1u<<BTN_SEL)){guarded(2,0);menu_draw();}
                if(b&(1u<<BTN_ENV)){guarded(3,0);menu_draw();}
            }
            prev_keys=keys;
            continue;
        }
        if(sv_on){
            sv_on=0;
            if(!sv_c)do_save(menu_slot);
            menu=0;full=1;mk(0x3Fu);
            prev_keys=keys;
            continue;
        }
        if(b&(1u<<BTN_HOME)){if(page==PG_GEN)take=0;page=page+1u<PG_N?page+1u:0;full=1;mk(0x3Fu);}
        for(tb=0;tb<4u;tb++)if(b&(1u<<(BTN_FX+tb))){if(page==PG_GEN && tb==trk)gcf.lock^=(uint8_t)(1u<<tb);trk=tb;mk(0x3Fu);for(i=0;i<4u;i++)fm1_led_key(BTN_FX+i,i==trk);}
        if(b&(1u<<BTN_OCT_UP) && oct<2){oct++;mk(0x21u);}
        if(b&(1u<<BTN_OCT_DOWN) && oct>-2){oct--;mk(0x21u);}
        if(b&(1u<<BTN_PLAY))do_play();
        if(b&(1u<<BTN_SEQ)){stop_play();}
        if(b&(1u<<BTN_GLO)){if(page==PG_SONG||page==PG_PATTERN)do_undo();else if(page==PG_GEN)gen_drop();}
        if(b&(1u<<BTN_ARP)){anchor=cstep;arp_c=0;if(page==PG_SONG && sg.nrows && sg.nrows<NROW){u_pushrows();lk();sg_row_dup(&sg,(int)crow);ulk();crow++;modified=1;mk(0x1Fu);}
            if(page==PG_MIXER){sg.trk[trk].solo^=1u;modified=1;mk(0x1Fu);}
            if(page==PG_GEN)gen_go(1);}
        if(b&(1u<<BTN_REC)){
            if(page==PG_PATTERN){
                uint32_t p=pat_of();Step z={{0,0,0,0}};
                if(p!=ROW_OFF){u_pushpat(trk,p);st_put(trk,p,cstep,&z);mk(0x1Eu);}
            }else if(page==PG_GEN)gen_go(0);
            else if(page==PG_SONG && sg.nrows){u_pushrows();lk();sg_row_delete(&sg,(int)crow);ulk();modified=1;if(crow && crow>=sg.nrows)crow=sg.nrows-1u;mk(0x1Fu);}
        }
        if(b&(1u<<BTN_EDIT)){
            if(page==PG_SONG){
                if(sg.nrows<NROW){u_pushrows();lk();sg_row_insert(&sg,(int)(sg.nrows?crow:0));ulk();modified=1;mk(0x1Fu);}
            }else if(page==PG_PATTERN && (fm1_in.buttons&(1u<<BTN_ARP))){
                uint32_t p=pat_of();
                arp_c=1;
                if(p!=ROW_OFF && clip_n && clip_t==trk){
                    uint32_t n=clip_n<NSTEP-cstep?clip_n:NSTEP-cstep;
                    u_pushpat(trk,p);
                    for(i=0;i<n;i++)st_put(trk,p,cstep+i,&clip[i]);
                    mk(0x1Eu);toast("PASTED",0);
                }else toast(clip_n?"OTHER TRACK":"NOTHING COPIED",1);
            }else if(page==PG_PATTERN){
                uint32_t p=pat_of();
                int d=p==ROW_OFF?-1:sg_dup_pat(&sg,(int)trk,(int)p);
                if(d<0)toast("NO FREE PATTERN",1);
                else{u_pushrows();sg_row_set(&sg,(int)crow,(int)trk,d);modified=1;toast("DUPLICATED",0);mk(0x1Fu);}
            }else if(page==PG_MIXER){sg.trk[trk].mute^=1u;modified=1;mk(0x1Fu);}
            else if(page==PG_GEN && take){take=0;toast("KEPT",0);}
        }
        if((released&(1u<<BTN_ARP)) && !arp_c && page==PG_PATTERN){
            uint32_t p=pat_of(),lo=anchor<cstep?anchor:cstep,hi=anchor<cstep?cstep:anchor;
            if(p!=ROW_OFF && sg_copy(&sg,(int)trk,(int)p,(int)lo,(int)(hi-lo+1u),clip)){clip_n=hi-lo+1u;clip_t=trk;toast("COPIED",0);}
        }
        /* encoders, per page */
        if(page==PG_STAGE){
            if(dS&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dS,0,(int32_t)sg.nrows-1);mk(0x20u);}
            if(dA&&trk){tweak(&sg.trk[trk].preset,dA,0,(int32_t)npst[trk]-1);mk(0x20u);}
            if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(0x20u);}
            {uint8_t *v[4]={&sg.trk[0].vib,&sg.trk[0].porta,&sg.trk[0].breath,&sg.trk[0].bright};
             for(i=0;i<4;i++)if(dK[i])tweak(v[i],dK[i]*2,0,100);}
        }else if(page==PG_SONG){
            if(dS&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dS,0,(int32_t)sg.nrows-1);mk(0x1Fu);}
            if(sg.nrows && crow<sg.nrows){
                if(dA){u_pushrows();sg_row_sect(&sg,(int)crow,wr((int32_t)sg.row[crow].sect+dA,SEC_N));modified=1;mk(0x1Fu);}
                if(dP){
                    int32_t v=(int32_t)sg.row[crow].pat[trk];
                    if(v==(int32_t)ROW_OFF)v=-1;
                    v=cl(v+dP,-1,NPAT-1);
                    u_pushrows();sg_row_set(&sg,(int)crow,(int)trk,v<0?(int)ROW_OFF:v);modified=1;mk(0x1Fu);
                }
            }
            if(dK[0]){sg.loop=dK[0]>0;modified=1;mk(1u);}
            if(dK[1]){tweak(&sg.bpm,dK[1],40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(1u);}
            if(dK[2]){sg.transpose=(int8_t)cl(sg.transpose+dK[2],-24,24);modified=1;mk(1u);}
            if(dK[3]){tweak(&sg.master,dK[3]*2,0,127);mk(1u);}
        }else if(page==PG_PATTERN){
            if(dS){cstep=(uint32_t)wr((int32_t)cstep+dS,NSTEP);mk(0x1Fu);}
            if(dA)ctl(2,dA);
            if(dP&&crow<sg.nrows){
                int32_t v=(int32_t)pat_of();
                if(v==(int32_t)ROW_OFF)v=-1;
                v=cl(v+dP,-1,NPAT-1);
                u_pushrows();sg_row_set(&sg,(int)crow,(int)trk,v<0?(int)ROW_OFF:v);modified=1;mk(0x1Fu);
            }
            if(dK[0])ctl(0,dK[0]);
            if(dK[1])ctl(1,dK[1]);
            if(dK[2] && trk!=TR_DRUMS){
                uint32_t p=pat_of();
                if(p!=ROW_OFF){u_pushpat(trk,p);lk();i=(uint32_t)sg_transpose(&sg,(int)trk,(int)p,dK[2]>0?1:-1);ulk();modified|=i;mk(0x1Eu);if(!i)toast("OUT OF RANGE",1);}
            }
            if(dK[3]&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dK[3],0,(int32_t)sg.nrows-1);mk(0x1Fu);}
        }else if(page==PG_GEN){
            if(dS&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dS,0,(int32_t)sg.nrows-1);mk(1u);}
            if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(4u);}
            if(dK[0]){gcf.style=(uint8_t)wr(gcf.style+dK[0],GS_N);mk(2u);}
            if(dK[1]){gcf.key=(uint8_t)wr(gcf.key+dK[1],12);mk(2u);}
            if(dK[2]){gcf.scale^=1u;mk(2u);}
            if(dA||dK[3]){gcf.seed=(uint16_t)(gcf.seed+dK[3]+dA*100);mk(4u);}
        }else{
            if(dS){trk=(uint32_t)wr((int32_t)trk+dS,NTRK);mk(0x1Fu);}
            if(dA&&trk){tweak(&sg.trk[trk].preset,dA,0,(int32_t)npst[trk]-1);mk(0x1Fu);}
            if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(1u);}
            if(dK[0]){tweak(&sg.trk[trk].level,dK[0]*2,0,127);mk(0x1Fu);}
            if(dK[1]){pf.pan[trk]=(uint8_t)cl(pf.pan[trk]+dK[1]*3,0,127);en.pan[trk]=pf.pan[trk];mk(0x1Fu);}
            if(dK[2]){tweak(&sg.swing,dK[2],0,75);mk(1u);}
            if(dK[3]){tweak(&sg.master,dK[3]*2,0,127);mk(1u);}
        }
        /* keys: always sound the current track's instrument; on PATTERN they also write the cursor step */
        if(down){
            uint32_t k;
            for(k=0;k<27u;k++)if(down&(1u<<k)){
                int32_t n=cl((int32_t)kbase[trk]+(int32_t)k+oct*12,1,127);
                kn[k]=(uint8_t)n;
                if(trk==TR_DRUMS){if(k<6u)en_post(&en,EN_ON,trk,k,0);else if(k<12u)en_post(&en,EN_ON,trk,k-6u,0);}
                else en_post(&en,EN_ON,trk,(uint32_t)n,ed_mora);
                if(page==PG_PATTERN)kwrite(k,(uint32_t)n);
            }
            pv_off=0;
        }
        if(prev_keys&~keys){
            uint32_t k;
            if(trk==TR_CHORDS){for(k=0;k<27u;k++)if((prev_keys&~keys)&(1u<<k))en_post(&en,EN_OFF,trk,kn[k],0);}
            else if(!keys && trk!=TR_DRUMS)en_post(&en,EN_OFF,trk,0,0);
        }
        prev_keys=keys;
        if(pv_off && (int32_t)(now-pv_off)>=0){pv_off=0;if(!keys)en_post(&en,EN_OFF,0,0,0);}
        /* what the engine tells us */
        if(ppos!=oldpos||playing!=(oldpos>>16&1u)){
            if(page==PG_STAGE)mk(0x20u);
            else if(page==PG_SONG)mk(0x1Eu);
            else if(page==PG_PATTERN && (ppos>>8&255u)==crow)mk(0x0Eu);
            else if(page==PG_PATTERN)mk(1u);
        }
        if(en.act!=last_act){last_act=en.act;if(page==PG_STAGE)mk(0x20u);}
        if(toast_s && (int32_t)(now-toast_until)>=0){toast_s=0;mk(0x21u);}
        if(page==PG_STAGE)face_update();
        else if((int32_t)(now-next_blink)>=0)next_blink=now+3000u;
        if(now-cpu_ms>=500u){
            uint32_t p=pub_peak*100u/BUDGET_TICKS;
            cpu_ms=now;
            if(p!=cpu_pct){cpu_pct=p;if(!page)mk(0x20u);}
        }
        if(dirty||full)redraw();
    }
}

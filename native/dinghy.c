/* SPDX-License-Identifier: GPL-3.0-only
 * MIKU-FM1 native: a singing songwriter for the FM-1 on Dinghy. Four tracks (VOCAL, CHORDS, BASS, DRUMS), patterns of 16 steps,
 * an arrangement of rows, 8 song slots. Pages: STAGE (the singer's face), DANCE (the singer, full body), SONG, PATTERN, MIXER, GEN (the song generator). Unofficial fan project, not a voicebank.
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
#ifndef MIKU_UVC
#define MIKU_UVC 0
#endif
#ifndef MIKU_VIDEO_BENCH
#define MIKU_VIDEO_BENCH 0
#endif
#if MIKU_VIDEO_BENCH && !MIKU_UVC
#error "video benchmark requires the isolated video build"
#endif
#include "platform.c"
/* Both shipped FM-1 flash variants must retain real persistent storage. */
_Static_assert(FL_JEDEC_OK(0x856014u),"Puya FM-1 flash unsupported");
_Static_assert(FL_JEDEC_OK(0xB36014u),"UC25HQ80 FM-1 flash unsupported");
_Static_assert(!FL_JEDEC_OK(0xFFFFFFu),"unknown flash must remain rejected");
#include "firmware/audio/fvoice/fvoice.c"
#include "firmware/ui/singer_sprites.h"
#include "firmware/ui/sprite_codec.c"
#include "firmware/ui/wardrobe_sprites.h"
#include "firmware/ui/performance.h"
_Static_assert(WD_OUTFIT_N==PERF_OUTFIT_N,"wardrobe metadata mismatch");
#include "firmware/ui/stagefx.c"
#include "firmware/ui/performance.c"
#include "firmware/ui/scene_prefs.c"
#if MIKU_UVC
#define MIKU_COMPACT_SPRITES 1
#include "firmware/usb/jpeg/mjpeg.c"
#include "firmware/usb/jpeg/performance_frame.c"
#endif
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
#define GEN_NEXT_NO_STACK_WRAPPER
#include "firmware/song/gen_next.c"
#include "firmware/song/lyric_edit.c"
#include "firmware/song/factory.c"
#define VX_POOL __attribute__((section(".pool")))
#define sat vx_sat
#define clr vx_clr
#include "firmware/audio/vfx/vfx.c"
#undef sat
#undef clr
#include "firmware/app/engine.c"
#define BUDGET_TICKS (5805u*FM1_TICKS_PER_US)       /* 256 frames at 44.1 kHz */
#define BG RGB(8,10,24)
#define UI_BLUE RGB(77,190,255)
#define UI_PINK RGB(255,91,172)
#define UI_EDGE RGB(65,51,100)
#define FACE_Y 184u
enum { PG_STAGE, PG_DANCE, PG_SONG, PG_PATTERN, PG_MIXER, PG_GEN, PG_LYRICS, PG_VISUALS, PG_LIBRARY, PG_N };
static Engine en;
static Song sg;
static Undo un;
static Song gbak;                                   /* GEN: the song before the last take (GLO drops the take) */
static GenCfg gcf={GS_POP,0,GK_MAJOR,0,1};
static GenNextWorkspace gen_work __attribute__((section(".pool")));
static uint32_t gen_style_id,gen_energy=70,gen_complexity=60,gen_edit;
#define GEN_CHOICES (GX_N+GS_N)
static const char *native_gen_style(void){return gen_style_id<GX_N?gen_next_style(gen_style_id):gen_style(gen_style_id-GX_N);}
static GenNextCfg native_gen_cfg(void){GenNextCfg c;c.seed=gcf.seed;c.style=(uint8_t)gen_style_id;c.key=gcf.key;c.minor=gcf.scale;c.lock=gcf.lock;c.energy=(uint8_t)gen_energy;c.complexity=(uint8_t)gen_complexity;return c;}

/* STAGE / DANCE: the voice's home. FX / ENV / LFO pick what the four knobs do (press again: back to VOICE), SEL the language,
   SELECT the syllable, ALGORITHM the character. Every value lives in the song (sg.trk[0], sg.vx), so it is saved with it. */
enum { BK_VOICE, BK_FX, BK_ENV, BK_LFO, BK_SRC, BK_FXD, BK_ART, BK_HAR, BK_N };   /* the last four: hold SEL / FX / ENV / LFO */
static uint32_t bank,animal_rate=50,lp_t[4],lp_on[4];                    /* lp_*: when FX/SEL/ENV/LFO went down; 1 pressed, 2 long press done */
static char popbuf[28];
static const char *const bkname[BK_N]={"VOICE","FX","ENV","LFO","SOURCE","FX DEEP","ARTIC","HARMONY"};
static const char *const knm[BK_N][4]={{"BREATH","BRIGHT","GLIDE","LEVEL"},{"CHORUS","DELAY","REVERB","DRIVE"},
    {"ATTACK","RELEASE","SCOOP","HUMAN"},{"VIBRATO","VIB RATE","VIB DELAY","TREMOLO"},{"TENSION","FRY","WHISPER","FORMANT"},
    {"REV SIZE","DLY TIME","FEEDBACK","CHO RATE"},{"CONS LEN","CONS LEVEL","VOWEL SPEED","ON BEAT"},{"INTERVAL","HARM MIX","DETUNE","HARM VOICE"}};
static const char *const ksh[BK_N][4]={{"BRE","BRI","GLI","LEV"},{"CHO","DLY","REV","DRV"},{"ATK","REL","SCP","HUM"},{"DEP","RAT","DLY","TRM"},
    {"TEN","FRY","WHS","FMT"},{"SIZ","TIM","FBK","RAT"},{"LEN","LVL","VOW","BT"},{"INT","MIX","DET","VOX"}};
static const char *const chname[VC_N]={"DEEP","LOW","MIKU","YOUNG","TINY"};
static const char *const lgname[VL_N]={"JAPANESE","ANIMAL","SCAT","VOWELS"};
static uint8_t *kptr(uint32_t b,uint32_t k){
    Voice *v=&sg.vx;
    uint8_t *t[BK_N][4]={{&sg.trk[0].breath,&sg.trk[0].bright,&sg.trk[0].porta,&sg.trk[0].level},{&v->cho,&v->dly,&v->rev,&v->drv},
        {&v->atk,&v->rel,&v->scoop,&v->human},{&sg.trk[0].vib,&v->vrate,&v->vdel,&v->trem},{&v->tens,&v->fry,&v->whsp,&v->fmt},
        {&v->rsize,&v->dtime,&v->dfb,&v->crate},{&v->clen,&v->clev,&v->vspd,&v->obeat},{&v->hint,&v->hmix,&v->hdet,&v->hchr}};
    return t[b%BK_N][k&3u];
}
/* knobs that move in steps: delay time (5 divisions), harmony interval (7, the first = off); harmony voice is a character 0..4 */
static const char *const dtname[5]={"1/16","1/8T","1/8","1/8.","1/4"};
static const char *const hiname[7]={"OFF","-12","+3","+4","+5","+7","+12"};
static uint32_t ksteps(uint32_t b,uint32_t k){return b==BK_FXD&&k==1u?5u:b==BK_HAR&&k==0u?7u:0u;}
static uint32_t kmax(uint32_t b,uint32_t k){return b==BK_VOICE&&k==3u?127u:b==BK_HAR&&k==3u?VC_N-1u:100u;}
static uint32_t modified;
static void tweak(uint8_t *v,int32_t d,int32_t lo,int32_t hi);
static int32_t cl(int32_t v,int32_t lo,int32_t hi);
static void cat(char *d,const char *s);
static void catn(char *d,uint32_t n);
static void kturn(uint32_t b,uint32_t k,int32_t d){
    uint8_t *v=kptr(b,k);
    uint32_t n=ksteps(b,k);
    if(n){int32_t i=cl((int32_t)(*v*n/101u)+(d>0?1:-1),0,(int32_t)n-1);*v=(uint8_t)(((uint32_t)i*101u+n-1u)/n);modified=1;}   /* the step's first value */
    else tweak(v,kmax(b,k)>4u?d*2:d,0,(int32_t)kmax(b,k));
}
static void ktext(uint32_t b,uint32_t k,char *o){          /* the value as shown */
    uint32_t v=*kptr(b,k),n=ksteps(b,k);
    o[0]=0;
    if(n)cat(o,n==5u?dtname[v*5u/101u]:hiname[v*7u/101u]);
    else if(b==BK_HAR&&k==3u)cat(o,chname[v%VC_N]);
    else catn(o,v);
}
static uint32_t take,take_mod,gvar;
static uint8_t enc[SONG_ENC_SIZE];                 /* slot I/O buffer */
static ScenePrefs pf;   /* settings: 0.3 saved 37 bytes, so its data never loads here */
#define PF_MAGIC 0x4D4B5632u
static volatile uint32_t pub_peak;
static Step clip[NSTEP];
static uint32_t clip_n,clip_t,anchor,arp_c;
static int32_t oct;
static uint32_t library_cursor;
static uint32_t page,trk,crow,cstep,top,playing,ppos,prev_keys,dirty,full,cpu_ms,cpu_pct,last_act,
    pend,pend_t,pend_arg,menu,menu_slot,sv_t,sv_c,sv_on,toast_until,toast_err,pv_off,u_kind,u_trk,u_pat,u_ms,lk_t0,
    face_cur,face_ms,blink_until,next_blink,happy_until;
static const char *toast_s;
static uint32_t ed_len[3]={2,8,2},ed_vel=12,ed_mora,ed_q,ed_mode,ed_bf;
static uint8_t kn[27],hw[2u*SINGER_FACE_H];
static uint16_t pw[16],dpw[16],c_bg,c_disc;
static uint32_t dance_cur=99,dance_ms;
static PerfState performance;
#if MIKU_UVC
/* One compressed frame plus incremental MCU scratch: no video framebuffer or core1 ownership.
   TIMER5 owns USB state; the main loop owns encoder/scene until immutable publication. */
static MjEncoder video_encoder __attribute__((section(".pool")));
static uint8_t video_jpeg[MJ_FRAME_CAP] __attribute__((section(".pool"),aligned(32)));
static PerfState video_scene;
static uint32_t video_encoding,video_started;
static volatile uint32_t video_step_ticks,video_frame_ticks,video_encoded,video_failed,video_last_bytes;
static void video_update(const PerfState *scene){
    uint32_t at,dt;int result;
#if MIKU_VIDEO_BENCH
    if(video_encoded>=3u || !scene->playing || scene->mouth>=5u)return; /* three frames under live vocal+band load */
    if(scene->effect&&scene->effect!=STFX_BEAMS&&scene->now_ms-scene->effect_epoch<480u)return; /* measure the expanded burst, not only launch pixels */
#else
    if(!video_usb_active()){video_encoding=0;return;}
    if(video_usb_busy())return;
#endif
    if(!video_encoding){
        volatile uint8_t *to=(volatile uint8_t *)&video_scene;
        const uint8_t *from=(const uint8_t *)scene;uint32_t i;
        for(i=0;i<sizeof video_scene;i++)to[i]=from[i];
        if(!mj_begin(&video_encoder,MUVC_WIDTH,MUVC_HEIGHT,65,video_jpeg,sizeof video_jpeg,mj_perf_pixel,&video_scene)){video_failed++;return;}
        video_encoding=1;video_started=fm1_ticks();
    }
    at=fm1_ticks();result=mj_step(&video_encoder,1); /* <=one16x16 tile per poll pass */
    dt=fm1_ticks()-at;if(dt>video_step_ticks)video_step_ticks=dt;
    if(result==0)return;
    video_encoding=0;dt=fm1_ticks()-video_started;if(dt>video_frame_ticks)video_frame_ticks=dt;
    if(result<0){video_failed++;return;}
    video_last_bytes=mj_size(&video_encoder);
#if MIKU_VIDEO_BENCH
    video_encoded++;
#else
    if(video_usb_publish(video_jpeg,mj_size(&video_encoder)))video_encoded++;
#endif
}
#endif
static const uint16_t tcol[NTRK]={RGB(247,123,180),RGB(35,182,176),RGB(249,207,94),RGB(120,150,250)};
static const char *const tname[NTRK]={"VOCAL","CHORDS","BASS","DRUMS"};
static const char *const sname[SEC_N]={"","INTRO","VERSE","CHORUS","BRIDGE","OUTRO"};
static const char *const cqn[10]={"MAJ","MIN","7","MAJ7","MIN7","SUS4","SUS2","DIM","AUG","ADD9"};
static const char *const cmn[8]={"BLOCK","ARP UP","ARP DN","ARP UD","STRUM","UP 32","DN 32","UD 32"};
static const char *const dnn[6]={"KICK","SNARE","CLAP","CHAT","OHAT","PERC"};
static const char *const pnm[NTRK][8]={{"VOICE"},{"BELLS","E.PIANO","CHIP ARP","PAD","ORGAN","PLUCK","MARIMBA","CLAV"},{"FM BASS","SUB","CHIP SQ","ACID","SUB DROP","HARD ACID"},{"808","CHIP","PUNCH","DIGITAL"}};
static const uint8_t kbase[NTRK]={53,41,29,0};     /* key 0 (F3) plays this note (before OCT) */
static const uint8_t npst[NTRK]={1,CH_NPRESET,BS_NPRESET,DR_NPRESET};
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
static void stage_init(void);
static void pf_defaults(void){sp_defaults(&pf);}
static void scene_load(uint32_t slot){
    if(slot<SP_SLOTS)perf_scene_decode(&performance,pf.scene[slot]);
    stage_init();face_cur=SF_NEUTRAL;dance_cur=99;
}
static void pf_apply(void){uint32_t i;for(i=0;i<NTRK;i++)en.pan[i]=pf.pan[i];}
static void after_load(void){
    crow=cstep=top=0;un_init(&un);u_kind=0;modified=0;take=0;
    en_post(&en,EN_PANIC,0,0,0);
    mk(0x3Fu);full=1;
}
static uint32_t sload(uint32_t s){                         /* a song in slot s into enc: its size (v3, v2 or v1), 0 = none */
    if(!slot_load(s,enc,SONG_ENC_SIZE))return SONG_ENC_SIZE;
    if(!slot_load(s,enc,SONG_ENC_V2))return SONG_ENC_V2;
    if(!slot_load(s,enc,SONG_ENC_V1))return SONG_ENC_V1;
    return 0;
}
static void stop_wait(void){if(playing){en_post(&en,EN_STOP,0,0,0);wait_ms(15);}}
static uint32_t slot_name(uint32_t s,char *nm){            /* 1 = a valid song is in the slot */
    uint32_t i;
    nm[0]=0;
    if(!sload(s)){
        const char *name=sf_name(s);if(!name)return 0;
        for(i=0;i<8u&&name[i];i++)nm[i]=name[i];
        nm[i]=0;return 1;
    }
    if(enc[0]!='M'||enc[1]!='K'||enc[2]!='S'||enc[3]!='2')return 0;
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
    if(ok){pf.slot=(uint8_t)s;perf_scene_encode(&performance,pf.scene[s]);ok=settings_save(&pf,sizeof pf)==0;}
    if(ok)modified=0;
    toast(ok?"SAVED":"SAVE FAILED",!ok);
}
static void do_load(uint32_t s){
    stop_wait();
    uint32_t n=sload(s);
    if(!n){
        if(s<SF_COUNT){lk();sf_load(&sg,s);ulk();pf.slot=(uint8_t)s;scene_load(s);after_load();toast("FACTORY SONG",0);}
        else toast("EMPTY SLOT - EDIT NEW",1);
        return;
    }
    lk();
    {int r=song_decode(&sg,enc,n);ulk();
     if(r!=SIO_OK){toast("LOAD FAILED",1);return;}}
    pf.slot=(uint8_t)s;scene_load(s);after_load();toast("LOADED",0);
}
static void do_new(void){
    stop_wait();lk();
    sg_init(&sg);sg_row_insert(&sg,0);
    {uint32_t t;for(t=0;t<NTRK;t++)sg_row_set(&sg,0,(int)t,0);}
    ulk();after_load();toast("NEW SONG",0);
}
static void do_demo(void){stop_wait();lk();song_demo(&sg);ulk();after_load();toast("DEMO LOADED",0);}
static void guarded(uint32_t act,uint32_t arg){            /* actions that drop the current song need a second press when it is unsaved */
    if(modified && !(pend==act && pend_arg==arg && (int32_t)(fm1_ms-pend_t)<0)){pend=act;pend_arg=arg;pend_t=fm1_ms+3000u;toast("UNSAVED! AGAIN",1);return;}
    pend=0;
    if(act==1)do_load(arg);else if(act==2)do_new();
    else if(act==5){stop_wait();lk();sf_load(&sg,arg);ulk();pf.slot=(uint8_t)arg;sp_reset_scene(&pf,arg);scene_load(arg);after_load();modified=1;toast("FACTORY RESTORED",0);}
    else if(act==6){do_new();pf.slot=(uint8_t)arg;scene_load(arg);modified=1;}
    else do_demo();
}
/* ---- face: 4bpp sprite rows, 2x, into the canvas buffer, blitted (the panel wants bytes swapped: swap16) */
#define FX 30u
#define FY 2u
static void face_draw(uint32_t f,uint32_t sx,uint32_t sy,uint32_t w,uint32_t h){
    uint8_t row[SINGER_FACE_W];
    uint32_t cr=CV_MAX/(4u*w),r0,r,c,k;
    cv_oy=0;
    for(r0=sy;r0<sy+h;r0+=cr){
        uint32_t nr=sy+h-r0<cr?sy+h-r0:cr;
        lcd_sync();
        for(r=0;r<nr;r++){
            uint16_t *o=cv_px+r*2u*2u*w;
            int32_t hh=hw[2u*(r0+r)];
            if(!spr_decode_row(WD_OUTFITS[performance.outfit].faces,f,r0+r,row,sizeof row))return;
            for(c=0;c<w;c++){
                uint32_t x=sx+c,i=row[x];
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
    const uint8_t *p=WD_OUTFITS[performance.outfit].face_dirty+4u*a,*q=WD_OUTFITS[performance.outfit].face_dirty+4u*f;
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
    for(i=0;i<16;i++){pw[i]=swap16(WD_OUTFITS[performance.outfit].face_palette[i]);dpw[i]=swap16(WD_OUTFITS[performance.outfit].stage_palette[i]);}
    c_bg=swap16(RGB(12,20,40));c_disc=swap16(RGB(40,66,120));
    for(i=0;i<2u*SINGER_FACE_H;i++){        /* half-width of a disc (r=90) behind the head, per output row */
        int32_t dy=(int32_t)i-90;
        v=(uint32_t)(8100-dy*dy);h=0;
        while((h+1u)*(h+1u)<=v)h++;
        hw[i]=(uint8_t)h;
    }
}
/* ---- DANCE: the full-body singer at 3x (183 x 213) above a one-line strip. One frame per beat while playing: the two
   arms up on every other beat of a CHORUS row, the two singing frames while the vocal sounds, else the four-frame sway; a bow when the
   song stops; a slow sway when stopped. A frame is redrawn only when it changes. */
#define DX 28u
static void dance_draw(uint32_t f){
    uint8_t row[SINGER_STAGE_W];uint32_t y,x,k,r,n;
    cv_oy=0;
    for(y=0;y<214u;y+=4u){
        n=214u-y<4u?214u-y:4u;lcd_sync();
        for(r=0;r<n;r++){
            if(y+r<213u && !spr_decode_row(WD_OUTFITS[performance.outfit].stage,f,(y+r)/3u,row,sizeof row))return;
            for(x=0;x<240u;x++){
                uint16_t p=perf_pixel(&performance,x,y+r);
                if(y+r<213u&&x>=DX&&x<DX+183u){k=row[(x-DX)/3u];if(k)p=WD_OUTFITS[performance.outfit].stage_palette[k];}
                cv_px[r*240u+x]=swap16(p);
            }
        }
        lcd_blit(0,y,240,n,cv_px);
    }
}
static void dance_update(void){
    uint32_t now=fm1_ms,f=performance.sprite;
    if((f!=dance_cur || performance.effect || performance.background || performance.frame) && (dance_cur==99u || now-dance_ms>=100u)){dance_ms=now;dance_cur=f;dance_draw(f);}
}
/* ---- pages. The screen below the face is drawn in bands of 48 lines (bit n of `dirty` = band n; bit 5 = the STAGE strip) */
static uint32_t shown_row(void){return playing?(ppos>>8&255u):crow;}
/* Shared integer-only chrome. Gradients are procedural, with no new buffers. */
static void ui_panel(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t selected){
    int32_t k;uint32_t v;
    for(k=0;k<h;k+=4){v=(uint32_t)k*8u/(uint32_t)h;cv_rect(x,y+k,w,h-k<4?h-k:4,RGB(12+v+(selected?7u:0u),16+v,32+v*2u));}
    cv_rect(x,y,w,1,selected?UI_BLUE:UI_EDGE);cv_rect(x,y,1,h,selected?UI_BLUE:UI_EDGE);
    cv_rect(x,y+h-1,w,1,selected?UI_PINK:UI_EDGE);cv_rect(x+w-1,y,1,h,UI_EDGE);
}
static void ui_rim(int32_t y,int32_t h){
    cv_rect(0,y,1,h,UI_BLUE);cv_rect(239,y,1,h,UI_PINK);
    cv_rect(0,y,120,1,UI_BLUE);cv_rect(120,y,120,1,UI_PINK);
    cv_rect(0,y+h-1,120,1,UI_PINK);cv_rect(120,y+h-1,120,1,UI_BLUE);
}
static void band_begin(uint32_t b){cv_begin(240,48,BG);cv_oy=-(int32_t)(b*48u);if(!b)ui_panel(1,0,238,48,1);}
static void band_end(uint32_t b){cv_oy=0;cv_blit(0,b*48u);}
static void txt(int32_t x,int32_t y,const felucca_font_t *f,const char *s,uint16_t c,int al){
    int32_t w=text_w(f,s);
    cv_text(al==1?x-w/2:al==2?x-w:x,y,f,s,c);
}
static void cell(int32_t x,int32_t y,int32_t w,int32_t h,const char *s,uint16_t fg,uint16_t bg){
    cv_rect(x,y,w,h,bg);cv_rect(x,y,w,1,UI_BLUE);cv_rect(x,y+h-1,w,1,UI_PINK);txt(x+w/2,y+(h-16)/2,&FONT_S,s,fg,1);
}
static void head(const char *name,const char *l2,uint16_t c2){
    char s[12];
    uint32_t t=fm1_ms;
    if(toast_s && (int32_t)(t-toast_until)<0)txt(4,0,&FONT_L,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),0);
    else{
        txt(4,0,&FONT_L,name,UI_BLUE,0);
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
        ui_panel(2,y,236,24,r==crow);
        s[0]=0;if(r<9u)cat(s," ");catn(s,r+1u);
        txt(6,y+4,&FONT_S,s,(playing && (ppos>>8&255u)==r)?RGB(249,207,94):RGB(150,160,190),0);
        if(playing && (ppos>>8&255u)==r)cv_rect(0,y+2,3,20,RGB(249,207,94));
        txt(34,y+4,&FONT_S,sname[sg.row[r].sect],UI_BLUE,0);
        {uint32_t t;for(t=0;t<NTRK;t++){
            uint32_t p=sg.row[r].pat[t];
            s[0]=0;if(p==ROW_OFF)cat(s,"--");else catn(s,p+1u);
            if(r==crow && t==trk)cell(110+(int32_t)t*32,y+1,30,22,s,BG,tcol[t]);
            else cell(110+(int32_t)t*32,y+1,30,22,s,p==ROW_OFF?RGB(80,90,120):tcol[t],RGB(17,19,37));
        }}
    }
}
static void roll(void){
    uint32_t p=pat_of(),i,lo=127,hi=0,ph=playing && (ppos>>8&255u)==crow?(ppos&255u):99u;
    int32_t cw=15;
    for(i=0;i<NSTEP;i++){                                  /* beats and the cursor column */
        cv_rect((int32_t)i*cw,52,cw-1,136,(i&3u)?RGB(13,15,31):RGB(24,23,46));
        if(i==cstep)cv_rect((int32_t)i*cw,52,cw-1,136,RGB(47,39,82));
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
        ui_panel(3,y+1,234,45,t==trk);
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
/* ---- STAGE visuals, in the 240 x 56 strip under the face.
   The syllable dial: the top of a big dial rises from the bottom edge; syllables sit round its rim with a tick each, the pink
   pointer marks the one the keys sing, and the dial turns (animated) with SELECT. A knob turn shows, for 1.5 s, a ring gauge
   (the value as a lit arc), the knob's name and a picture of what it does (envelope, pitch, LFO, echoes, clip curve ...). */
enum { VS_NONE, VS_KNOB, VS_CHR, VS_LANG, VS_BPM };
static uint32_t vis_kind,vis_id,dial_ms;
static int32_t dial_q=-1;                                  /* the dial's shown position, syllables Q8 (eases toward ed_mora) */
#define C_PNK UI_PINK
#define C_TEL UI_BLUE
#define C_DIMR RGB(60,72,110)
#define C_PNL RGB(24,23,46)
static int32_t isin(int32_t a){return fv_sin[a&255];}     /* 256 steps a turn, Q15 */
static int32_t icos(int32_t a){return fv_sin[(a+64)&255];}
static uint32_t hsh(uint32_t i){i*=2654435761u;return i>>24;}
static void dial(void){
    int32_t a,m,k,cm=(dial_q+128)>>8;
    for(a=44;a<=84;a++)cv_rect(120+((150*icos(a))>>15),170-((150*isin(a))>>15),2,2,C_DIMR);   /* the rim (its top, clear of the line below) */
    for(k=-3;k<=3;k++){
        int32_t mm=cm+k,ang=(64*256-(mm*256-dial_q)*8+128)>>8,x,y,w;
        const char *ms;
        char up[6];
        uint32_t i;
        if(ang<45||ang>83)continue;
        m=wr(mm,MORA_N);ms=mora_str((uint32_t)m);if(!*ms)ms=".";
        for(i=0;i<3u;i++){int32_t r=144+(int32_t)i*3;cv_rect(120+((r*icos(ang))>>15),170-((r*isin(ang))>>15),2,2,k?C_DIMR:C_PNK);}   /* tick */
        x=120+((166*icos(ang))>>15);y=170-((166*isin(ang))>>15);
        if(!k){
            for(i=0;i<5u&&ms[i];i++)up[i]=(char)(ms[i]>='a'&&ms[i]<='z'?ms[i]-32:ms[i]);
            up[i]=0;w=text_w(&FONT_S,up);
            cv_rect(x-w/2-5,y-10,w+10,19,C_PNK);cv_text(x-w/2,y-9,&FONT_S,up,RGB(12,20,40));
        }else{w=text_w(&FONT_S,ms);cv_text(x-w/2,y-9,&FONT_S,ms,k==-1||k==1?RGB(190,200,230):k==-2||k==2?RGB(120,132,170):C_DIMR);}
    }
    for(k=0;k<5;k++)cv_rect(120-4+k,26+k,9-2*k,1,C_PNK);  /* the pointer, under the rim */
}
static void ring(int32_t cx,int32_t cy,int32_t r,int32_t v,int32_t mx){   /* 270-degree gauge, lit up to v of mx */
    int32_t t;
    for(t=0;t<=48;t++){
        int32_t a=160-t*4;
        cv_rect(cx+((r*icos(a))>>15)-1,cy-((r*isin(a))>>15)-1,3,3,t*mx<=v*48&&v?C_PNK:C_DIMR);
    }
}
static int32_t fpeaks(int32_t i,int32_t pct){             /* three formant peaks, positions scaled by pct % */
    static const int16_t pk[3]={34,74,124},ht[3]={30,24,16};
    int32_t j,best=0;
    for(j=0;j<3;j++){int32_t c=pk[j]*pct/100,dd=i-c,h=ht[j]-(dd<0?-dd:dd)*2/3;if(h>best)best=h;}
    return best;
}
static void graph(uint32_t id,int32_t v){                  /* id: bank*4+knob (0..31), 40 character, 41 language, 42 tempo; v 0..100 */
    enum{GX=64,GY=20,GW=172,GH=34,MID=17};
    int32_t i,p=-1,acc=32767,acc2=0;
    cv_rect(GX,GY,GW,GH,C_PNL);
    if(id==41){static const char *const ex[VL_N]={"ka ki ku ke ko","ba-mi-ko-ne-pu!","doo-ba da-bi","a  i  u  e  o"};cv_text(GX+8,GY+9,&FONT_S,ex[sg.vx.lang%VL_N],C_TEL);return;}
    for(i=0;i<GW;i++){
        int32_t y=-1,fill=0,y2=-1;
        switch(id){
        case 0: fill=1;y=(int32_t)hsh((uint32_t)i)*v*(GH-2)/25600;break;                          /* breath: noise */
        case 1: if(i%12<8){int32_t n=i/12,h=32767,j,r=14745+v*164;for(j=0;j<n;j++)h=(h*r)>>15;y=(GH-2)*h>>15;fill=1;}break;   /* bright: harmonics */
        case 2: if(i<30)y=5;else{acc-=acc/(1+v*v/80);y=GH-5-((GH-10)*acc>>15);}break;           /* glide: pitch into the note */
        case 3: if(i*100<v*GW&&(i&3)!=3){y=GH-1;fill=1;}break;                                  /* level: a meter */
        case 4: y=MID+((10*isin(i*3))>>15);y2=MID+((10*isin(i*3+v*64/100))>>15);break;          /* chorus: two voices drifting apart */
        case 5: {int32_t n=i/26;if(i%26<3){int32_t h=GH-2,j,fb=6000+v*90;if(n){h=h*v/100;for(j=1;j<n;j++)h=h*fb>>15;}y=h;fill=1;}}break;   /* delay: the note, then its echoes */
        case 6: if(i&1){if(i>1)acc-=acc/(4+v*v/50);y=(GH-2)*v/100*acc>>15;fill=1;}break;       /* reverb: a dense tail */
        case 7: {int32_t u=(i-GW/2)*65536/GW,g=16+v*v*13/1000;u=u*g>>4;u=u>32767?32767:u<-32767?-32767:u;    /* drive: the clip curve */
                 u=(3*u-((((u*u)>>15)*u)>>15))>>1;y=MID+((u*(MID-2))>>15);}break;
        case 8: {int32_t at=2+v*v/70;y=i<at?(GH-3)*i/at:i<GW-24?GH-3:(GH-3)*(GW-i)/24;}break;   /* attack */
        case 9: if(i<4)y=(GH-3)*i/4;else if(i<40)y=GH-3;else{acc-=acc/(1+v*v/70);y=(GH-3)*acc>>15;}break;   /* release */
        case 10: if(i<10)y=MID-(v*(MID-3))/100;else{acc-=acc/9;y=MID-((v*(MID-3)/100)*acc>>15);}break;   /* scoop: from below */
        case 11: y=MID+((v*(MID-3)/100)*((2*isin(i*2)+isin(i*5+40))/3)>>15);break;          /* human: a slow wander */
        case 12: y=MID+((v*(MID-3)/100)*isin(i*6)>>15);break;                                 /* vibrato depth */
        case 13: y=MID+(10*isin(i*(2+v/7))>>15);break;                                        /* vibrato rate */
        case 14: {int32_t d=v*GW/130;if(i<d)y=MID;else{acc2+=acc2<32767-1100?1100:0;y=MID+(((10*acc2)>>15)*isin((i-d)*6)>>15);}}break;   /* vibrato delay: flat, then it fades in */
        case 15: fill=1;y=(GH-2)-(GH-2)*v/100*((32768+isin(i*5))>>1)/32768;break;           /* tremolo */
        case 40: {static const uint8_t f[VC_N]={84,92,100,108,118};y=fpeaks(i,f[sg.vx.chr%VC_N]);fill=1;}break;   /* character: formant peaks move */
        case 42: fill=1;y=(i%(600/(v?v:1)+1)<2)?GH-4:-1;break;                              /* tempo: beats */
        /* SOURCE */
        case 16: {int32_t x=i%86,cl2=36-v*30/100;                                             /* tension: one glottal pulse, its close sharper */
                  y=x<50?(GH-3)*(32768-icos(x*128/50))/65536:x<50+cl2?(GH-3)*(32768+icos((x-50)*128/cl2))/65536:0;}break;
        case 17: if(i%20<4){fill=1;y=(i/20)&1?(GH-2)*(100-v*7/10)/100:GH-2;}break;          /* fry: every other pulse weaker */
        case 18: y=MID+(((100-v)*12/100)*isin(i*6)>>15)+((int32_t)(hsh((uint32_t)i)%25u)-12)*v/100;break;   /* whisper: the wave into noise */
        case 19: y=fpeaks(i,100+(v-50)*36/100);fill=1;break;                                  /* formant fine shift */
        /* FX DEEP */
        case 20: if(i&1){if(i>1)acc-=acc/(2+v*v/40);y=(GH-4)*acc>>15;fill=1;}break;           /* reverb size: the tail */
        case 21: {static const uint8_t sp[5]={11,15,22,33,44};int32_t q=sp[v*5/101];if(i%q<3){fill=1;y=i<q?GH-2:(GH-2)/2;}}break;   /* delay time */
        case 22: {int32_t n=i/24;if(i%24<3){int32_t h=GH-2,j,fb=6000+v*180;for(j=0;j<n;j++)h=h*fb>>15;y=h;fill=1;}}break;   /* feedback */
        case 23: y=MID+(10*isin(i*(1+v/10))>>15);y2=MID+(10*isin(i*(1+v/10)+40)>>15);break;  /* chorus rate */
        /* ARTICULATION */
        case 24: {int32_t w=40*(25+v*3/2)/100;if(i<w){fill=1;y=(int32_t)hsh((uint32_t)i)%(GH/2);}else y=MID+(12*isin((i-w)*8)>>15);}break;   /* consonant length */
        case 25: if(i<40){fill=1;y=(int32_t)hsh((uint32_t)i)*(v*2)/100*(GH-2)/512;}else y=MID+(10*isin(i*8)>>15);break;   /* consonant level */
        case 26: if(i<20)y=4;else{acc-=acc/(1+(100-v)*(100-v)/200);y=GH-4-((GH-8)*acc>>15);}break;   /* vowel speed: how fast the vowel arrives */
        case 27: {int32_t vx2=86+(100-v)*40/100;                                              /* on-beat: the vowel moves onto the beat */
                  if(i==86||i==87){fill=1;y=GH-1;}else if(i<vx2&&i>=vx2-40){fill=1;y=(int32_t)hsh((uint32_t)i)%(GH/2);}else if(i>=vx2)y=MID+(12*isin((i-vx2)*8)>>15);}break;
        /* HARMONY */
        case 28: {static const int8_t st[7]={0,-12,3,4,5,7,12};int32_t sm=st[v*7/101];y=MID-4;   /* interval: the note and its harmony */
                  if(sm){y2=MID-4+sm*3/2;y2=y2<0?0:y2>GH-2?GH-2:y2;}}break;
        case 29: fill=1;y=i<86?GH-2:(GH-2)*v/100;break;                                      /* harmony mix: main, harmony */
        case 30: y=MID+((6*isin(i*6)+6*isin(i*6+(i*v)/40))>>15);break;                        /* detune: the beat between two voices */
        case 31: {static const uint8_t f[VC_N]={84,92,100,108,118};y=fpeaks(i,f[sg.vx.hchr%VC_N]);fill=1;}break;   /* harmony voice */
        }
        if(y<0)continue;
        if(y>GH-1)y=GH-1;
        if(y<0)y=0;
        if(fill)cv_rect(GX+i,GY+GH-1-y,1,y+1,id==0||id==6?C_TEL:C_PNK);
        else{int32_t q=p<0?y:p,lo=y<q?y:q,hi=y<q?q:y;cv_rect(GX+i,GY+GH-1-hi,2,hi-lo+1,C_PNK);p=y;}
        if(y2>=0)cv_rect(GX+i,GY+GH-1-y2,2,2,C_TEL);
    }
}
static void pg_stage(void){
    char s[30];
    uint32_t vis=toast_s && (int32_t)(fm1_ms-toast_until)<0 && toast_s==popbuf && vis_kind;
    cv_begin(240,56,BG);ui_panel(1,0,238,56,0);
    if(vis){
        int32_t v,mx=100;
        const char *nm;
        uint32_t kb2=vis_id>>2,kk=vis_id&3u;
        if(vis_kind==VS_KNOB){v=*kptr(kb2,kk);mx=(int32_t)kmax(kb2,kk);nm=knm[kb2%BK_N][kk];ktext(kb2,kk,s);}
        else if(vis_kind==VS_CHR){v=sg.vx.chr;mx=VC_N-1;nm="CHARACTER";s[0]=0;catn(s,sg.vx.chr+1u);}
        else if(vis_kind==VS_LANG){v=(int32_t)sg.vx.lang;mx=VL_N-1;nm=lgname[sg.vx.lang%VL_N];s[0]=0;catn(s,sg.vx.lang+1u);}
        else{v=sg.bpm;mx=240;nm="TEMPO";s[0]=0;catn(s,sg.bpm);}
        ring(30,30,22,v,mx);
        cv_text(30-text_w(&FONT_S,s)/2,22,&FONT_S,s,C_WHITE);
        cv_text(64,1,&FONT_S,vis_kind==VS_CHR?chname[sg.vx.chr%VC_N]:nm,C_TEL);
        if(vis_kind==VS_KNOB && kb2>=BK_SRC)cv_text(236-text_w(&FONT_S,bkname[kb2])-0,1,&FONT_S,bkname[kb2],RGB(150,160,190));
        graph(vis_kind==VS_KNOB?vis_id:vis_kind==VS_CHR?40u:vis_kind==VS_LANG?41u:42u,vis_kind==VS_KNOB?v*100/mx:vis_kind==VS_BPM?v*100/240:v);
    }else{
        dial();
        if(toast_s && (int32_t)(fm1_ms-toast_until)<0)txt(120,38,&FONT_S,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),1);
        else{
            s[0]=0;cat(s,lgname[sg.vx.lang%VL_N]);cat(s," ");cat(s,chname[sg.vx.chr%VC_N]);
            txt(120,38,&FONT_S,s,C_TEL,1);
            cv_text(4,38,&FONT_S,bkname[bank],RGB(150,160,190));
            s[0]=0;catn(s,sg.bpm);cat(s," ");catn(s,cpu_pct);cat(s,"%");
            cv_text(236-text_w(&FONT_S,s),38,&FONT_S,s,RGB(249,207,94));
        }
    }
    ui_rim(0,56);cv_blit(0,FACE_Y);
}
static void pop(uint32_t kind,uint32_t id,const char *name,uint32_t v){   /* what a control just did: the picture on STAGE, text on DANCE */
    popbuf[0]=0;cat(popbuf,name);
    if(kind==VS_KNOB){char t[12];ktext(id>>2,id&3u,t);cat(popbuf," ");cat(popbuf,t);}
    else if(kind!=VS_LANG){cat(popbuf," ");if(kind==VS_CHR)cat(popbuf,chname[v%VC_N]);else catn(popbuf,v);}
    toast(popbuf,0);
    vis_kind=kind;vis_id=id;
}
static void dial_step(void){                               /* ease the dial toward the selected syllable (main loop, STAGE) */
    int32_t t=(int32_t)ed_mora*256,d;
    if(dial_q<0){dial_q=t;return;}
    d=t-dial_q;
    while(d>MORA_N*128){d-=MORA_N*256;}
    while(d<-MORA_N*128){d+=MORA_N*256;}
    if(!d){dial_q=t;return;}
    if(fm1_ms-dial_ms<30u)return;
    dial_ms=fm1_ms;
    dial_q+=d/3?d/3:d;
    if(dial_q<0)dial_q+=MORA_N*256;
    if(dial_q>=MORA_N*256)dial_q-=MORA_N*256;
    mk(0x20u);
}
static uint32_t led_st;
static void bank_leds(void){                              /* FX / ENV / LFO lit for their bank; a held-in layer blinks its button */
    static const uint8_t bt[BK_N]={9,0,2,3,1,0,2,3};
    uint32_t i,on=bank>=BK_SRC?(fm1_ms/250u)&1u:1u,st=0;
    for(i=0;i<4u;i++)st|=(page<=PG_DANCE?bank!=BK_VOICE&&i==bt[bank]&&on:i==trk)<<i;
    if(st==led_st)return;
    led_st=st;
    for(i=0;i<4u;i++)fm1_led_key(BTN_FX+i,(st>>i)&1u);
}
/* ---- GEN: a take is one generate (REC) or one new phrase (ARP); EDIT keeps it, GLO drops it (back to the song before it) */
static void gen_go(uint32_t phrase){
    uint32_t pl=playing,r=1;
    if(phrase && (gcf.lock>>trk&1u)){toast("TRACK LOCKED",1);return;}
    if(!phrase)stop_wait();
    gbak=sg;take_mod=modified;take=1;un_init(&un);u_kind=0;
    lk();
    if(gen_style_id<GX_N){
        GenNextCfg c=native_gen_cfg();
        r=phrase?(uint32_t)gen_next_phrase(&sg,&c,(int)crow,(int)trk,++gvar):(uint32_t)gen_next_song_ws(&sg,&c,&gen_work);
        if(!phrase)gvar=0;
    }else{
        gcf.style=(uint8_t)(gen_style_id-GX_N);
        if(phrase)r=(uint32_t)gen_phrase(&sg,&gcf,(int)crow,(int)trk,++gvar);
        else{gen_song(&sg,&gcf);gvar=0;}
    }
    ulk();
    if(!r){take=0;toast(phrase?"NO EDITABLE PHRASE":"GEN CONTEXT LIMIT",1);return;}
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
    if(b>0u)ui_panel(2,(int32_t)b*48,236,47,b==1u);
    if(b==1u){
        txt(4,52,&FONT_S,"STYLE  K1",RGB(100,110,140),0);
        txt(4,66,&FONT_L,native_gen_style(),UI_BLUE,0);
        txt(236,52,&FONT_S,"KEY K2  K3",RGB(100,110,140),2);
        s[0]=0;cat(s,kn12[gcf.key%12u]);cat(s,gcf.scale?" MIN":" MAJ");
        txt(236,66,&FONT_L,s,RGB(249,207,94),2);
    }else if(b==2u){
        txt(4,100,&FONT_S,gen_edit?"EDIT+K2 ENERGY K3 COMPLEX":"SEED K4   EDIT+K2/K3 SHAPE",RGB(100,110,140),0);
        s[0]=0;if(gen_edit){cat(s,"E");catn(s,gen_energy);cat(s," C");catn(s,gen_complexity);}else catn(s,gcf.seed);txt(4,114,&FONT_L,s,C_WHITE,0);
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
static void pg_dance(void){
    char s[14]="BPM ",r1[16],up[6];
    const char *ms="";
    uint32_t i,r=shown_row(),x=playing?(ppos&255u):cstep,vp=r<sg.nrows?sg.row[r].pat[0]:ROW_OFF;
    if(en.act&1u)ms=mora_str(en.mora);
    else if(!playing && vp<NPAT && sg.pat[0][vp][x].v[0])ms=mora_str(sg.pat[0][vp][x].v[2]);
    for(i=0;i<5u&&ms[i];i++)up[i]=(char)(ms[i]-32);
    up[i]=0;
    num(s+4,sg.bpm);
    r1[0]=0;cat(r1,"R");catn(r1,r+1u);cat(r1," ");cat(r1,r<sg.nrows?sname[sg.row[r].sect]:"");
    cv_begin(240,26,BG);ui_panel(1,0,238,26,0);
    if(toast_s && (int32_t)(fm1_ms-toast_until)<0)txt(120,4,&FONT_S,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),1);
    else{
        cv_text(4,4,&FONT_S,playing?r1:"EDIT: LOOKS",RGB(150,160,190));
        cv_text(120-text_w(&FONT_S,playing?up:"ARP SONGS")/2,4,&FONT_S,playing?up:"ARP SONGS",C_WHITE);
        cv_text(236-text_w(&FONT_S,s),4,&FONT_S,s,RGB(249,207,94));
    }
    ui_rim(0,26);cv_blit(0,214);
}
#include "lyrics_ui.inc"
#include "visuals_ui.inc"
#include "library_ui.inc"

static void redraw(void){
    uint32_t b;
    if(full){
        full=0;
        if(!page){
            lcd_fill(0,0,240,240,RGB(12,20,40));face_draw(face_cur,0,0,SINGER_FACE_W,SINGER_FACE_H);
            for(b=0;b<2u;b++){
                uint32_t x=b?216u:8u,edge=b?230u:8u;uint16_t c=b?UI_PINK:UI_BLUE;
                lcd_fill(x,8,16,2,c);lcd_fill(edge,8,2,16,c);
                lcd_fill(x,FACE_Y-10u,16,2,c);lcd_fill(edge,FACE_Y-24u,2,16,c);
            }
            dirty|=0x20u;
        }
        else if(page==PG_DANCE){lcd_fill(0,0,240,240,RGB(12,20,40));dance_cur=99;dirty|=0x20u;}
        else dirty|=0x1Fu;
    }
    if(!page){if(dirty&0x20u){dirty&=~0x20u;pg_stage();}dirty&=~0x1Fu;return;}
    if(page==PG_DANCE){if(dirty&0x20u){dirty&=~0x20u;pg_dance();}dirty&=~0x1Fu;return;}
    dirty&=~0x20u;
    for(b=0;b<5u;b++)if(dirty&(1u<<b)){
        dirty&=~(1u<<b);
        band_begin(b);
        if(page==PG_SONG)pg_song(b);else if(page==PG_PATTERN)pg_pattern(b);else if(page==PG_GEN)pg_gen(b);else if(page==PG_LYRICS)pg_lyrics(b);else if(page==PG_VISUALS)pg_visuals(b);else if(page==PG_LIBRARY)pg_library(b);else pg_mixer(b);
        if(!b&&(page==PG_LIBRARY||page==PG_LYRICS||page==PG_VISUALS)&&toast_s&&(int32_t)(fm1_ms-toast_until)<0){cv_rect(0,0,240,24,BG);txt(120,4,&FONT_S,toast_s,toast_err?RGB(255,110,110):RGB(249,207,94),1);}
        ui_rim(0,240);band_end(b);
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
    en_post(&en,EN_PLAY,crow,page==PG_PATTERN||page==PG_LYRICS,0);
    happy_until=fm1_ms+500u;
}
static void menu_draw(void){
    char s[24],nm[10];uint32_t b,ok=slot_name(menu_slot,nm);
    for(b=0;b<5u;b++){
        band_begin(b);ui_panel(12,52,216,84,1);ui_panel(12,144,216,62,0);
        s[0]=0;cat(s,"SAVE / SLOT ");catn(s,menu_slot+1u);txt(120,12,&FONT_S,s,UI_BLUE,1);
        txt(120,70,&FONT_L,ok?nm:"EMPTY",C_WHITE,1);
        txt(120,112,&FONT_S,menu_slot==pf.slot?"CURRENT SLOT":"SELECT TO CHANGE",UI_PINK,1);
        txt(120,150,&FONT_S,"FX LOAD   SCL NEW",C_WHITE,1);
        txt(120,174,&FONT_S,"ENV DEMO",C_WHITE,1);
        txt(120,192,&FONT_S,modified?"UNSAVED CHANGES":"",UI_PINK,1);
        txt(120,218,&FONT_S,"RELEASE SAVE: SAVE HERE",UI_BLUE,1);
        ui_rim(0,240);band_end(b);
    }
}
static void app_main(void){
    uint32_t i;
    pf_defaults();
    if(settings_load(&pf,sizeof pf)!=0||!sp_valid(&pf)){
        uint8_t old[12];pf_defaults();if(settings_load(old,sizeof old)==0)sp_migrate(&pf,old);
    }
    if(pf.slot<SF_COUNT)sf_load(&sg,pf.slot);else song_demo(&sg);
    {uint32_t n=sload(pf.slot);if(n)song_decode(&sg,enc,n);}   /* untouched (= the demo) when it fails */
    un_init(&un);
    while(!en.up)platform_poll();
    pf_apply();
    perf_init(&performance);
    scene_load(pf.slot);
    next_blink=fm1_ms+3000u;full=1;
    bank_leds();
    for(;;){
        uint32_t released,b,keys,down,now,tb,sh,kt,oldpos=ppos;
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
        if(b&(1u<<BTN_HOME)){if(page==PG_GEN){take=0;gen_edit=0;}page=page+1u<PG_N?page+1u:0;if(page==PG_LYRICS){trk=TR_VOCAL;lyric_load();}if(page==PG_LIBRARY)library_cursor=pf.slot;full=1;mk(0x3Fu);bank_leds();}
        for(tb=0;tb<4u;tb++){
            uint32_t bit=1u<<(BTN_FX+tb),held=(fm1_in.buttons&bit)!=0;
            if(page<=PG_DANCE){
                static const uint8_t bk[4]={BK_FX,0,BK_ENV,BK_LFO},deep[4]={BK_FXD,BK_SRC,BK_ART,BK_HAR};
                int32_t act=-1;                            /* 0 short press, 1 long press */
                if(b&bit){lp_t[tb]=now;lp_on[tb]=1;}
                if(lp_on[tb]==1u && held && now-lp_t[tb]>=400u){lp_on[tb]=2;act=1;}
                if((released&bit) && lp_on[tb]){if(lp_on[tb]==1u)act=0;lp_on[tb]=0;}
                if(act==0 && tb==1u){sg.vx.lang=(uint8_t)((sg.vx.lang+1u)%VL_N);modified=1;pop(VS_LANG,0,lgname[sg.vx.lang],0);mk(0x20u);}
                else if(act>=0){
                    uint32_t nb=act?deep[tb]:bk[tb];
                    bank=bank==nb?BK_VOICE:nb;
                    popbuf[0]=0;cat(popbuf,bkname[bank]);cat(popbuf,":");
                    for(i=0;i<4u;i++){cat(popbuf," ");cat(popbuf,ksh[bank][i]);}
                    toast(popbuf,0);vis_kind=VS_NONE;mk(0x20u);
                }
            }else if(page==PG_LYRICS && (b&bit)){lyric_flag(tb);}
            else if(page==PG_LIBRARY && (b&bit)){
                if(tb==0)guarded(1,library_cursor);
                else if(tb==1)guarded(6,library_cursor);
                else if(tb==2 && library_cursor<SF_COUNT)guarded(5,library_cursor);
                else if(tb==3){if(library_cursor!=pf.slot)guarded(1,library_cursor);if(library_cursor==pf.slot){page=PG_VISUALS;full=1;}}
                mk(0x1fu);
            }
            else if(b&bit){lp_on[tb]=0;if(page==PG_GEN && tb==trk)gcf.lock^=(uint8_t)(1u<<tb);trk=tb;mk(0x3Fu);}
        }
        bank_leds();
        if(b&(1u<<BTN_OCT_UP) && oct<2){oct++;mk(0x21u);}
        if(b&(1u<<BTN_OCT_DOWN) && oct>-2){oct--;mk(0x21u);}
        if(b&(1u<<BTN_PLAY)){
            if(page==PG_LIBRARY&&library_cursor!=pf.slot){guarded(1,library_cursor);if(pf.slot==library_cursor&&!modified){playing=0;do_play();}}
            else do_play();
        }
        if(b&(1u<<BTN_SEQ)){if(page==PG_LIBRARY){page=PG_SONG;full=1;mk(0x1fu);}else stop_play();}
        if(b&(1u<<BTN_GLO)){if(page==PG_SONG||page==PG_PATTERN||page==PG_LYRICS){do_undo();if(page==PG_LYRICS)lyric_load();}else if(page==PG_GEN)gen_drop();}
        if(b&(1u<<BTN_ARP)){if(page==PG_STAGE||page==PG_DANCE){page=PG_LIBRARY;library_cursor=pf.slot;full=1;mk(0x3fu);}anchor=cstep;arp_c=0;if(page==PG_SONG && sg.nrows && sg.nrows<NROW){u_pushrows();lk();sg_row_dup(&sg,(int)crow);ulk();crow++;modified=1;mk(0x1Fu);}
            if(page==PG_MIXER){sg.trk[trk].solo^=1u;modified=1;mk(0x1Fu);}
            if(page==PG_GEN)gen_go(1);
            }
        if(b&(1u<<BTN_REC)){
            if(page==PG_LIBRARY){
                char nm[9];if(library_cursor!=pf.slot&&slot_name(library_cursor,nm)&&!(pend==4u&&pend_arg==library_cursor&&(int32_t)(now-pend_t)<0)){pend=4;pend_arg=library_cursor;pend_t=now+3000u;toast("OVERWRITE? REC AGAIN",1);}
                else{pend=0;do_save(library_cursor);}mk(0x1fu);
            }else if(page==PG_VISUALS){performance.effect_epoch=now;toast("EFFECT TRIGGERED",0);}
            else if(page==PG_PATTERN){
                uint32_t p=pat_of();Step z={{0,0,0,0}};
                if(p!=ROW_OFF){u_pushpat(trk,p);st_put(trk,p,cstep,&z);mk(0x1Eu);}
            }else if(page==PG_GEN)gen_go(0);
            else if(page==PG_LYRICS)lyric_delete();
            else if(page==PG_SONG && sg.nrows){u_pushrows();lk();sg_row_delete(&sg,(int)crow);ulk();modified=1;if(crow && crow>=sg.nrows)crow=sg.nrows-1u;mk(0x1Fu);}
        }
        if(b&(1u<<BTN_EDIT)){
            if(page==PG_DANCE){page=PG_VISUALS;full=1;mk(0x3fu);}
            else if(page==PG_VISUALS){page=PG_DANCE;full=1;mk(0x3fu);}
            else if(page==PG_SONG){
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
            }else if(page==PG_LIBRARY){char nm[9];guarded(slot_name(library_cursor,nm)?1u:6u,library_cursor);}
            else if(page==PG_LYRICS){
                if(fm1_in.buttons&(1u<<BTN_ARP)){
                    uint32_t k;arp_c=1;if(!lyric_clip_count)toast("NO COPIED LYRICS",1);
                    else{lyric_count=lyric_clip_count;for(k=0;k<lyric_count;k++)lyric_units[k]=lyric_clip[k];lyric_cursor=0;lyric_commit(1);}
                }else lyric_insert();
            }else if(page==PG_MIXER){sg.trk[trk].mute^=1u;modified=1;mk(0x1Fu);}
            else if(page==PG_GEN){gen_edit=1;mk(4u);}
        }
        if(page==PG_GEN && (released&(1u<<BTN_EDIT))){
            if(gen_edit==1u && take){take=0;toast("KEPT",0);}
            gen_edit=0;mk(4u);
        }
        if((released&(1u<<BTN_ARP)) && !arp_c && page==PG_PATTERN){
            uint32_t p=pat_of(),lo=anchor<cstep?anchor:cstep,hi=anchor<cstep?cstep:anchor;
            if(p!=ROW_OFF && sg_copy(&sg,(int)trk,(int)p,(int)lo,(int)(hi-lo+1u),clip)){clip_n=hi-lo+1u;clip_t=trk;toast("COPIED",0);}
        }
        if((released&(1u<<BTN_ARP)) && !arp_c && page==PG_LYRICS){uint32_t k;lyric_clip_count=lyric_count;for(k=0;k<lyric_count;k++)lyric_clip[k]=lyric_units[k];toast("LYRICS COPIED",0);}
        /* encoders, per page */
        if(page==PG_STAGE||page==PG_DANCE){
            if(dS){
                ed_mora=(uint32_t)wr((int32_t)ed_mora+dS,MORA_N);mk(0x20u);
                /* Selection only: keys own live note-on/off; PATTERN ctl() owns its editor audition. */
            }
            if(dA){tweak(&sg.vx.chr,dA,0,VC_N-1);pop(VS_CHR,0,"CHARACTER",sg.vx.chr);}
            if(dP && sg.vx.lang==VL_ANIMAL && (fm1_in.buttons&(1u<<BTN_SEL))){
                uint32_t rate=(uint32_t)cl((int32_t)animal_rate+dP*2,0,100);
                if(en_post(&en,EN_RATE,rate,0,0))animal_rate=rate;
                lp_on[1]=2; /* the modifier gesture consumes the language tap */
                pop(VS_NONE,0,"ANIMAL RATE",animal_rate);
            }else if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);pop(VS_BPM,0,"BPM",sg.bpm);}
            for(i=0;i<4;i++)if(dK[i]){kturn(bank,i,dK[i]);pop(VS_KNOB,bank*4u+i,knm[bank][i],*kptr(bank,i));}
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
        }else if(page==PG_LYRICS){
            if(dS&&lyric_count){lyric_cursor=(uint32_t)wr((int32_t)lyric_cursor+dS,(int32_t)lyric_count);if(!(lyric_units[lyric_cursor].flags&LY_REST))ed_mora=lyric_units[lyric_cursor].mora;mk(0x1Eu);}
            if(dA)lyric_token(dA);
            if(dK[0]&&lyric_count){lyric_units[lyric_cursor].ticks=(uint16_t)cl((int32_t)lyric_units[lyric_cursor].ticks+dK[0],1,NSTEP);lyric_commit(0);}
            if(dK[1]&&lyric_count&&!(lyric_units[lyric_cursor].flags&LY_REST)){lyric_notes[lyric_cursor]=(uint8_t)cl((int32_t)lyric_notes[lyric_cursor]+dK[1],1,127);lyric_commit(0);}
            if(dK[2]&&crow<sg.nrows){uint32_t p=lyric_pattern();int32_t n=p==ROW_OFF?0:(int32_t)p;n=cl(n+dK[2],0,NPAT-1);u_pushrows();sg_row_set(&sg,(int)crow,TR_VOCAL,n);modified=1;lyric_load();}
            if(dK[3]&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dK[3],0,(int32_t)sg.nrows-1);lyric_load();}
            if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(1u);}
        }else if(page==PG_LIBRARY){
            if(dS||dP){library_cursor=(uint32_t)wr((int32_t)library_cursor+dS+dP,NSLOT);mk(0x1fu);}
        }else if(page==PG_VISUALS){
            if(dS){perf_style(&performance,(uint32_t)wr((int32_t)performance.outfit+dS,WD_OUTFIT_N));stage_init();face_cur=SF_NEUTRAL;dance_cur=99;}
            if(dP)performance.palette=(uint8_t)wr((int32_t)performance.palette+dP,PERF_PALETTE_N);
            if(dK[0])performance.background=(uint8_t)wr((int32_t)performance.background+dK[0],PERF_BG_N);
            if(dK[1])performance.frame=(uint8_t)wr((int32_t)performance.frame+dK[1],PERF_FRAME_N);
            if(dK[2]||dA){performance.effect=(uint8_t)wr((int32_t)performance.effect+dK[2]+dA,STFX_N);performance.effect_epoch=now;}
            if(dK[3])performance.intensity=(uint8_t)cl((int32_t)performance.intensity+dK[3]*4,0,100);
            if(dS||dA||dP||dK[0]||dK[1]||dK[2]||dK[3]){modified=1;mk(0x1fu);}
        }else if(page==PG_GEN){
            if(dS&&sg.nrows){crow=(uint32_t)cl((int32_t)crow+dS,0,(int32_t)sg.nrows-1);mk(1u);}
            if(dP){tweak(&sg.bpm,dP,40,240);if(playing)en_post(&en,EN_BPM,sg.bpm,0,0);mk(4u);}
            if(dK[0]){gen_style_id=(uint32_t)wr((int32_t)gen_style_id+dK[0],GEN_CHOICES);mk(2u);}
            if(gen_edit){
                if(dK[1]){gen_energy=(uint32_t)cl((int32_t)gen_energy+dK[1]*2,0,100);gen_edit=2;mk(4u);}
                if(dK[2]){gen_complexity=(uint32_t)cl((int32_t)gen_complexity+dK[2]*2,0,100);gen_edit=2;mk(4u);}
            }else{
                if(dK[1]){gcf.key=(uint8_t)wr(gcf.key+dK[1],12);mk(2u);}
                if(dK[2]){gcf.scale^=1u;mk(2u);}
            }
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
        kt=page<=PG_DANCE||page==PG_LIBRARY?TR_VOCAL:trk;                    /* the voice's home: keys sing */
        if(down){
            uint32_t k;
            for(k=0;k<27u;k++)if(down&(1u<<k)){
                int32_t n=cl((int32_t)kbase[kt]+(int32_t)k+oct*12,1,127);
                kn[k]=(uint8_t)n;
                if(kt==TR_DRUMS){if(k<6u)en_post(&en,EN_ON,kt,k,0);else if(k<12u)en_post(&en,EN_ON,kt,k-6u,0);}
                else en_post(&en,EN_ON,kt,(uint32_t)n,ed_mora);
                if(page==PG_PATTERN)kwrite(k,(uint32_t)n);else if(page==PG_LYRICS)lyric_key((uint32_t)n);
            }
            pv_off=0;
        }
        if(prev_keys&~keys){
            uint32_t k;
            if(kt==TR_CHORDS){for(k=0;k<27u;k++)if((prev_keys&~keys)&(1u<<k))en_post(&en,EN_OFF,kt,kn[k],0);}
            else if(!keys && kt!=TR_DRUMS)en_post(&en,EN_OFF,kt,0,0);
        }
        prev_keys=keys;
        if(pv_off && (int32_t)(now-pv_off)>=0){pv_off=0;if(!keys)en_post(&en,EN_OFF,0,0,0);}
        /* what the engine tells us */
        if(ppos!=oldpos||playing!=(oldpos>>16&1u)){
            if(page==PG_STAGE||page==PG_DANCE)mk(0x20u);
            else if(page==PG_LIBRARY)mk(1u);
            else if(page==PG_SONG)mk(0x1Eu);
            else if(page==PG_PATTERN && (ppos>>8&255u)==crow)mk(0x0Eu);
            else if(page==PG_PATTERN)mk(1u);
        }
        if(en.act!=last_act){last_act=en.act;if(page==PG_STAGE||page==PG_DANCE)mk(0x20u);}
        if(toast_s && (int32_t)(now-toast_until)>=0){toast_s=0;mk(0x21u);}
        {   /* Animation state advances from the musical clock, independently of LCD redraw. */
            uint32_t row=ppos>>8&255u;
            PerfInput pi={now,((row*16u+(ppos&255u))*64u),(uint8_t)playing,
                row<sg.nrows?sg.row[row].sect:0,(uint8_t)en.mouth,
                (uint8_t)((en.act&1u)?en.level:0),0,0};
            perf_update(&performance,&pi);
#if MIKU_UVC
            video_update(&performance);
#endif
        }
        if(page==PG_STAGE){face_update();dial_step();}
        else if(page==PG_DANCE)dance_update();
        else if((int32_t)(now-next_blink)>=0)next_blink=now+3000u;
        if(now-cpu_ms>=500u){
            uint32_t p=pub_peak*100u/BUDGET_TICKS;
            cpu_ms=now;
            if(p!=cpu_pct){cpu_pct=p;if(!page)mk(0x20u);}
        }
        if(dirty||full)redraw();
    }
}

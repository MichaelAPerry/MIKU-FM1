/* SPDX-License-Identifier: GPL-3.0-only
 * MIKU-FM1 native: a singing synthesizer for the FM-1 on Dinghy. Formant voice (fvoice), 16-step eighth-note melody,
 * singer face on the STAGE page, the step grid on the EDIT page. Unofficial fan project, not a voicebank. */
#define APP_NAME "MIKU-FM"
#include "platform.c"
#include "firmware/audio/fvoice/fvoice.c"
#include "firmware/ui/singer_sprites.h"
#define STEPS 16u
#define BUDGET_TICKS (5805u*FM1_TICKS_PER_US)       /* 256 frames at 44.1 kHz */
static FvVoice fv;
/* main loop -> ISR: one word, written whole (a counter makes every request new); ISR -> main: published copies */
static volatile uint32_t rq,pv,pp,pb,pt,pub_mouth=5,pub_level,pub_peak;
static volatile int32_t ptr;                 /* transpose (semitones), OCT- / OCT+ */
static int32_t oct;
static struct {uint8_t pitch[STEPS], vowel[STEPS], bpm, kn[4];} song;   /* kn: vibrato, portamento, breath, brightness 0..100 */
static uint32_t cursor,playhead,last_ms,off_ms,off_done=1,dirty=1,prev_keys,playing,page,cpu_ms,cpu_pct,
    face_cur,face_ms,blink_until,next_blink,happy_until;
static uint8_t hw[2u*SINGER_FACE_H];
static uint16_t pw[16],c_bg,c_disc;
static const uint8_t demo_p[STEPS]={15,15,17,20,17,15,12,15,17,17,20,22,24,22,20,0},demo_v[STEPS]={4,4,4,0,4,4,0,0,0,0,1,0,0,2,4,0};
static const char *const mora[STEPS]={"O","TO","NO","HA","KO","NO","NA","KA","WA","TA","SHI","WA","MA","TSU","YO",""};
static void app_audio(int32_t *out,uint32_t n){
    static uint32_t seen,up,pk;
    uint32_t t0=fm1_ticks(),r=rq,c;
    if(!up){fv_init(&fv);fv.vib_rate=55;fv.vib_delay=200;up=1;}
    if(r!=seen){
        seen=r;
        if(((r>>12)&3u)==1u)fv_note_on(&fv,(uint8_t)(r&127u),(uint8_t)((r>>8)&7u),110);
        else if(((r>>12)&3u)==0u)fv_note_off(&fv);
        else fv_set_vowel(&fv,(uint8_t)((r>>8)&7u));
    }
    fv.vib_depth=(uint16_t)pv;fv.porta=(uint16_t)pp;fv.breath=(uint8_t)pb;fv.bright=(uint8_t)pt;fv.transpose=(int8_t)ptr;
    fv_render(&fv,out,n);
    for(c=0;c<2u*n;c++){int32_t o=out[c]*3/2;out[c]=o>32767?32767:o<-32767?-32767:o;}   /* +3.5 dB: demo peaks ~-3 dBFS at full MASTER */
    pub_mouth=fv_mouth(&fv);pub_level=fv_level(&fv);
    c=fm1_ticks()-t0;
    if(c>pk)pk=c;else pk-=pk>>6;
    pub_peak=pk;
}
static void post(uint32_t kind,uint32_t note,uint32_t vw){
    static uint32_t cnt;
    rq=(++cnt<<16)|kind<<12|vw<<8|note;
}
static void knobs(void){pv=song.kn[0];pp=song.kn[1]*5u;pb=song.kn[2]*127u/100u;pt=song.kn[3]*127u/100u;}
static void defaults(void){
    static const uint8_t kn[4]={35,10,8,79};   /* = vibrato 35, portamento 50 ms, breath 10, brightness 100 */
    uint32_t i;
    for(i=0;i<STEPS;i++){song.pitch[i]=demo_p[i];song.vowel[i]=demo_v[i];}
    for(i=0;i<4;i++)song.kn[i]=kn[i];
    song.bpm=148;
}
static void apply_step(uint32_t index){
    uint32_t n=song.pitch[index];
    if(n)post(1,52u+n,song.vowel[index]);
    else if(!fm1_in.notes)post(0,0,0);
    off_done=0;
}
static uint32_t is_demo(void){
    uint32_t i,same=1;
    for(i=0;i<STEPS;i++)same&=song.pitch[i]==demo_p[i] && song.vowel[i]==demo_v[i];
    return same;
}
static uint32_t period(void){return 30000u/(uint32_t)song.bpm;}      /* one eighth note, ms */
static void num(char *s,uint32_t n){
    char t[6];uint32_t i=0,j=0;
    do{t[i++]=(char)('0'+n%10u);n/=10u;}while(n&&i<5u);
    while(i)s[j++]=t[--i];
    s[j]=0;
}
static void cat(char *d,const char *s){while(*d)d++;while((*d++=*s++)!=0);}
/* ---- face: 4bpp sprite rows, 2x, into the canvas buffer, blitted (the panel wants bytes swapped: swap16) */
#define FX 30u
#define FY 2u
static void face_draw(uint32_t f,uint32_t sx,uint32_t sy,uint32_t w,uint32_t h){
    const uint8_t *d=SINGER_FACES[f];
    uint32_t cr=CV_MAX/(4u*w),r0,r,c,k;
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
    uint32_t now=fm1_ms,m=pub_mouth,w;
    if((int32_t)(now-happy_until)<0)w=SF_HAPPY;
    else if(m<5u)w=pub_level>=100u?vf[m]:SF_N;
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
static void stage_text(void){
    char s[12]="BPM ",c[12]="CPU ";
    const char *m=is_demo()?mora[playing?playhead:cursor]:"";
    uint32_t i,x=playing?playhead:cursor;
    static const char *const vs[5]={"A","I","U","E","O"};
    if(!is_demo())m=vs[song.vowel[x]];
    num(s+4,song.bpm);num(c+4,cpu_pct);cat(c,"%");
    if(oct){char o[8];num(s,song.bpm);cat(s,oct>0?" OCT+":" OCT-");num(o,(uint32_t)(oct>0?oct:-oct));cat(s,o);}
    cv_begin(240,56,RGB(12,20,40));
    cv_text(120-text_w(&FONT_L,m)/2,0,&FONT_L,m,C_WHITE);
    cv_text(4,14,&FONT_S,s,RGB(249,207,94));
    cv_text(236-text_w(&FONT_S,c),14,&FONT_S,c,RGB(150,160,190));
    for(i=0;i<STEPS;i++){
        uint16_t col=song.pitch[i]?RGB(35,182,176):RGB(49,54,78);
        if(cursor==i)col=RGB(247,123,180);
        if(playing && playhead==i)col=RGB(249,207,94);
        cv_rect(9+(int32_t)i*14,40,12,12,col);
    }
    cv_blit(0,184);
}
static void stage_full(void){
    lcd_fill(0,0,240,240,RGB(12,20,40));
    face_draw(face_cur,0,0,SINGER_FACE_W,SINGER_FACE_H);
    stage_text();
}
static void edit_full(void){
    static const char *const vs[5]={"A","I","U","E","O"};
    uint32_t i;char s[30]="BPM ",k[30]="V",t[8];
    lcd_fill(0,0,240,240,RGB(7,17,34));
    draw_text_box(0,5,240,&FONT_L,"MIKU-FM",RGB(64,222,209),1);
    num(k+1,song.kn[0]);cat(k," P");num(t,song.kn[1]);cat(k,t);cat(k," B");num(t,song.kn[2]);cat(k,t);cat(k," T");num(t,song.kn[3]);cat(k,t);
    draw_text_box(0,43,240,&FONT_S,k,C_WHITE,1);
    for(i=0;i<STEPS;i++){
        uint32_t x=12u+(i%8u)*28u,y=75u+(i/8u)*33u;
        uint16_t c=song.pitch[i]?RGB(35,182,176):RGB(49,54,78);
        if(cursor==i)c=RGB(247,123,180);
        if(playing && playhead==i)c=RGB(249,207,94);
        lcd_fill(x,y,23,25,c);
        if(song.pitch[i])lcd_fill(x+8u,y+4u,7u,7u,RGB(16,31,50));
    }
    num(s+4,song.bpm);
    draw_text_box(0,150,240,&FONT_S,s,C_WHITE,1);
    s[0]=0;cat(s,"STEP ");num(t,cursor+1u);cat(s,t);
    draw_text_box(0,171,240,&FONT_S,s,RGB(249,207,94),1);
    s[0]=0;cat(s,"VOWEL ");cat(s,vs[song.vowel[cursor]]);
    draw_text_box(0,190,240,&FONT_S,s,RGB(247,123,180),1);
    draw_text_box(0,213,240,&FONT_S,playing?"PLAY: PAUSE SEQ: STOP":"KEY: WRITE REC: REST",C_WHITE,1);
}
static void app_main(void){
    uint32_t last_input=fm1_ms,full=1;
    defaults();
    if(settings_load(&song,sizeof song)==0){
        uint32_t valid=song.bpm>=40u && song.bpm<=240u;
        for(uint32_t i=0;i<STEPS;i++)valid&=song.pitch[i]<=27u && song.vowel[i]<5u;
        for(uint32_t i=0;i<4;i++)valid&=song.kn[i]<=100u;
        if(!valid)defaults();
    }
    knobs();stage_init();
    last_ms=fm1_ms;next_blink=fm1_ms+3000u;
    for(;;){
        uint32_t released,b,keys,down,i;
        int32_t delta;
        platform_poll();
        b=fm1_input_edges(&released);
        if(b&(1u<<BTN_HOME)){page^=1u;full=1;}
        if(b&(1u<<BTN_OCT_UP) && oct<2){oct++;ptr=oct*12;dirty=1;}
        if(b&(1u<<BTN_OCT_DOWN) && oct>-2){oct--;ptr=oct*12;dirty=1;}
        if(b&(1u<<BTN_PLAY)){
            playing=!playing;playhead=cursor;
            if(playing){apply_step(playhead);last_ms=fm1_ms;off_ms=last_ms+period()*85u/100u;happy_until=fm1_ms+500u;}
            else post(0,0,0);
            dirty=1;
        }
        if(b&(1u<<BTN_SEQ)){playing=0;post(0,0,0);dirty=1;}
        if(b&(1u<<BTN_REC)){song.pitch[cursor]=0;dirty=1;last_input=fm1_ms;}
        if(b&(1u<<BTN_SAVE)){
            if(!playing){post(0,0,0);settings_save(&song,sizeof song);}
            dirty=1;
        }
        delta=fm1_enc_take(ENC_SELECT);
        if(delta){cursor=(uint32_t)((int32_t)(cursor+STEPS+delta)%STEPS);dirty=1;}
        delta=fm1_enc_take(ENC_ALGORITHM);
        if(delta){
            int32_t v=(int32_t)song.vowel[cursor]+delta;
            while(v<0)v+=5;
            song.vowel[cursor]=(uint8_t)(v%5);
            if(!playing)post(2,0,song.vowel[cursor]);
            last_input=fm1_ms;dirty=1;
        }
        delta=fm1_enc_take(ENC_PRESETS);
        if(delta){
            int32_t bpm=(int32_t)song.bpm+delta;
            song.bpm=(uint8_t)(bpm<40?40:bpm>240?240:bpm);
            last_input=fm1_ms;dirty=1;
        }
        for(i=0;i<4;i++){
            delta=fm1_enc_take(ENC_KNOB1+i)*2;
            if(delta){
                delta+=(int32_t)song.kn[i];
                song.kn[i]=(uint8_t)(delta<0?0:delta>100?100:delta);
                knobs();last_input=fm1_ms;dirty=1;
            }
        }
        keys=fm1_in.notes;down=keys&~prev_keys;
        if(down){
            uint32_t k=0;while(k<26u && !(down&(1u<<k)))k++;
            song.pitch[cursor]=(uint8_t)(k+1u);
            post(1,53u+k,song.vowel[cursor]);
            last_input=fm1_ms;dirty=1;
        }
        if(!keys && prev_keys)post(0,0,0);
        prev_keys=keys;
        if(playing){
            if(!off_done && (int32_t)(fm1_ms-off_ms)>=0){off_done=1;if(!keys)post(0,0,0);}
            if((uint32_t)(fm1_ms-last_ms)>=period()){
                last_ms+=period();playhead=(playhead+1u)%STEPS;apply_step(playhead);
                off_ms=last_ms+period()*85u/100u;dirty=1;
            }
        }
        face_update();
        if((uint32_t)(fm1_ms-cpu_ms)>=500u){
            uint32_t p=pub_peak*100u/BUDGET_TICKS;
            cpu_ms=fm1_ms;
            if(p!=cpu_pct){cpu_pct=p;if(!page)dirty=1;}
        }
        if(full){page?edit_full():stage_full();full=dirty=0;}
        else if(dirty){page?edit_full():stage_text();dirty=0;}
    }
}

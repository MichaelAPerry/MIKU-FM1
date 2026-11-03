/* SPDX-License-Identifier: GPL-3.0-only
 * MIKU-FM1 native, editable 16-step melody for the FM-1, on Dinghy.
 * Experimental monophonic harmonic-vowel instrument, not a voicebank. */
#define APP_NAME "MIKU-FM"
#include "platform.c"
#define STEPS 16u
static uint32_t phase;
static volatile uint32_t step=25480161u,gate,vowel,playing;
static int32_t env;
static const uint32_t notes[27]={17005939u,18017165u,19088521u,20223584u,21426141u,22700205u,24050030u,25480119u,26995246u,28600467u,30301139u,32102938u,34011878u,36034330u,38177043u,40447168u,42852281u,45400411u,48100060u,50960238u,53990491u,57200933u,60602278u,64205876u,68023757u,72068660u,76354085u};
static struct {uint8_t pitch[STEPS], vowel[STEPS], bpm;} song;
static uint32_t cursor,playhead,last_ms,dirty=1,prev_keys;
static int32_t sine(uint32_t ph){
    int32_t x=(int32_t)ph>>16,ax=x<0?-x:x,y=(x*(32768-ax))>>13;
    int32_t ay=y<0?-y:y;
    y+=((((y*ay)>>15)-y)*7373)>>15;
    return y>32767?32767:y< -32767?-32767:y;
}
static void app_audio(int32_t *out,uint32_t n){
    uint32_t p=phase,i,inc=step,v=vowel;int32_t a=env,target=gate?10000:0;
    for(i=0;i<n;i++){
        if(a<target){a+=48;if(a>target)a=target;}
        else if(a>target){a-=12;if(a<target)a=target;}
        p+=inc;
        int32_t x=sine(p),h=sine(p*2u),j=sine(p*3u);
        switch(v%5u){
            case 0:x=(x*5+h*3+j*2)/10;break;
            case 1:x=(x*3+h+j*6)/10;break;
            case 2:x=(x*7+h*2+j)/10;break;
            case 3:x=(x*4+h*4+j*2)/10;break;
            default:x=(x*8+h+j)/10;break;
        }
        x=(x*a)>>15;out[2*i]+=x;out[2*i+1]+=x;
    }
    phase=p;env=a;
}
static void apply_step(uint32_t index){
    uint32_t n=song.pitch[index];
    vowel=song.vowel[index];
    if(n){step=notes[n-1u];gate=1;}else gate=0;
}
static void defaults(void){
    static const uint8_t demo[16]={8,10,12,15,12,10,8,5,8,10,13,12,15,13,10,8};
    uint32_t i;
    for(i=0;i<STEPS;i++){song.pitch[i]=demo[i];song.vowel[i]=(uint8_t)(i%5u);}
    song.bpm=120;
}
static void print_number(char *s,uint32_t n){
    s[0]=(char)('0'+(n/100u)%10u);
    s[1]=(char)('0'+(n/10u)%10u);
    s[2]=(char)('0'+n%10u);
    s[3]=0;
}
static void redraw(void){
    static const char *const vs[5]={"A","I","U","E","O"};
    uint32_t i;char num[4],line[30]="BPM ";
    lcd_fill(0,0,240,240,RGB(7,17,34));
    draw_text_box(0,5,240,&FONT_L,"MIKU-FM",RGB(64,222,209),1);
    draw_text_box(0,43,240,&FONT_S,"EDITABLE MELODY",C_WHITE,1);
    for(i=0;i<STEPS;i++){
        uint32_t x=12u+(i%8u)*28u,y=75u+(i/8u)*33u;
        uint16_t c=song.pitch[i]?RGB(35,182,176):RGB(49,54,78);
        if(cursor==i)c=RGB(247,123,180);
        if(playing && playhead==i)c=RGB(249,207,94);
        lcd_fill(x,y,23,25,c);
        if(song.pitch[i])lcd_fill(x+8u,y+4u,7u,7u,RGB(16,31,50));
    }
    print_number(num,(uint32_t)song.bpm);
    line[4]=num[0];line[5]=num[1];line[6]=num[2];line[7]=0;
    draw_text_box(0,150,240,&FONT_S,line,C_WHITE,1);
    print_number(num,cursor+1u);
    draw_text_box(0,171,240,&FONT_S,num,RGB(249,207,94),1);
    draw_text_box(0,190,240,&FONT_S,vs[song.vowel[cursor]],RGB(247,123,180),1);
    draw_text_box(0,213,240,&FONT_S,playing?"PLAY: PAUSE SEQ: STOP":"KEY: WRITE REC: REST",C_WHITE,1);
}
static void app_main(void){
    uint32_t last_input=fm1_ms;
    defaults();
    if(settings_load(&song,sizeof song)==0){
        uint32_t valid=song.bpm>=40u && song.bpm<=240u;
        for(uint32_t i=0;i<STEPS;i++)valid&=song.pitch[i]<=27u && song.vowel[i]<5u;
        if(!valid)defaults();
    }
    last_ms=fm1_ms;
    for(;;){
        uint32_t released,b,keys,down;
        int32_t delta;
        platform_poll();
        b=fm1_input_edges(&released);
        if(b&(1u<<BTN_PLAY)){
            playing=!playing;playhead=cursor;
            if(playing){apply_step(playhead);last_ms=fm1_ms;}
            else gate=0;
            dirty=1;
        }
        if(b&(1u<<BTN_SEQ)){playing=0;gate=0;dirty=1;}
        if(b&(1u<<BTN_REC)){song.pitch[cursor]=0;dirty=1;last_input=fm1_ms;}
        if(b&(1u<<BTN_SAVE)){
            if(!playing){gate=0;settings_save(&song,sizeof song);}
            dirty=1;
        }
        delta=fm1_enc_take(ENC_SELECT);
        if(delta){cursor=(uint32_t)((int32_t)(cursor+STEPS+delta)%STEPS);dirty=1;}
        delta=fm1_enc_take(ENC_ALGORITHM);
        if(delta){
            int32_t v=(int32_t)song.vowel[cursor]+delta;
            while(v<0)v+=5;
            song.vowel[cursor]=(uint8_t)(v%5);
            if(!playing)vowel=song.vowel[cursor];
            last_input=fm1_ms;dirty=1;
        }
        delta=fm1_enc_take(ENC_PRESETS);
        if(delta){
            int32_t bpm=(int32_t)song.bpm+delta;
            song.bpm=(uint8_t)(bpm<40?40:bpm>240?240:bpm);
            last_input=fm1_ms;dirty=1;
        }
        keys=fm1_in.notes;down=keys&~prev_keys;
        if(down){
            uint32_t k=0;while(k<26u && !(down&(1u<<k)))k++;
            song.pitch[cursor]=(uint8_t)(k+1u);
            if(!playing){step=notes[k];gate=1;vowel=song.vowel[cursor];}
            last_input=fm1_ms;dirty=1;
        }
        if(!keys && prev_keys && !playing){gate=0;dirty=1;}
        prev_keys=keys;
        if(playing && (uint32_t)(fm1_ms-last_ms)>=60000u/((uint32_t)song.bpm*4u)){
            last_ms=fm1_ms;playhead=(playhead+1u)%STEPS;apply_step(playhead);dirty=1;
        }
        if(dirty){redraw();dirty=0;}
    }
}

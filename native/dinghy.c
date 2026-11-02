/* SPDX-License-Identifier: GPL-3.0-only */
/* MIKU-FM native FM-1 app, built on Dinghy's GPL platform. */
#define APP_NAME "MIKU-FM"
#define FELUCCA_ID "FM-1_949"
#include "platform.c"
static uint32_t phase;
static volatile uint32_t step=365072220u,gate=0,vowel=0,playing=0;
static int32_t env;
static const uint32_t tune[8]={365072220u,409868193u,460121920u,547063359u,460121920u,409868193u,365072220u,307010551u};
static int32_t sine(uint32_t ph){
    int32_t x=(int32_t)ph>>16, ax=x<0?-x:x, y=(x*(32768-ax))>>13;
    int32_t ay=y<0?-y:y;
    y+=((((y*ay)>>15)-y)*7373)>>15;
    return y>32767?32767:y< -32767?-32767:y;
}
static void app_audio(int32_t *out,uint32_t n){
    uint32_t p=phase,i,inc=step,v=vowel;
    int32_t a=env,target=gate?10000:0;
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
static void redraw(uint32_t index){
    static const char *const vowels[5]={"A","I","U","E","O"};
    char s[]="STEP 1/8";s[5]=(char)('1'+index);
    lcd_fill(0,0,240,240,RGB(7,17,34));
    draw_text_box(0,12,240,&FONT_L,"MIKU-FM",RGB(64,222,209),1);
    draw_text_box(0,55,240,&FONT_S,"FM-1 NATIVE PROTOTYPE",C_WHITE,1);
    lcd_fill(44,88,48,92,RGB(0,180,175));
    lcd_fill(148,88,48,92,RGB(0,180,175));
    lcd_fill(82,84,76,104,RGB(255,210,185));
    lcd_fill(94,117,12,10,RGB(8,22,51));
    lcd_fill(134,117,12,10,RGB(8,22,51));
    lcd_fill(111,146,18,gate?14:3,RGB(166,28,87));
    draw_text_box(0,190,240,&FONT_S,vowels[vowel%5u],C_WHITE,1);
    draw_text_box(0,211,240,&FONT_S,playing?s:"PLAY: START  SEQ: STOP",C_WHITE,1);
}
static void app_main(void){
    uint32_t index=0,last=fm1_ms,previous=0,dirty=1;
    for(;;){
        uint32_t released,buttons,notes,down;
        platform_poll();
        buttons=fm1_input_edges(&released);
        if(buttons&(1u<<BTN_PLAY)){playing=!playing;gate=playing;index=0;step=tune[0];last=fm1_ms;dirty=1;}
        if(buttons&(1u<<BTN_SEQ)){playing=0;gate=0;dirty=1;}
        if(fm1_enc_take(ENC_ALGORITHM)){vowel=(vowel+1u)%5u;dirty=1;}
        notes=fm1_in.notes;down=notes&~previous;
        if(down){uint32_t k=0;while(k<26u && !(down&(1u<<k)))k++;step=tune[k%8u];gate=1;dirty=1;}
        if(!notes && previous && !playing){gate=0;dirty=1;}
        previous=notes;
        if(playing && (uint32_t)(fm1_ms-last)>=250u){
            last+=250u;index=(index+1u)%8u;step=tune[index];dirty=1;
        }
        if(dirty){redraw(index);dirty=0;}
    }
}

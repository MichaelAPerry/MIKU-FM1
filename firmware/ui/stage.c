/* SPDX-License-Identifier: MIT */
#include "firmware/ui/stage.h"
#include <string.h>
/* RGB565 original pixel character, no fonts/assets or external graphics. */
#define BG 0x10A5u
#define HAIR 0x067Fu
#define FACE 0xFFBCu
#define DARK 0x19CBu
#define WHITE 0xFFFFu
#define PINK 0xF12Fu
#define CYAN 0x05FFu
static void rect(uint16_t *b,int x,int y,int w,int h,uint16_t c){
    if(x<0||y<0||w<=0||h<=0)return;
    for(int yy=y;yy<y+h&&yy<MIKU_LCD_H;yy++)
        for(int xx=x;xx<x+w&&xx<MIKU_LCD_W;xx++)b[yy*MIKU_LCD_W+xx]=c;
}
static void glyph(uint16_t *b,int x,int y,int digit,uint16_t color){
    /* 3x5 bitmap numerals */
    static const uint16_t numerals[10]={0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
    uint16_t bits=numerals[digit%10];
    for(int dy=0;dy<5;dy++)for(int dx=0;dx<3;dx++)
        if((bits>>(14-(dy*3+dx)))&1)rect(b,x+dx*3,y+dy*3,3,3,color);
}
void miku_stage_init(MikuStage *s){
    if(!s)return;
    memset(s,0,sizeof(*s));s->bpm=120;
}
void miku_stage_button(MikuStage *s,uint32_t pressed){
    if(!s)return;
    if(pressed&MIKU_BTN_STOP)s->playing=0;
    else if(pressed&MIKU_BTN_PLAY)s->playing=!s->playing;
    if(pressed&MIKU_BTN_NEXT)s->page=(uint8_t)((s->page+1)%4);
    if(pressed&MIKU_BTN_PREV)s->page=(uint8_t)((s->page+3)%4);
    if(pressed&MIKU_BTN_VOWEL)s->vowel=(uint8_t)((s->vowel+1)%5);
}
void miku_stage_draw(const MikuStage *s,uint16_t *b,uint32_t pixels){
    if(!s||!b||pixels<MIKU_LCD_W*MIKU_LCD_H)return;
    for(int i=0;i<MIKU_LCD_W*MIKU_LCD_H;i++)b[i]=BG;
    rect(b,0,0,240,27,DARK);rect(b,8,9,9,9,CYAN);rect(b,21,9,9,9,PINK);
    /* Animated original singing idol with twin tails */
    rect(b,36,55,37,112,HAIR);rect(b,167,55,37,112,HAIR);
    rect(b,70,42,100,121,HAIR);rect(b,81,58,78,94,FACE);
    rect(b,89,96,17,12,DARK);rect(b,133,96,17,12,DARK);
    rect(b,92,96,5,4,WHITE);rect(b,136,96,5,4,WHITE);
    if(s->blink){rect(b,89,98,17,4,HAIR);rect(b,133,98,17,4,HAIR);}
    rect(b,87,145,66,8,HAIR);
    rect(b,111,117,18,s->playing?(s->vowel==0?19:s->vowel==1?7:s->vowel==2?11:s->vowel==3?15:13):3,DARK);
    rect(b,99,164,42,31,PINK);rect(b,64,176,33,31,CYAN);rect(b,143,176,33,31,CYAN);
    rect(b,0,213,240,27,DARK);
    /* transport indicator, 3-digit BPM, vowel index, page index */
    rect(b,12,220,12,12,s->playing?CYAN:PINK);
    unsigned v=s->bpm>999?999:s->bpm;
    glyph(b,53,219,v/100,WHITE);glyph(b,66,219,(v/10)%10,WHITE);glyph(b,79,219,v%10,WHITE);
    glyph(b,155,219,s->vowel,PINK);glyph(b,197,219,s->page,CYAN);
}

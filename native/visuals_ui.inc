/* SPDX-License-Identifier: GPL-3.0-only */
static void pg_visuals(uint32_t band){
    static const char *fx[]={"OFF","FIREWORKS","SPARKLES","CONFETTI","SMOKE","BEAMS"};
    static const char *bg[]={"ORIGINAL","GRID","HORIZON","CONCERT"};
    static const char *fr[]={"NONE","CORNERS","RAILS"};
    char s[32];uint32_t y=band*48u;
    if(!band){txt(120,4,&FONT_S,"VISUALS",C_PNK,1);txt(120,26,&FONT_S,"SELECT: OUTFIT",C_WHITE,1);}
    else if(band==1){txt(120,y+4,&FONT_S,WD_OUTFITS[performance.outfit].name,C_PNK,1);txt(120,y+26,&FONT_S,"PAIRED FACE + DANCE",C_WHITE,1);}
    else if(band==2){txt(8,y+4,&FONT_S,"K1 BACKGROUND",C_WHITE,0);txt(232,y+4,&FONT_S,bg[performance.background],C_PNK,2);txt(8,y+26,&FONT_S,"K2 FRAME",C_WHITE,0);txt(232,y+26,&FONT_S,fr[performance.frame],C_PNK,2);}
    else if(band==3){txt(8,y+4,&FONT_S,"K3 EFFECT",C_WHITE,0);txt(232,y+4,&FONT_S,fx[performance.effect],C_PNK,2);s[0]=0;cat(s,"K4 INTENSITY ");catn(s,performance.intensity);txt(120,y+26,&FONT_S,s,C_WHITE,1);}
    else{txt(120,y+4,&FONT_S,"REC: TRIGGER EFFECT",C_WHITE,1);txt(120,y+26,&FONT_S,"DANCE / USB: WATCH",C_PNK,1);}
}

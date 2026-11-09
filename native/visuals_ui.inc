/* SPDX-License-Identifier: GPL-3.0-only */
static void pg_visuals(uint32_t band){
    static const char *fx[]={"OFF","FIREWORKS","SPARKLES","CONFETTI","SMOKE","BEAMS"};
    static const char *bg[]={"ORIGINAL","GRID","HORIZON","CONCERT","LED WALL"};
    static const char *fr[]={"NONE","CORNERS","RAILS"};
    char s[32];uint32_t y=band*48u;
    if(!band){char name[18];sname_slot(name);s[0]=0;cat(s,"LOOK ");cat(s,name);if(modified)cat(s,"*");txt(120,4,&FONT_S,s,C_PNK,1);txt(120,26,&FONT_S,"SELECT: STYLE / OUTFIT",C_WHITE,1);}
    else if(band==1){
        uint32_t xx,yy;uint8_t index;const WardrobeSprites *w=WD_OUTFITS+performance.outfit;
        txt(8,y+4,&FONT_S,w->name,C_PNK,0);txt(8,y+26,&FONT_S,performance.palette?"PRESETS: WARM":"PRESETS: STYLE",C_WHITE,0);
        for(yy=0;yy<42u;yy++)for(xx=0;xx<42u;xx++){
            uint32_t px=xx*240u/42u,py=yy*214u/42u;uint16_t colour=perf_pixel(&performance,px,py);
            index=0;if(px>=28u&&px<211u&&py<213u&&spr_pixel(w->stage,performance.sprite,(px-28u)/3u,py/3u,&index)&&index)colour=w->stage_palette[index];
            cv_rect(195+(int32_t)xx,(int32_t)y+2+(int32_t)yy,1,1,colour);
        }
        for(yy=0;yy<22u;yy++)for(xx=0;xx<22u;xx++){index=0;if(spr_pixel(w->faces,0,xx*4u,yy*4u,&index)&&index)cv_rect(167+(int32_t)xx,(int32_t)y+10+(int32_t)yy,1,1,w->face_palette[index]);}
    }
    else if(band==2){txt(8,y+4,&FONT_S,"K1 BACKGROUND",C_WHITE,0);txt(232,y+4,&FONT_S,bg[performance.background],C_PNK,2);txt(8,y+26,&FONT_S,"K2 FRAME",C_WHITE,0);txt(232,y+26,&FONT_S,fr[performance.frame],C_PNK,2);}
    else if(band==3){txt(8,y+4,&FONT_S,"K3 EFFECT",C_WHITE,0);txt(232,y+4,&FONT_S,fx[performance.effect],C_PNK,2);s[0]=0;cat(s,"K4 INTENSITY ");catn(s,performance.intensity);txt(120,y+26,&FONT_S,s,C_WHITE,1);}
    else{txt(120,y+4,&FONT_S,"REC: TRIGGER EFFECT",C_WHITE,1);txt(120,y+26,&FONT_S,"EDIT: DANCE SAVE: KEEP",C_PNK,1);}
}

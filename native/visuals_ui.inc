/* SPDX-License-Identifier: GPL-3.0-only */
/* Five independently clipped bands; no framebuffer or additional sprite storage. */
#define LOOK_BG BG
#define LOOK_CARD RGB(20,22,42)
#define LOOK_MUTED RGB(145,150,164)
static void look_tile(int32_t x,int32_t y,const char *knob,const char *value,uint16_t accent){
    ui_panel(x,y,110,26,1);cv_rect(x,y+5,2,16,accent);
    txt(x+7,y+5,&FONT_S,knob,LOOK_MUTED,0);txt(x+104,y+5,&FONT_S,value,C_WHITE,2);
}
static void pg_visuals(uint32_t band){
    static const char *fx[]={"OFF","FIREWORKS","SPARKLES","CONFETTI","SMOKE","BEAMS"};
    static const char *bg[]={"ORIGINAL","GRID","HORIZON","CONCERT","LED WALL"};
    static const char *fr[]={"NONE","CORNERS","RAILS"};
    const WardrobeSprites *w=WD_OUTFITS+performance.outfit;
    char s[32];uint32_t x,y;uint8_t index;
    cv_rect(0,band*48u,240,48,LOOK_BG);
    if(!band){ui_panel(1,0,238,47,1);
        txt(8,3,&FONT_S,"SELECT OUTFIT",C_TEL,0);s[0]=0;catn(s,performance.outfit+1u);cat(s," / 12");txt(232,3,&FONT_S,s,LOOK_MUTED,2);
        txt(8,27,&FONT_S,w->name,C_WHITE,0);if(modified)cv_rect(228,32,5,5,C_PNK);
    }else if(band<3u){
        /* Character keeps its aspect ratio; stage geometry fits the preview card. */
        for(y=(band-1u)*48u;y<band*48u;y++)for(x=0;x<140u;x++){
            uint32_t px=x*240u/140u,py=y*214u/96u;
            uint16_t colour=perf_pixel(&performance,px,py);index=0;
            if(x>=29u&&x<111u&&spr_pixel(w->stage,performance.sprite,(x-29u)*71u/96u,y*71u/96u,&index)&&index)colour=w->stage_palette[index];
            cv_rect(8+(int32_t)x,48+(int32_t)y,1,1,colour);
        }
        ui_panel(156,48,76,64,0);
        if(band==1u)for(y=0;y<44u;y++)for(x=0;x<44u;x++){
            index=0;if(spr_pixel(w->faces,0,x*90u/44u,y*90u/44u,&index)&&index)cv_rect(172+(int32_t)x,52+(int32_t)y,1,1,w->face_palette[index]);
        }
        if(band==2u){
            txt(194,96,&FONT_S,"PRESETS",LOOK_MUTED,1);
            ui_panel(156,116,76,28,1);txt(194,122,&FONT_S,performance.palette?"WARM":"STYLE",C_TEL,1);
        }
        cv_rect(8,143,140,1,C_TEL);
    }else{
        look_tile(8,150,"K1",bg[performance.background],C_TEL);
        look_tile(122,150,"K2",fr[performance.frame],C_TEL);
        look_tile(8,180,"K3",fx[performance.effect],C_PNK);
        s[0]=0;catn(s,performance.intensity);cat(s,"%");look_tile(122,180,"K4",s,C_PNK);
        cv_rect(129,202,(int32_t)performance.intensity,2,C_PNK);
        for(x=0;x<WD_OUTFIT_N;x++)cv_rect(9+(int32_t)x*19,211,13,2,x==performance.outfit?C_TEL:LOOK_CARD);
        txt(120,220,&FONT_S,"REC FX  EDIT BACK  SAVE",LOOK_MUTED,1);
    }
}

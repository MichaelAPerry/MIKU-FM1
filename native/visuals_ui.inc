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
    ui_back(band);
    if(!band){ui_header();ui_badge();
        txt(30,8,&FONT_S,"SELECT OUTFIT",C_TEL,0);s[0]=0;catn(s,performance.outfit+1u);cat(s," / 12");txt(232,8,&FONT_S,s,LOOK_MUTED,2);
        txt(8,32,&FONT_S,w->name,C_WHITE,0);if(modified)cv_rect(228,37,5,5,C_PNK);
    }else{
        /* Larger character preview; preserve character aspect ratio and paired face. */
        uint32_t first=band*48u,last=first+48u;if(first<48u)first=48u;if(last>174u)last=174u;
        for(y=first;y<last;y++)for(x=0;x<140u;x++){
            uint32_t py=(y-48u)*214u/126u;uint16_t colour=perf_pixel(&performance,x*240u/140u,py);index=0;
            if(x>=16u&&x<124u&&spr_pixel(w->stage,performance.sprite,(x-16u)*71u/126u,(y-48u)*71u/126u,&index)&&index)colour=w->stage_palette[index];
            cv_rect(8+(int32_t)x,(int32_t)y,1,1,colour);
        }
        ui_panel(156,48,76,66,0);
        for(y=0;y<44u;y++)if(50u+y>=band*48u&&50u+y<(band+1u)*48u)for(x=0;x<44u;x++){
            index=0;if(spr_pixel(w->faces,0,x*90u/44u,y*90u/44u,&index)&&index)cv_rect(172+(int32_t)x,50+(int32_t)y,1,1,w->face_palette[index]);
        }
        txt(194,98,&FONT_S,"PRESETS",LOOK_MUTED,1);
        ui_panel(156,118,76,24,1);txt(194,122,&FONT_S,performance.palette?"WARM":"STYLE",C_TEL,1);
        txt(194,146,&FONT_S,"EDIT BACK",LOOK_MUTED,1);txt(194,162,&FONT_S,"SAVE KEEP",C_PNK,1);
        cv_rect(8,174,140,1,C_TEL);

        look_tile(8,180,"K1",bg[performance.background],C_TEL);
        look_tile(122,180,"K2",fr[performance.frame],C_TEL);
        look_tile(8,210,"K3",fx[performance.effect],C_PNK);
        s[0]=0;catn(s,performance.intensity);cat(s,"%");look_tile(122,210,"K4",s,C_PNK);
        cv_rect(129,232,(int32_t)performance.intensity,2,C_PNK);
        for(x=0;x<WD_OUTFIT_N;x++)cv_rect(9+(int32_t)x*19,176,13,2,x==performance.outfit?C_TEL:LOOK_CARD);

    }
}

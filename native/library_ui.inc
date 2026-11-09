/* SPDX-License-Identifier: GPL-3.0-only */
static void pg_library(uint32_t band){
    char s[32],name[9];uint32_t first=library_cursor/4u*4u,slot,y=band*48u;
    ui_back(band);
    if(!band){ui_header();ui_badge();txt(30,8,&FONT_S,playing?"LIBRARY / PLAYING":"SONG LIBRARY",C_TEL,0);txt(232,8,&FONT_S,"LFO LOOK",LOOK_MUTED,2);txt(8,32,&FONT_S,"EDIT LOAD",C_WHITE,0);txt(232,32,&FONT_S,"REC SAVE",LOOK_MUTED,2);return;}
    slot=first+band-1u;if(slot>=NSLOT)return;
    ui_panel(8,y+1,224,43,slot==library_cursor);
    if(slot==library_cursor){cv_rect(8,y+1,3,43,C_TEL);cv_rect(12,y+42,220,2,C_TEL);}
    s[0]=0;catn(s,slot+1u);txt(27,y+14,&FONT_S,s,slot==library_cursor?C_TEL:LOOK_MUTED,1);
    s[0]=0;
    if(slot==pf.slot){uint32_t k;for(k=0;k<8u;k++)name[k]=sg.name[k];name[8]=0;cat(s,name);if(modified)cat(s,"*");cv_rect(220,y+9,5,5,C_PNK);}
    else if(slot_name(slot,name))cat(s,name);else cat(s,"EMPTY");
    txt(46,y+4,&FONT_S,s,C_WHITE,0);
    txt(46,y+24,&FONT_S,slot<SF_COUNT?sf_title(slot):"YOUR SONG",LOOK_MUTED,0);
}

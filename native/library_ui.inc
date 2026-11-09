/* SPDX-License-Identifier: GPL-3.0-only */
static void pg_library(uint32_t band){
    char s[32],name[9];uint32_t first=library_cursor/4u*4u,slot;
    if(!band){txt(120,3,&FONT_S,playing?"SONGS  PLAYING":"SONG LIBRARY",C_PNK,1);txt(120,25,&FONT_S,"EDIT LOAD  REC SAVE",C_WHITE,1);return;}
    slot=first+band-1u;if(slot>=NSLOT)return;
    if(slot==library_cursor)cv_rect(0,band*48u,240,46,RGB(30,48,72));
    s[0]=0;catn(s,slot+1u);cat(s," ");
    if(slot==pf.slot){uint32_t k;for(k=0;k<8u;k++)name[k]=sg.name[k];name[8]=0;cat(s,name);if(modified)cat(s,"*");}
    else if(slot_name(slot,name))cat(s,name);else cat(s,"EMPTY");
    txt(8,band*48u+2,&FONT_S,s,slot==library_cursor?C_PNK:C_WHITE,0);
    if(slot<SF_COUNT)txt(8,band*48u+24,&FONT_S,sf_title(slot),RGB(150,160,190),0);
    else txt(8,band*48u+24,&FONT_S,"YOUR SONG",RGB(150,160,190),0);
}

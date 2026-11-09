/* SPDX-License-Identifier: GPL-3.0-only */
static const char *const dx_pages[DB_PAGES]={"BROWSE","GLOBAL","OP FREQUENCY","OP ENV RATES","OP ENV LEVELS","KEYBOARD SCALE","MODULATION","PITCH RATES","PITCH LEVELS","MIX / TUNING","CURVES / TOUCH"};
static const char *const dx_labels[DB_PAGES][4]={
 {"","","",""},{"K1 ALG","K2 FB","K3 LFO","K4 LVL"},{"K1 OUT","K2 CRS","K3 FINE","K4 DET"},
 {"K1 R1","K2 R2","K3 R3","K4 R4"},{"K1 L1","K2 L2","K3 L3","K4 L4"},
 {"K1 BREAK","K2 LEFT","K3 RIGHT","K4 RATE"},{"K1 DELAY","K2 PMD","K3 AMD","K4 PMS"},
 {"K1 R1","K2 R2","K3 R3","K4 R4"},{"K1 L1","K2 L2","K3 L3","K4 L4"},
 {"K1 LVL","K2 PAN","K3 TRANS","K4 SYNC"},{"K1 LEFT","K2 RIGHT","K3 VEL","K4 AMS"}};
static void dx_title(char *out,const uint8_t *patch){uint32_t i;for(i=0;i<10u;i++){uint8_t c=patch[118u+i];out[i]=(char)(c>='a'&&c<='z'?c-32:c<32||c>126?' ':c);}out[10]=0;}
static void pg_bank(uint32_t band){
    const uint8_t *p=dx_page?sg.dx7[trk-1u]:dx7_bank[dx_cursor];char s[32],name[11];uint32_t i;
    dx_title(name,p);
    if(!band){head("DX7 BANK",dx_page?dx_pages[dx_page]:"SELECT BROWSE   REC LOAD",C_TEL);if(modified)cv_rect(228,38,5,5,C_PNK);return;}
    ui_panel(8,52,224,86,1);
    s[0]=0;cat(s,dx_page?"ALGO ":"FACTORY ");catn(s,dx_page?(uint32_t)p[110]+1u:dx_cursor+1u);
    if(!dx_page)cat(s," / 8");else{cat(s," FB ");catn(s,p[111]&7u);}
    txt(16,57,&FONT_S,s,LOOK_MUTED,0);s[0]=0;cat(s,"6 OP");if(dx_page){cat(s," / ");catn(s,dx_op+1u);}txt(224,57,&FONT_S,s,C_TEL,2);
    txt(16,79,&FONT_L,name,C_WHITE,0);
    if(!dx_page){
        txt(16,117,&FONT_S,"REC LOAD / KEYS PLAY LOADED",C_PNK,0);
        s[0]=0;cat(s,"LOADED: ");if(db_active(&sg,trk)){char n[11];dx_title(n,sg.dx7[trk-1u]);cat(s,n);}else cat(s,pnm[trk][en_pre(trk,sg.trk[trk].preset)]);
        txt(8,146,&FONT_S,s,C_TEL,0);txt(8,164,&FONT_S,"PATCH OPERATOR LEVELS",LOOK_MUTED,0);
        for(i=0;i<6;i++){uint32_t h=(uint32_t)p[i*17+14]*26u/99u;cv_rect(12+(int32_t)i*38,208-(int32_t)h,26,(int32_t)h,i==5?C_PNK:C_TEL);s[0]=0;catn(s,6u-i);txt(25+(int32_t)i*38,210,&FONT_S,s,C_WHITE,1);}
        txt(120,228,&FONT_S,"PRESETS / EDIT: PARAMETERS",LOOK_MUTED,1);return;
    }
    if(dx_page==2u)txt(16,117,&FONT_S,(p[(5u-dx_op)*17u+15u]&1u)?"ARP MODE: FIXED":"ARP MODE: RATIO",C_PNK,0);
    else if(dx_page==6u){s[0]=0;cat(s,"ARP LFO WAVE ");catn(s,(p[116]>>1)&7u);txt(16,117,&FONT_S,s,C_PNK,0);}
    else txt(16,117,&FONT_S,sg.trk[trk].preset==SG_DX7_CUSTOM?"EDITED PATCH":"FACTORY PATCH",C_PNK,0);
    for(i=0;i<4;i++){
        int32_t x=i&1u?122:8,y=i<2?146:176;uint32_t v=dx_page==9u&&i==1u?pf.pan[trk]:db_value(&sg,trk,dx_op,dx_page,i),max=db_limit(dx_page,i);
        ui_panel(x,y,110,26,1);txt(x+6,y+3,&FONT_S,dx_labels[dx_page][i],LOOK_MUTED,0);s[0]=0;
        if(dx_page==2u&&i==3u){if(v<7u){cat(s,"-");catn(s,7u-v);}else catn(s,v-7u);}
        else if(dx_page==9u&&i==2u){if(v<24u){cat(s,"-");catn(s,24u-v);}else catn(s,v-24u);}
        else catn(s,v+(dx_page==1u&&i==0u?1u:0u));
        txt(x+104,y+3,&FONT_S,s,C_WHITE,2);cv_rect(x+6,y+22,(int32_t)(v*98u/max),2,C_PNK);
    }
    txt(120,204,&FONT_S,"SELECT OP  PRESETS PAGE",LOOK_MUTED,1);txt(120,222,&FONT_S,"KEYS TRY  REC BACK  SAVE",C_TEL,1);
}

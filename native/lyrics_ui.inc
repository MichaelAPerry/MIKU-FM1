/* SPDX-License-Identifier: GPL-3.0-only */
/* Native lyric unit editor. Uses the existing vocal patterns/undo/slot format. */
static LyUnit lyric_units[NSTEP],lyric_clip[NSTEP];
static uint8_t lyric_notes[NSTEP];
static uint32_t lyric_count,lyric_cursor,lyric_clip_count;
static int32_t lyric_error;
static uint32_t lyric_pattern(void){return crow<sg.nrows?sg.row[crow].pat[TR_VOCAL]:ROW_OFF;}
static void lyric_load(void){
    uint32_t p=lyric_pattern();
    lyric_count=0;lyric_error=p<NPAT?ly_read(&sg,p,lyric_units,lyric_notes,NSTEP,&lyric_count):LE_ARGUMENT;
    if(lyric_cursor>=lyric_count)lyric_cursor=lyric_count?lyric_count-1u:0;
    if(lyric_count && !(lyric_units[lyric_cursor].flags&LY_REST))ed_mora=lyric_units[lyric_cursor].mora;
    mk(0x1Fu);
}
static uint32_t lyric_commit(uint32_t retain_pitch){
    uint32_t p=lyric_pattern(),sum=0,i;
    int rc;
    if(p>=NPAT || !lyric_count)return 0;
    for(i=0;i<lyric_count;i++)sum+=lyric_units[i].ticks;
    /* Consume only trailing silence when increasing a syllable duration. */
    while(sum>NSTEP && lyric_count>1u && (lyric_units[lyric_count-1u].flags&LY_REST)){
        LyUnit *tail=&lyric_units[lyric_count-1u];uint32_t take=sum-NSTEP;
        if(take>=tail->ticks){sum-=tail->ticks;lyric_count--;}
        else{tail->ticks=(uint16_t)(tail->ticks-take);sum-=take;}
    }
    if(sum>NSTEP){lyric_load();toast("PHRASE EXCEEDS 16 STEPS",1);return 0;}
    u_pushpat(TR_VOCAL,p);lk();
    rc=ly_apply(&sg,p,lyric_units,lyric_count,retain_pitch?0:lyric_notes,(uint32_t)cl(60+oct*12,1,127),ed_vel);
    ulk();
    if(rc==LE_OK){modified=1;lyric_load();return 1;}
    lyric_load();toast(rc==LE_SUSTAIN?"HOLD NEEDS PREVIOUS VOWEL":"INVALID LYRIC EDIT",1);return 0;
}
static void lyric_insert(void){
    uint32_t i,at=lyric_cursor;LyUnit u;
    if(!lyric_count || lyric_error){toast("CHOOSE VOCAL PATTERN",1);return;}
    u=lyric_units[at];
    if(u.flags&LY_REST){
        lyric_units[at].ticks=1;lyric_units[at].mora=(uint8_t)ed_mora;lyric_units[at].flags=LY_WORD_END;
        lyric_notes[at]=(uint8_t)cl(60+oct*12,1,127);
        if(u.ticks>1u){
            if(lyric_count==NSTEP){lyric_load();return;}
            for(i=lyric_count;i>at+1u;i--){lyric_units[i]=lyric_units[i-1u];lyric_notes[i]=lyric_notes[i-1u];}
            lyric_count++;u.ticks--;lyric_units[at+1u]=u;lyric_notes[at+1u]=0;
        }
    }else{
        if(lyric_count==NSTEP){toast("PHRASE FULL",1);return;}
        for(i=lyric_count;i>at+1u;i--){lyric_units[i]=lyric_units[i-1u];lyric_notes[i]=lyric_notes[i-1u];}
        lyric_count++;lyric_cursor=++at;
        lyric_units[at].ticks=1;lyric_units[at].mora=(uint8_t)ed_mora;lyric_units[at].flags=LY_WORD_END;
        lyric_notes[at]=(uint8_t)cl(60+oct*12,1,127);
    }
    lyric_commit(0);
}
static void lyric_delete(void){
    uint32_t i;
    if(!lyric_count)return;
    if(lyric_count==1u){lyric_units[0].ticks=NSTEP;lyric_units[0].mora=0;lyric_units[0].flags=LY_REST|LY_WORD_END;lyric_notes[0]=0;}
    else{for(i=lyric_cursor+1u;i<lyric_count;i++){lyric_units[i-1u]=lyric_units[i];lyric_notes[i-1u]=lyric_notes[i];}lyric_count--;}
    lyric_commit(0);
}
static void lyric_token(int32_t delta){
    LyUnit *u;uint32_t token;
    if(!lyric_count)return;u=&lyric_units[lyric_cursor];
    token=u->flags&LY_REST?0:u->flags&LY_SUSTAIN?1:2u+u->mora;
    token=(uint32_t)wr((int32_t)token+delta,MORA_N+2);
    u->flags&=(uint8_t)~(LY_REST|LY_SUSTAIN);
    if(!token){u->flags|=LY_REST|LY_WORD_END;u->mora=0;lyric_notes[lyric_cursor]=0;}
    else if(token==1u){
        if(!lyric_cursor || (lyric_units[lyric_cursor-1u].flags&LY_REST)){lyric_load();toast("HOLD NEEDS PREVIOUS VOWEL",1);return;}
        u->mora=lyric_units[lyric_cursor-1u].mora;u->flags|=LY_SUSTAIN;u->flags&=(uint8_t)~LY_SEPARATE;
    }else{u->mora=(uint8_t)(token-2u);ed_mora=u->mora;}
    if(!(u->flags&LY_REST)&&!lyric_notes[lyric_cursor])lyric_notes[lyric_cursor]=(uint8_t)cl(60+oct*12,1,127);
    lyric_commit(0);
}
static void lyric_flag(uint32_t button){
    static const uint8_t flags[4]={LY_WORD_END,LY_PHRASE_END,LY_SEPARATE,LY_SUSTAIN};
    LyUnit *u;if(!lyric_count)return;u=&lyric_units[lyric_cursor];
    if(button==3u && !(u->flags&LY_SUSTAIN)){
        if(!lyric_cursor || (lyric_units[lyric_cursor-1u].flags&LY_REST)){toast("HOLD NEEDS PREVIOUS VOWEL",1);return;}
        u->mora=lyric_units[lyric_cursor-1u].mora;u->flags&=(uint8_t)~(LY_REST|LY_SEPARATE);
    }
    u->flags^=flags[button];lyric_commit(0);
}
static void lyric_key(uint32_t note){
    if(!lyric_count)return;
    if(lyric_units[lyric_cursor].flags&LY_REST){lyric_units[lyric_cursor].flags=LY_WORD_END;lyric_units[lyric_cursor].mora=(uint8_t)ed_mora;}
    lyric_notes[lyric_cursor]=(uint8_t)note;lyric_commit(0);
}
static void pg_lyrics(uint32_t band){
    char text[32];uint32_t i,p=lyric_pattern(),at=(lyric_cursor/4u)*4u+band-1u;
    if(!band){
        text[0]=0;cat(text,"R");catn(text,crow+1u);cat(text," P");if(p<NPAT)catn(text,p+1u);else cat(text,"--");
        head("LYRICS",text,C_TEL);return;
    }
    if(lyric_error){
        if(band==1u)txt(4,56,&FONT_S,"OPEN VOCAL PATTERN WITH K3",C_WHITE,0);
        if(band==2u)txt(4,104,&FONT_S,"LONG/OVERLAPPING NOTES:",C_WHITE,0);
        if(band==3u)txt(4,152,&FONT_S,"EDIT LENGTHS IN PATTERN",C_WHITE,0);
        return;
    }
    if(at>=lyric_count)return;
    {LyUnit *u=&lyric_units[at];int32_t y=52+(int32_t)(band-1u)*48;
     if(at==lyric_cursor)cv_rect(0,y-2,240,44,RGB(30,48,72));
     text[0]=0;catn(text,at+1u);cat(text," ");cat(text,u->flags&LY_REST?"REST":u->flags&LY_SUSTAIN?"HOLD":mora_str(u->mora));
     txt(5,y,&FONT_L,text,at==lyric_cursor?C_PNK:C_WHITE,0);
     text[0]=0;cat(text,"LEN ");catn(text,u->ticks);
     if(!(u->flags&LY_REST)){static const char *const pitch[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};uint32_t n=lyric_notes[at];cat(text," ");cat(text,pitch[n%12u]);if(n>=12u)catn(text,n/12u-1u);}
     txt(236,y,&FONT_S,text,C_TEL,2);
     text[0]=0;
     if(u->flags&LY_WORD_END)cat(text,"WORD ");if(u->flags&LY_PHRASE_END)cat(text,"PHRASE ");
     if(u->flags&LY_SEPARATE)cat(text,"SEP ");else if(!(u->flags&LY_REST))cat(text,"LEGATO ");
     txt(5,y+24,&FONT_S,text,RGB(170,180,200),0);
    }
    (void)i;
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* PHRASE (0.9): the phrase bank on the FM-1 (docs/ENGLISH_SINGING.md sections 3b, 4, 5).
 * 64 phrases of 1 KiB at PHRASES_AT (4 a sector). SELECT picks one; each key or MIDI note sings its next syllable at
 * that pitch (English through the engine's live slots, Japanese as morae); ALGORITHM moves the cursor; REC places the
 * phrase into the song from the cursor row; EDIT opens the typing editor (a letter a key, A-Z then space), where
 * ALGORITHM changes the character before the cursor (digits, punctuation, romaji marks), SELECT moves the cursor, REC
 * deletes, K1 switches English / Japanese, EDIT again compiles and stores it, GLO leaves without storing. */
static Phrase ph_cur,ph_new;                  /* the selected phrase; the one the editor builds */
static uint8_t ph_rec[PH_REC] __attribute__((section(".pool")));
static uint32_t ph_idx,ph_pos,ph_ok,ph_live,ph_editing,ph_ecur,ph_elang;
static char ph_etext[PH_TEXT];
static const char PH_CHARS[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789',.?!-:_/|";
static void ph_load(uint32_t i){
    ph_idx=i<PH_N?i:0;ph_pos=0;
    ph_ok=!st_read(PHRASES_AT+ph_idx*PH_REC,ph_rec,PH_REC)&&!ph_decode(ph_rec,&ph_cur);
    if(!ph_ok){uint32_t k;ph_cur.n=0;ph_cur.text[0]=0;ph_cur.lang=PH_EN;for(k=0;k<12u;k++)ph_cur.name[k]=' ';}
    mk(0x1Fu);
}
static int ph_store(uint32_t i,const Phrase *p){   /* read-modify-write the phrase's sector; only while stopped (an erase mutes ~50 ms) */
    uint32_t sec=PHRASES_AT+(i/4u)*0x1000u,took,t=fm1_ms;
    if(playing||i>=PH_N)return -1;
    _Static_assert(sizeof enc>=0x1000u,"phrase store borrows the song I/O buffer for one sector");
    if(st_read(sec,enc,0x1000u))return -2;
    if(ph_encode(p,enc+(i%4u)*PH_REC))return -3;
    en_post(&en,EN_HUSH,0,0,0);
    while(en.act&&fm1_ms-t<800u)platform_poll();
    if(fl_erase4k(sec,&took))return -4;
    return fl_write(sec,enc,0x1000u)?-5:0;
}
/* the mora byte the next vocal note sings: the next syllable of the selected phrase (wrapping), else the STAGE syllable */
static uint32_t ph_next(void){
    uint32_t tries;
    if(!ph_ok||!ph_cur.n)return ed_mora;
    for(tries=0;tries<ph_cur.n;tries++){
        const PhSyl *s=&ph_cur.s[ph_pos];
        ph_pos=ph_pos+1u<ph_cur.n?ph_pos+1u:0;mk(0x1Eu);
        if(ph_cur.lang==PH_JP){if(s->ph[0])return s->ph[0];continue;}   /* a Japanese rest is mora 0: skipped live */
        {FvSyl f;ph_fvsyl(s,&f);ph_live=(ph_live+1u)%EN_NLIVE;en_set_live(&en,ph_live,&f);return EN_LIVE0+ph_live;}
    }
    return ed_mora;
}
static uint32_t vmora(uint32_t kt){return kt==TR_VOCAL&&page==PG_PHRASE&&!ph_editing?ph_next():ed_mora;}
static void ph_place_here(void){                  /* REC: the phrase into the VOCAL patterns from the cursor row on */
    uint32_t p,used=0,k,npat=0;int r;
    if(!ph_ok||!ph_cur.n){toast("EMPTY PHRASE - EDIT TO WRITE",1);return;}
    if(crow>=sg.nrows||(p=sg.row[crow].pat[TR_VOCAL])>=NPAT){toast("OPEN A VOCAL PATTERN (LYRICS K3)",1);return;}
    /* the phrase may run on into the following patterns, but only into empty ones, and only where rows exist for them */
    while(npat<4u&&p+npat<NPAT&&(npat==0||(sg_pat_empty(&sg,TR_VOCAL,(int)(p+npat))&&crow+npat<sg.nrows)))npat++;
    un_push_rows(&un,&sg);for(k=0;k<npat;k++)un_push_pat(&un,&sg,TR_VOCAL,(int)(p+k));u_kind=0;   /* exact snapshots, not merged with earlier edits */
    lk();r=ph_place(&sg,p,npat,&ph_cur,(uint8_t)cl(60+oct*12,1,127),&used);
    if(!r)for(k=1;k<used;k++)sg_row_set(&sg,(int)(crow+k),TR_VOCAL,(int)(p+k));
    ulk();
    if(r){                                        /* ph_place left the song as it was: drop the snapshots taken for it */
        lk();for(k=0;k<=npat;k++)un_pop(&un,&sg);ulk();
        toast(sg.nsyl>=SG_NSYL?"SONG SYLLABLE TABLE FULL":"PHRASE DOES NOT FIT THESE PATTERNS",1);return;
    }
    for(k=used;k<npat;k++){lk();un_pop(&un,&sg);ulk();}   /* drop the snapshots of patterns it did not need (pushed last, so popped first; they are unchanged) */
    modified=1;mk(0x1Fu);toast(used>1u?"PLACED OVER ROWS - GLO UNDOES PART BY PART":"PLACED - GLO UNDOES",0);
}
/* ---- the typing editor */
static void ph_edit_begin(void){
    uint32_t k;
    for(k=0;k<PH_TEXT;k++)ph_etext[k]=ph_ok?ph_cur.text[k]:0;
    ph_etext[PH_TEXT-1u]=0;
    for(ph_ecur=0;ph_etext[ph_ecur];ph_ecur++){}
    ph_elang=ph_ok?ph_cur.lang:PH_EN;ph_editing=1;mk(0x1Fu);
}
static void ph_insert(char c){
    uint32_t n,k;
    for(n=0;ph_etext[n];n++){}
    if(n+1u>=PH_TEXT){toast("TEXT FULL",1);return;}
    for(k=n+1u;k>ph_ecur;k--)ph_etext[k]=ph_etext[k-1u];
    ph_etext[ph_ecur++]=c;mk(0x1Eu);
}
static void ph_backspace(void){
    uint32_t k;
    if(!ph_ecur)return;
    for(k=ph_ecur-1u;ph_etext[k];k++)ph_etext[k]=ph_etext[k+1u];
    ph_ecur--;mk(0x1Eu);
}
static void ph_cycle(int32_t d){                  /* ALGORITHM: the character before the cursor through PH_CHARS */
    uint32_t i;
    if(!ph_ecur){ph_insert(' ');return;}
    for(i=0;PH_CHARS[i]&&PH_CHARS[i]!=ph_etext[ph_ecur-1u];i++){}
    if(!PH_CHARS[i])i=0;
    ph_etext[ph_ecur-1u]=PH_CHARS[wr((int32_t)i+d,(int32_t)sizeof PH_CHARS-1)];mk(0x1Eu);
}
static void ph_key_type(uint32_t k){ph_insert(k<26u?(char)(ph_elang==PH_JP?'a'+k:'A'+k):' ');}
static void ph_edit_done(void){
    int err=-1,r;uint32_t k;
    if(!ph_etext[0]){toast("TYPE SOMETHING FIRST",1);return;}
    ph_new=ph_cur;
    if(!ph_ok){
        ph_new.bpm=sg.bpm;ph_new.deflen=2;ph_new.flags=0;
        for(k=0;k<12u;k++)ph_new.name[k]=' ';
        {static const char nm[]="PHRASE ";for(k=0;k<7u;k++)ph_new.name[k]=nm[k];ph_new.name[7]=(char)('0'+(ph_idx+1u)/10u);ph_new.name[8]=(char)('0'+(ph_idx+1u)%10u);}
    }
    r=ph_compile(ph_etext,ph_elang,&ph_new,&err);
    if(r){static char m[28];m[0]=0;cat(m,"CANNOT READ AT ");catn(m,(uint32_t)(err<0?0:err)+1u);toast(m,1);if(err>=0)ph_ecur=(uint32_t)err;mk(0x1Eu);return;}
    if(playing){toast("STOP FIRST, THEN EDIT AGAIN",1);return;}
    r=ph_store(ph_idx,&ph_new);
    if(r){toast("PHRASE NOT STORED",1);return;}
    ph_editing=0;ph_load(ph_idx);toast("PHRASE STORED",0);
}
/* ---- drawing */
static void ph_line(char *out,const char *s,uint32_t from,uint32_t n){uint32_t k;for(k=0;k<n&&s[from+k];k++)out[k]=s[from+k];out[k]=0;}
static void pg_phrase(uint32_t band){
    char text[40];
    if(!band){
        text[0]=0;if(ph_idx+1u<10u)cat(text,"0");catn(text,ph_idx+1u);cat(text," ");
        if(ph_ok){char nm[13];uint32_t k;for(k=0;k<12u;k++)nm[k]=ph_cur.name[k];nm[12]=0;cat(text,nm);cat(text,ph_cur.lang==PH_JP?" JP":" EN");}
        else cat(text,"EMPTY");
        head(ph_editing?"TYPE":"PHRASE",text,C_TEL);return;
    }
    if(ph_editing){                               /* the text in 4 lines of 28, the cursor shown as | */
        static char shown[PH_TEXT+2u];uint32_t k,j=0;
        for(k=0;ph_etext[k]||k==ph_ecur;k++){if(k==ph_ecur)shown[j++]='|';if(!ph_etext[k])break;shown[j++]=ph_etext[k];}
        shown[j]=0;
        if(band<=3u){ph_line(text,shown,(band-1u)*2u*28u,28u);txt(4,52+(int32_t)(band-1u)*48,&FONT_S,text,C_WHITE,0);
            ph_line(text,shown,((band-1u)*2u+1u)*28u,28u);txt(4,76+(int32_t)(band-1u)*48,&FONT_S,text,C_WHITE,0);}
        else{txt(4,200,&FONT_S,ph_elang==PH_JP?"KEYS a-z  ALG MARKS  K1 JP":"KEYS A-Z  ALG 0-9 ,.?  K1 EN",C_TEL,0);
             txt(4,220,&FONT_S,"REC DEL  EDIT SAVE  GLO EXIT",RGB(170,180,200),0);}
        return;
    }
    if(!ph_ok||!ph_cur.n){
        if(band==2u)txt(120,100,&FONT_S,"EMPTY - EDIT TO WRITE WORDS",C_WHITE,1);
        if(band==3u)txt(120,148,&FONT_S,"OR SEND FROM THE WEB EDITOR",RGB(170,180,200),1);
        return;
    }
    if(band==1u){                                 /* the syllable the next note sings, big */
        const PhSyl *s=&ph_cur.s[ph_pos];
        ui_panel(2,50,236,44,1);
        ph_line(text,ph_cur.lang==PH_JP?mora_str(s->ph[0]):s->spell,0,6u);
        txt(120,56,&FONT_L,text[0]?text:"-",C_PNK,1);
    }else if(band==2u){                           /* the next three */
        uint32_t k,j=ph_pos;text[0]=0;
        for(k=0;k<3u;k++){j=j+1u<ph_cur.n?j+1u:0;cat(text,ph_cur.lang==PH_JP?mora_str(ph_cur.s[j].ph[0]):ph_cur.s[j].spell);cat(text,"  ");}
        txt(120,108,&FONT_S,text,C_WHITE,1);
    }else if(band==3u){                           /* the words */
        ph_line(text,ph_cur.text,0,28u);txt(4,148,&FONT_S,text,RGB(170,180,200),0);
        ph_line(text,ph_cur.text,28u,28u);txt(4,166,&FONT_S,text,RGB(170,180,200),0);
    }else{
        text[0]=0;cat(text,"SYL ");catn(text,ph_pos+1u);cat(text,"/");catn(text,ph_cur.n);cat(text,"  BPM ");catn(text,ph_cur.bpm);
        txt(4,200,&FONT_S,text,C_TEL,0);
        txt(4,220,&FONT_S,"KEYS SING  REC PLACE  EDIT",RGB(170,180,200),0);
    }
}
/* ---- USB: Miku SysEx 0x7D (docs/ENGLISH_SINGING.md section 7). Called from app_sysex in the main loop. The web
 * editor reads and writes phrases (stored at once while stopped, "busy" while playing) and sends songs, which wait
 * here until REC loads them (10 s), as a DX7 bank import does. Buffers live in the pool, not in static RAM. */
#define POOL_BUF __attribute__((section(".pool")))
static uint8_t mkx_scratch[MKX_RAW_CAP] POOL_BUF,mkx_songbuf[SONG_ENC_CAP] POOL_BUF,mkx_reply[MKX_FRAME_CAP] POOL_BUF;
static Song mkx_song POOL_BUF;
static uint32_t mkx_song_until,mkx_song_ok;
static int mkx_pget(void *u,uint8_t i,uint8_t rec[PH_REC]){
    uint32_t k;(void)u;
    if(i>=PH_N||st_read(PHRASES_AT+(uint32_t)i*PH_REC,rec,PH_REC))return -1;
    if(ph_decode(rec,0))for(k=0;k<PH_REC;k++)rec[k]=0;   /* an empty (or foreign) record reads as zeros */
    return 0;
}
static int mkx_pput(void *u,uint8_t i,const uint8_t rec[PH_REC]){
    static char m[24];(void)u;
    if(i>=PH_N||ph_decode(rec,&ph_new)||ph_store(i,&ph_new))return -1;
    if(page==PG_PHRASE&&!ph_editing&&i==ph_idx)ph_load(i);
    m[0]=0;cat(m,"PHRASE ");catn(m,(uint32_t)i+1u);cat(m," RECEIVED");toast(m,0);
    return 0;
}
static int mkx_list(void *u,uint8_t e[MKX_LIST_SIZE]){
    uint32_t i,k;(void)u;
    for(i=0;i<PH_N;i++){
        uint8_t *x=e+i*14u;
        int ok=!st_read(PHRASES_AT+i*PH_REC,ph_rec,PH_REC)&&!ph_decode(ph_rec,0);
        x[0]=(uint8_t)ok;x[1]=ok?ph_rec[5]:0;
        for(k=0;k<12u;k++)x[2u+k]=ok?ph_rec[12u+k]:(uint8_t)' ';
    }
    return 0;
}
static void mkx_got_song(void *u,const Song *g){(void)u;(void)g;mkx_song_ok=1;mkx_song_until=fm1_ms+10000u;toast("SONG RECEIVED - REC LOADS IT",0);toast_until=mkx_song_until;}
static MkxHost mkx_host;
static void mkx_app(const uint8_t *p,uint32_t n){
    int r;
    if(!mkx_host.scratch){
        mkx_host.phrase_get=mkx_pget;mkx_host.phrase_put=mkx_pput;mkx_host.bank_list=mkx_list;mkx_host.song_end=mkx_got_song;
        mkx_host.version=FELUCCA_ID;mkx_host.scratch=mkx_scratch;mkx_host.song_buffer=mkx_songbuf;mkx_host.song=&mkx_song;
    }
    mkx_host.busy=(uint8_t)(playing!=0);
    r=mkx_handle(p,n,&mkx_host,mkx_reply,sizeof mkx_reply);
    if(r>0)ota_wire_send(mkx_reply,(uint32_t)r);
}
static uint32_t mkx_song_pending(uint32_t now){
    if(mkx_song_ok&&(int32_t)(now-mkx_song_until)>=0)mkx_song_ok=0;
    return mkx_song_ok;
}
static void mkx_song_apply(void){                 /* REC on the prompt: the received song replaces the current one (not saved yet) */
    mkx_song_ok=0;stop_wait();
    lk();sg=mkx_song;ulk();
    after_load();modified=1;toast("SONG LOADED - SAVE KEEPS IT",0);
}

/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/lyrics.h"
#include "firmware/song/mora.h"
static int ly_space(char c){return c==' '||c=='\t'||c=='\n'||c=='\r';}
static int ly_letter(char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');}
static int ly_parse(const char *s,uint16_t def,LyUnit *out,uint32_t *total,uint32_t *err){
    uint32_t p=0,n=0,last=0,need=0;
    while(ly_space(s[p]))p++;
    while(s[p]){
        uint32_t start=p,d=def,flags=0,mo=0,k=0;
        char token[4];
        if(s[p]=='/'||s[p]=='|'){flags=LY_REST|(s[p]=='|'?LY_PHRASE_END:0);p++;}
        else if(s[p]=='_'){
            if(!last||mora_vowel(last)==MV_N){*err=p;return LY_MORA;}
            mo=last;flags=LY_SUSTAIN;p++;
        }else{
            while(ly_letter(s[p])){if(k<3u)token[k]=s[p];k++;p++;}
            if(!k){*err=p;return LY_SYNTAX;}
            if(k>3u){*err=start;return LY_MORA;}
            token[k]=0;mo=mora_find(token);
            if(!mo){*err=start;return LY_MORA;}
        }
        if(s[p]==':'){
            p++;d=0;
            if(s[p]<'0'||s[p]>'9'){*err=p;return LY_DURATION;}
            while(s[p]>='0'&&s[p]<='9'){
                d=d*10u+(uint32_t)(s[p]-'0');
                if(d>65535u){*err=p;return LY_DURATION;}p++;
            }
            if(!d){*err=p;return LY_DURATION;}
        }
        if(s[p]=='!'){
            if(flags&(LY_REST|LY_SUSTAIN)){*err=p;return LY_SYNTAX;}
            flags|=LY_SEPARATE;p++;
        }
        if(s[p]&&!ly_space(s[p])&&s[p]!='-'){*err=p;return LY_SYNTAX;}
        if(!s[p]||ly_space(s[p])||(flags&LY_REST))flags|=LY_WORD_END;
        if(out){out[n].ticks=(uint16_t)d;out[n].mora=(uint8_t)mo;out[n].flags=(uint8_t)flags;}
        n++;last=flags&LY_REST?0:mo;need=0;
        if(s[p]=='-'){p++;need=1;}
        while(ly_space(s[p]))p++;
        if(need&&(!s[p]||s[p]=='/'||s[p]=='|')){*err=p;return LY_SYNTAX;}
    }
    *total=n;return LY_OK;
}
int ly_compile(const char *s,uint16_t def,LyUnit *out,uint32_t cap,uint32_t *count,uint32_t *err){
    uint32_t n=0,e=0;int rc;
    if(!s||!def||!count||(!out&&cap)){if(err)*err=0;return LY_ARGUMENT;}
    rc=ly_parse(s,def,0,&n,&e);
    if(rc){if(err)*err=e;return rc;}
    if(out&&n>cap){if(err)*err=0;return LY_CAPACITY;}
    if(out)ly_parse(s,def,out,&n,&e);
    *count=n;if(err)*err=0;return LY_OK;
}
uint32_t ly_consonant(const LyUnit *u){return !u||(u->flags&(LY_REST|LY_SUSTAIN))?MC_NONE:mora_cons(u->mora);}
uint32_t ly_vowel(const LyUnit *u){return u?mora_vowel(u->mora):MV_A;}

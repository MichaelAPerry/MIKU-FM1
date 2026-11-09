/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/lyric_edit.h"
#include "firmware/song/mora.h"
int ly_rest_step(const Step *s){return s&&!s->v[SV_NOTE]&&s->v[SV_LEN]&&(s->v[SV_FLAG]&(VF_WORD_END|VF_PHRASE_END));}
static int le_event(const Step *s){return s->v[SV_NOTE]||ly_rest_step(s);}
int ly_apply(Song *g,uint32_t pat,const LyUnit *u,uint32_t count,const uint8_t *notes,uint32_t fallback,uint32_t velocity){
    Step tmp[NSTEP];uint32_t i,k,at=0,last=0,previous_note=0;
    if(!g||pat>=NPAT||!u||!count||count>NSTEP||fallback<1u||fallback>127u||velocity>15u)return LE_ARGUMENT;
    for(i=0;i<NSTEP;i++)for(k=0;k<4;k++)tmp[i].v[k]=0;
    for(i=0;i<count;i++){
        uint32_t f=u[i].flags,mo=u[i].mora,n;
        Step *s;
        if(!u[i].ticks||u[i].ticks>NSTEP)return LE_RANGE;
        if(at+u[i].ticks>NSTEP)return LE_CAPACITY;
        if(f&~(LY_REST|LY_SEPARATE|LY_SUSTAIN|LY_WORD_END|LY_PHRASE_END))return LE_FLAGS;
        if(mo>=MORA_N)return LE_RANGE;
        if((f&LY_REST)&&(mo||(f&(LY_SEPARATE|LY_SUSTAIN))))return LE_FLAGS;
        if((f&LY_SUSTAIN)&&((f&LY_SEPARATE)||!previous_note||mora_vowel(last)==MV_N||mora_vowel(last)!=mora_vowel(mo)||(i&&u[i-1u].flags&LY_PHRASE_END)))return LE_SUSTAIN;
        s=&tmp[at];s->v[SV_LEN]=(uint8_t)u[i].ticks;
        s->v[SV_FLAG]=(uint8_t)(((f&LY_WORD_END)||(f&LY_REST)?VF_WORD_END:0)|(f&LY_PHRASE_END?VF_PHRASE_END:0));
        if(f&LY_REST){last=0;previous_note=0;}
        else{
            n=notes?notes[i]:g->pat[TR_VOCAL][pat][at].v[SV_NOTE];
            if(!n)n=fallback;
            if(n>127u)return LE_RANGE;
            s->v[SV_NOTE]=(uint8_t)n;s->v[SV_MORA]=(uint8_t)mo;s->v[SV_FLAG]|=(uint8_t)velocity;
            if(f&LY_SUSTAIN)s->v[SV_FLAG]|=VF_SUSTAIN;
            if(i+1u<count&&!(f&LY_PHRASE_END)&&!(u[i+1u].flags&(LY_REST|LY_SEPARATE)))s->v[SV_FLAG]|=VF_LEGATO;
            last=mo;previous_note=1;
        }
        at+=u[i].ticks;
    }
    for(i=0;i<NSTEP;i++)g->pat[TR_VOCAL][pat][i]=tmp[i];
    return LE_OK;
}
int ly_read(const Song *g,uint32_t pat,LyUnit *units,uint8_t *notes,uint32_t capacity,uint32_t *count){
    LyUnit tmp[NSTEP];uint8_t pitch[NSTEP];uint32_t at=0,n=0,i,prev_legato=0,prev_note=0,last=0;
    if(!g||pat>=NPAT||!units||!count)return LE_ARGUMENT;
    while(at<NSTEP){
        const Step *s=&g->pat[TR_VOCAL][pat][at];uint32_t len=s->v[SV_LEN],f=s->v[SV_FLAG],flags=0,mo=s->v[SV_MORA];
        if(n>=capacity)return LE_CAPACITY;
        if(!sg_step_ok(TR_VOCAL,s))return LE_RANGE;
        if(!le_event(s)){
            len=1;while(at+len<NSTEP&&!le_event(&g->pat[TR_VOCAL][pat][at+len]))len++;
            flags=LY_REST|LY_WORD_END;mo=0;pitch[n]=0;
        }else{
            if(!len||at+len>NSTEP)return LE_RANGE;
            for(i=1;i<len;i++)if(le_event(&g->pat[TR_VOCAL][pat][at+i]))return LE_OVERLAP;
            if(f&VF_WORD_END)flags|=LY_WORD_END;
            if(f&VF_PHRASE_END)flags|=LY_PHRASE_END;
            if(!s->v[SV_NOTE]){flags|=LY_REST|LY_WORD_END;mo=0;pitch[n]=0;}
            else{
                pitch[n]=s->v[SV_NOTE];
                if(prev_note&&!prev_legato)flags|=LY_SEPARATE;
                if(f&VF_SUSTAIN){
                    if(!prev_note||!prev_legato||mora_vowel(last)==MV_N||mora_vowel(last)!=mora_vowel(mo))return LE_SUSTAIN;
                    flags|=LY_SUSTAIN;
                }
            }
        }
        tmp[n].ticks=(uint16_t)len;tmp[n].mora=(uint8_t)mo;tmp[n].flags=(uint8_t)flags;
        prev_note=!(flags&LY_REST);prev_legato=(f&VF_LEGATO)&&!(flags&LY_PHRASE_END);last=mo;
        at+=len;n++;
    }
    for(i=0;i<n;i++){units[i]=tmp[i];if(notes)notes[i]=pitch[i];}
    *count=n;return LE_OK;
}

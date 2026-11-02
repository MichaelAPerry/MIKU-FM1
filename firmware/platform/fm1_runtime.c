/* SPDX-License-Identifier: MIT */
#include "firmware/platform/fm1_runtime.h"
#include <math.h>
#include <string.h>
int miku_runtime_init(MikuRuntime *r,const MikuSong *s,uint32_t sr){
    if(!r||!s||s->pattern_count<MIKU_TRACKS||sr<8000)return 0;
    memset(r,0,sizeof(*r));r->sample_rate=sr;
    if(!miku_voice_init(&r->vocal,sr)||!miku_band_init(&r->band,sr))return 0;
    miku_drums_init(&r->drums);
    for(int i=0;i<MIKU_TRACKS;i++){
        r->mute[i]=s->mute[i];
        if(!miku_seq_init(&r->tracks[i],&s->patterns[i],s->bpm,sr))return 0;
    }
    r->running=1;return 1;
}
void miku_runtime_render(MikuRuntime *r,int32_t *dst,uint32_t frames){
    if(!dst)return;
    if(!r||!r->running){memset(dst,0,(size_t)frames*2*sizeof(*dst));return;}
    for(uint32_t i=0;i<frames;i++,r->frame++){
        for(int t=0;t<MIKU_TRACKS;t++){
            MikuEvent e;
            while(miku_seq_next(&r->tracks[t],r->frame,&e)){
                if(t==MIKU_VOCAL){
                    if(e.kind==MIKU_NOTE_ON){
                        miku_voice_note_on(&r->vocal,e.note.pitch,e.note.velocity,(MikuVowel)e.note.vowel);
                        r->vocal_pitch=e.note.pitch;
                    }else if(r->vocal_pitch==e.note.pitch)miku_voice_note_off(&r->vocal);
                }else if(t==MIKU_DRUMS){
                    if(e.kind==MIKU_NOTE_ON && !r->mute[t])
                        miku_drums_trigger(&r->drums,e.note.pitch,e.note.velocity,r->sample_rate);
                }else miku_band_note(&r->band,&e.note,e.kind==MIKU_NOTE_ON);
            }
        }
        float vocal=miku_voice_render(&r->vocal), band=miku_band_render(&r->band);
        float drums=miku_drums_render(&r->drums,r->sample_rate);
        /* Muting chords or bass here requires separate buses: until then, notes
           remain clocked but mute is applied on note-on by the adapter below. */
        float mix=(r->mute[MIKU_VOCAL]?0.f:vocal*.34f)+band+drums;
        if(mix>1.f)mix=1.f;if(mix< -1.f)mix=-1.f;
        /* FM-1 DMA uses signed 24-bit audio carried in int32 words. */
        int32_t pcm=(int32_t)lrintf(mix*8388607.f);
        dst[2*i]=pcm;dst[2*i+1]=pcm;
    }
}

/* SPDX-License-Identifier: MIT */
#include "firmware/sequencer/sequencer.h"
#include "firmware/audio/vocal/vocal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
    MikuPattern p={0};
    MikuNote n={0,96,69,100,MIKU_A,MIKU_VOCAL};
    assert(miku_pattern_add(&p,n));
    assert(miku_tick_to_sample(96,120,22050)==11025);
    MikuSequencer s;assert(miku_seq_init(&s,&p,120,22050));
    MikuEvent e;assert(miku_seq_next(&s,0,&e)&&e.kind==MIKU_NOTE_ON);
    assert(!miku_seq_next(&s,0,&e));
    assert(miku_seq_next(&s,11025,&e)&&e.kind==MIKU_NOTE_OFF);
    assert(!miku_seq_next(&s,11025,&e));
    MikuVoice v;assert(miku_voice_init(&v,22050));
    for(int vowel=0;vowel<5;vowel++){
        miku_voice_note_on(&v,69,110,(MikuVowel)vowel);
        float energy=0;
        for(int i=0;i<2000;i++){float x=miku_voice_render(&v);assert(isfinite(x));energy+=x*x;}
        assert(energy>0.000001f);
    }
    assert(!miku_pattern_add(&p,(MikuNote){0,0,60,100,0,0}));
    puts("core tests passed");return 0;
}

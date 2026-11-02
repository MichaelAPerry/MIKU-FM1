/* SPDX-License-Identifier: MIT */
#include "firmware/platform/fm1_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    MikuSong *song=calloc(1,sizeof(*song));MikuRuntime *r=calloc(1,sizeof(*r));
    assert(song&&r);song->version=1;song->bpm=120;song->pattern_count=4;
    assert(miku_pattern_add(&song->patterns[0],(MikuNote){0,96,69,105,MIKU_A,MIKU_VOCAL}));
    assert(miku_pattern_add(&song->patterns[1],(MikuNote){0,96,60,85,0,MIKU_CHORDS}));
    assert(miku_pattern_add(&song->patterns[2],(MikuNote){0,96,36,100,0,MIKU_BASS}));
    assert(miku_pattern_add(&song->patterns[3],(MikuNote){0,20,36,110,0,MIKU_DRUMS}));
    assert(miku_runtime_init(r,song,44100));
    int32_t out[512];uint64_t energy=0;
    for(int block=0;block<96;block++){
        miku_runtime_render(r,out,256);
        for(int i=0;i<512;i++){assert(out[i]<=8388607&&out[i]>=-8388607);
            if(out[i])energy++;
            if(i%2==0)assert(out[i]==out[i+1]);}
    }
    assert(energy>1000);assert(r->frame==96*256u);
    r->running=0;miku_runtime_render(r,out,256);
    for(int i=0;i<512;i++)assert(out[i]==0);
    free(song);free(r);puts("44.1k 256-frame runtime test passed");return 0;
}

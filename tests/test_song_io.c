/* SPDX-License-Identifier: MIT */
#include "shared/song_io.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void){
    MikuSong *s=calloc(1,sizeof(*s)),*out=calloc(1,sizeof(*out));assert(s&&out);
    s->version=1;s->bpm=133;s->pattern_count=2;s->mute[3]=1;
    assert(miku_pattern_add(&s->patterns[0],(MikuNote){3,96,60,90,MIKU_I,MIKU_VOCAL}));
    assert(miku_pattern_add(&s->patterns[1],(MikuNote){0,48,48,127,0,MIKU_BASS}));
    size_t n=miku_song_encoded_size(s),used=0;uint8_t *buf=malloc(n);assert(buf);
    assert(miku_song_encode(s,buf,n,&used)&&used==n);
    assert(miku_song_decode(out,buf,n));assert(out->bpm==133&&out->mute[3]==1);
    assert(out->patterns[1].notes[0].pitch==48);
    buf[18]^=1;assert(!miku_song_decode(out,buf,n));
    buf[18]^=1;assert(!miku_song_decode(out,buf,n-1));
    assert(!miku_song_encode(s,buf,n-1,0));
    free(buf);free(s);free(out);puts("song I/O tests passed");return 0;
}

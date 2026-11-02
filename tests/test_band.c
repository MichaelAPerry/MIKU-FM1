/* SPDX-License-Identifier: MIT */
#include "firmware/audio/band/band.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){
    MikuBand b;MikuDrums d;
    assert(miku_band_init(&b,22050));miku_drums_init(&d);
    for(int i=0;i<4;i++){
        MikuNote n={0,96,(uint8_t)(60+i),100,0,MIKU_CHORDS};
        miku_band_note(&b,&n,1);
    }
    MikuNote bass={0,96,36,120,0,MIKU_BASS};miku_band_note(&b,&bass,1);
    miku_drums_trigger(&d,36,110,22050);
    double power=0;
    for(int i=0;i<10000;i++){
        float x=miku_band_render(&b)+miku_drums_render(&d,22050);
        assert(isfinite(x));power+=x*x;
    }
    assert(power>0.01);
    miku_band_note(&b,&bass,0);
    assert(!b.bass.active);
    puts("band test passed");return 0;
}

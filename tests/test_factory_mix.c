/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
#include "firmware/song/factory.h"
#include <stdio.h>
#include <math.h>
static Song song;static Engine engine;
static double render(unsigned id,int solo,int *peak){
    double sum=0;unsigned count=0;sf_load(&song,id);
    if(solo>=0)song.trk[solo].solo=1;
    en_init(&engine,&song);engine.pan[0]=64;engine.pan[1]=44;engine.pan[2]=64;engine.pan[3]=64;en_post(&engine,EN_PLAY,0,0,0);*peak=0;
    for(unsigned at=0;at<44100u*8u;at+=256){
        int32_t out[512]={0};en_render(&engine,out,256);
        for(unsigned i=0;i<512;i++){int x=out[i],a=x<0?-x:x;if(a>*peak)*peak=a;sum+=(double)x*x;count++;}
    }
    return sqrt(sum/count);
}
int main(void){
    for(unsigned id=0;id<SF_COUNT;id++){
        int peak;double vocal=render(id,TR_VOCAL,&peak);if(vocal<500)return 3;
        for(int track=TR_CHORDS;track<NTRK;track++){
            double band=render(id,track,&peak),floor=track==TR_BASS?.18:.08;
            printf("%s track %d: RMS %.1f, voice-relative %.1f dB\n",sf_name(id),track,band,20*log10(band/vocal));
            if(band<vocal*floor)return 1; /* catches missing or practically inaudible backing */
        }
        render(id,-1,&peak);if(peak>=32767){fprintf(stderr,"%s: default mix clips at %d\n",sf_name(id),peak);return 2;}
    }return 0;
}

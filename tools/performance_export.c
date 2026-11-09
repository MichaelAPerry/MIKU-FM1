/* SPDX-License-Identifier: GPL-3.0-only */
/* Actual firmware engine -> WAV + sample-clocked visual state JSON + SysEx capture. */
#include "firmware/app/engine.h"
#include "firmware/app/performance_wire.h"
#include "firmware/ui/performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Song g;static Engine en;
static void le(FILE *f,uint32_t v,uint32_t n){while(n--){fputc((int)(v&255u),f);v>>=8;}}
int main(int argc,char **argv){
    FILE *j,*w,*m;uint32_t frames=44100u*12u,seq=0,lastlevel=0;PerfState visual;
    if(argc!=4){fprintf(stderr,"usage: performance_export STATE.json AUDIO.wav EVENTS.syx\n");return 2;}
    j=fopen(argv[1],"w");w=fopen(argv[2],"wb");m=fopen(argv[3],"wb");
    if(!j||!w||!m){perror("performance_export");return 1;}
    song_demo(&g);g.loop=0;en_init(&en,&g);for(unsigned i=0;i<NTRK;i++)en.pan[i]=64;
    perf_init(&visual);en_post(&en,EN_PLAY,2,0,0);
    fwrite("RIFF",1,4,w);le(w,36+frames*4,4);fwrite("WAVEfmt ",1,8,w);le(w,16,4);le(w,1,2);le(w,2,2);le(w,44100,4);le(w,176400,4);le(w,4,2);le(w,16,2);fwrite("data",1,4,w);le(w,frames*4,4);
    fprintf(j,"{\"format\":\"MIKU-performance-1\",\"rate\":44100,\"events\":[\n");
    for(uint32_t at=0;at<frames;){
        int32_t out[512]={0};uint32_t n=frames-at<256?frames-at:256,pk=0,row;
        en_render(&en,out,n);
        for(uint32_t i=0;i<2*n;i++){uint32_t a=(uint32_t)abs(out[i]);if(a>pk)pk=a;le(w,(uint16_t)out[i],2);}
        at+=n;row=(en.pos>>8)&255u;
        uint32_t level=pk*127u/32767u;
        PerfInput input={at*10u/441u,(at/441u)*g.bpm*256u/6000u,
            (uint8_t)(en.pos>>16&1u),(uint8_t)(row<g.nrows?g.row[row].sect:0),
            (uint8_t)en.mouth,(uint8_t)en.level,(uint8_t)(level*2u),
            (uint8_t)(level>lastlevel+16u?255:0)};
        perf_update(&visual,&input);lastlevel=level;
        PwState state={at,(uint16_t)(seq++&16383u),(uint8_t)en.mouth,(uint8_t)(en.level/2u),
            (uint8_t)en.mora,input.section,(uint8_t)row,(uint8_t)(en.pos&15u),input.playing,
            visual.sprite,visual.outfit,visual.background,visual.frame,visual.palette,(uint8_t)level,(uint8_t)(input.transient/2u)};
        uint8_t wire[PW_SIZE];if(!pw_encode(wire,sizeof wire,&state)){fprintf(stderr,"invalid telemetry\n");return 1;}
        fwrite(wire,1,sizeof wire,m);
        fprintf(j,"%s{\"sample\":%u,\"mouth\":%u,\"level\":%u,\"mora\":%u,\"lyric\":\"%s\",\"section\":%u,\"row\":%u,\"step\":%u,\"playing\":%u,\"sprite\":%u,\"audio\":%u}",at==n?"":",\n",at,state.mouth,state.vocal_level,state.mora,mora_str(state.mora),state.section,state.row,state.step,state.playing,state.sprite,state.audio_level);
    }
    fprintf(j,"\n]}\n");fclose(j);fclose(w);fclose(m);printf("performance_export: %u frames, %u real-engine state records\n",frames,seq);return 0;
}

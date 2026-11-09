/* SPDX-License-Identifier: GPL-3.0-only */
/* Review renders from actual firmware engine. Usage: gen_next_render PREFIX STYLE SEED [ENERGY COMPLEXITY] */
#include "firmware/song/gen_next.h"
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static Song song;static Engine engine;
static void w32(FILE *f,uint32_t n){for(int i=0;i<4;i++)fputc((int)(n>>(i*8))&255,f);}
int main(int argc,char **argv){
    if(argc<4){fprintf(stderr,"usage: gen_next_render PREFIX STYLE SEED [ENERGY COMPLEXITY]\n");return 1;}
    GenNextCfg c={(uint16_t)atoi(argv[3]),(uint8_t)atoi(argv[2]),9,1,0,(uint8_t)(argc>4?atoi(argv[4]):75),(uint8_t)(argc>5?atoi(argv[5]):70)};
    sg_init(&song);if(!gen_next_song(&song,&c))return 2;
    char path[1024];uint8_t data[SONG_ENC_MAX];snprintf(path,sizeof path,"%s.mks",argv[1]);
    FILE *f=fopen(path,"wb");if(!f)return 3;fwrite(data,1,song_encode(&song,data,sizeof data),f);fclose(f);
    song.loop=0;en_init(&engine,&song);en_post(&engine,EN_PLAY,0,0,0);
    snprintf(path,sizeof path,"%s.wav",argv[1]);f=fopen(path,"wb");if(!f)return 3;
    fwrite("RIFF",1,4,f);w32(f,0);fwrite("WAVEfmt ",1,8,f);w32(f,16);w32(f,0x20001);w32(f,44100);w32(f,176400);w32(f,0x100004);fwrite("data",1,4,f);w32(f,0);
    uint32_t frames=0,tail=3*44100,cap=44100*120,clips=0;int peak=0;double sum=0;
    while(frames<cap){int32_t out[512]={0};int16_t pcm[512];en_render(&engine,out,256);
        for(int i=0;i<512;i++){int x=out[i];if(abs(x)>peak)peak=abs(x);if(x>=32767||x<=-32768)clips++;sum+=(double)x*x;pcm[i]=(int16_t)x;}
        fwrite(pcm,4,256,f);frames+=256;
        if(!(engine.pos>>16&1u)&&frames>4096){if(tail<=256)break;tail-=256;}
    }
    fseek(f,4,SEEK_SET);w32(f,36+frames*4);fseek(f,40,SEEK_SET);w32(f,frames*4);fclose(f);
    printf("%s seed %u %u rows %.3fs peak %d rms %.1f fullscale %u\n",gen_next_style(c.style),c.seed,song.nrows,frames/44100.,peak,sqrt(sum/(frames*2.)),clips);
    return frames>=cap||clips?4:0;
}

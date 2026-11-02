/* SPDX-License-Identifier: MIT */
#include "firmware/sequencer/sequencer.h"
#include "firmware/audio/vocal/vocal.h"
#include "firmware/audio/band/band.h"
#include "shared/song_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
static void u16(FILE *f,uint16_t v){fputc(v&255,f);fputc((v>>8)&255,f);}
static void u32(FILE *f,uint32_t v){u16(f,(uint16_t)v);u16(f,(uint16_t)(v>>16));}
static int add(MikuPattern *p,uint32_t t,uint32_t dur,uint8_t pitch,uint8_t vel,uint8_t vowel,uint8_t track){
    return miku_pattern_add(p,(MikuNote){t,dur,pitch,vel,vowel,track});
}
static int compose(MikuSong *s){
    s->version=1;s->bpm=120;s->pattern_count=4;
    const uint8_t roots[8]={60,57,65,67,60,57,65,67};
    const uint8_t melody[16]={72,74,76,79,76,74,72,69,72,74,77,76,79,77,74,72};
    for(int bar=0;bar<8;bar++){
        uint32_t start=(uint32_t)bar*384;
        for(int beat=0;beat<4;beat++){
            int idx=bar*2+beat/2;
            if(!add(&s->patterns[0],start+beat*96,86,melody[idx],108,(uint8_t)(idx%5),MIKU_VOCAL))return 0;
            if(!add(&s->patterns[2],start+beat*96,85,(uint8_t)(roots[bar]-12+(beat==2?7:0)),112,0,MIKU_BASS))return 0;
            if(!add(&s->patterns[3],start+beat*96,24,(uint8_t)(beat==0||beat==2?36:38),110,0,MIKU_DRUMS))return 0;
            for(int half=0;half<2;half++)
                if(!add(&s->patterns[3],start+beat*96+half*48,18,42,70,0,MIKU_DRUMS))return 0;
        }
        for(int chord=0;chord<3;chord++){
            int interval=chord==0?0:chord==1?4:7;
            if(!add(&s->patterns[1],start,370,(uint8_t)(roots[bar]+interval),82,0,MIKU_CHORDS))return 0;
        }
    }
    return 1;
}
int main(int argc,char **argv){
    if(argc<2||argc>3){fprintf(stderr,"usage: %s output.wav [output.mik]\n",argv[0]);return 2;}
    MikuSong *song=calloc(1,sizeof(*song)),*loaded=calloc(1,sizeof(*loaded));
    if(!song||!loaded||!compose(song))return 3;
    size_t size=miku_song_encoded_size(song),used=0;uint8_t *bytes=malloc(size);
    if(!bytes||!miku_song_encode(song,bytes,size,&used)||!miku_song_decode(loaded,bytes,used))return 4;
    if(argc==3){
        FILE *s=fopen(argv[2],"wb");if(!s)return 5;
        if(fwrite(bytes,1,used,s)!=used||fclose(s))return 6;
    }
    free(bytes);free(song);
    MikuSequencer seq[4];MikuVoice voice;MikuBand band;MikuDrums drums;
    const uint32_t sr=22050,frames=sr*16; /* 8 bars at 120 BPM */
    if(!miku_voice_init(&voice,sr)||!miku_band_init(&band,sr))return 7;
    miku_drums_init(&drums);
    for(int i=0;i<4;i++)if(!miku_seq_init(&seq[i],&loaded->patterns[i],loaded->bpm,sr))return 8;
    FILE *f=fopen(argv[1],"wb");if(!f){perror(argv[1]);return 9;}
    fwrite("RIFF",1,4,f);u32(f,36+frames*2);fwrite("WAVEfmt ",1,8,f);
    u32(f,16);u16(f,1);u16(f,1);u32(f,sr);u32(f,sr*2);u16(f,2);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*2);
    for(uint32_t i=0;i<frames;i++){
        for(int t=0;t<4;t++){
            MikuEvent e;
            while(miku_seq_next(&seq[t],i,&e)){
                if(t==MIKU_VOCAL){
                    if(e.kind==MIKU_NOTE_ON)miku_voice_note_on(&voice,e.note.pitch,e.note.velocity,(MikuVowel)e.note.vowel);
                    else miku_voice_note_off(&voice);
                }else if(t==MIKU_DRUMS){
                    if(e.kind==MIKU_NOTE_ON)miku_drums_trigger(&drums,e.note.pitch,e.note.velocity,sr);
                }else miku_band_note(&band,&e.note,e.kind==MIKU_NOTE_ON);
            }
        }
        float x=miku_voice_render(&voice)*.34f + miku_band_render(&band)+miku_drums_render(&drums,sr);
        if(x>1.f)x=1.f;if(x< -1.f)x=-1.f;
        u16(f,(uint16_t)(int16_t)lrintf(x*32767.f));
    }
    int ok=!ferror(f);if(fclose(f))ok=0;
    free(loaded);return ok?0:10;
}

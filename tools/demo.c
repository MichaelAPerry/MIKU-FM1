/* SPDX-License-Identifier: MIT */
#include "firmware/sequencer/sequencer.h"
#include "firmware/audio/vocal/vocal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
static void u16(FILE *f,uint16_t v){fputc(v&255,f);fputc((v>>8)&255,f);}
static void u32(FILE *f,uint32_t v){u16(f,(uint16_t)v);u16(f,(uint16_t)(v>>16));}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: %s output.wav\n",argv[0]);return 2;}
    MikuPattern p={0};
    const uint8_t pitch[]={60,62,64,67,69,67,64,60};
    const uint8_t vowel[]={0,1,2,3,4,0,3,0};
    for(unsigned i=0;i<8;i++){
        MikuNote n={i*96u,88u,pitch[i],110,vowel[i],MIKU_VOCAL};
        if(!miku_pattern_add(&p,n))return 3;
    }
    MikuSequencer seq;MikuVoice voice;
    const uint32_t sr=22050,frames=sr*5;
    if(!miku_seq_init(&seq,&p,120,sr)||!miku_voice_init(&voice,sr))return 4;
    FILE *f=fopen(argv[1],"wb");if(!f){perror(argv[1]);return 5;}
    fwrite("RIFF",1,4,f);u32(f,36+frames*2);fwrite("WAVEfmt ",1,8,f);
    u32(f,16);u16(f,1);u16(f,1);u32(f,sr);u32(f,sr*2);u16(f,2);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*2);
    for(uint32_t i=0;i<frames;i++){
        MikuEvent e;
        while(miku_seq_next(&seq,i,&e)){
            if(e.kind==MIKU_NOTE_ON)miku_voice_note_on(&voice,e.note.pitch,e.note.velocity,(MikuVowel)e.note.vowel);
            else miku_voice_note_off(&voice);
        }
        float x=miku_voice_render(&voice);if(x>1.f)x=1.f;if(x< -1.f)x=-1.f;
        int16_t sample=(int16_t)lrintf(x*32767.f);u16(f,(uint16_t)sample);
    }
    int ok=!ferror(f)&&fclose(f)==0;
    return ok?0:6;
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* Compiler -> editable native pattern -> saved MKS2 -> actual sequencer/engine, not direct voice audition.
 * lyric_song_render OUT.wav OUT.csv [--baseline] */
#include "firmware/song/lyric_edit.h"
#include "firmware/app/engine.h"
#include <stdio.h>
#include <string.h>
static Song song,loaded;static Engine engine;static uint8_t encoded[SONG_ENC_SIZE];
static void ls_w16(FILE *f,uint32_t v){fputc((int)(v&255u),f);fputc((int)((v>>8)&255u),f);}
static void ls_w32(FILE *f,uint32_t v){ls_w16(f,v);ls_w16(f,v>>16);}
int main(int argc,char **argv){
    LyUnit unit[NSTEP];uint8_t note[NSTEP]={60,62,67,0,64,0,65};uint32_t count,at,peak=0,clips=0;
    FILE *wav,*csv;
    if(argc<3||argc>4||(argc==4&&strcmp(argv[3],"--baseline"))){fprintf(stderr,"usage: lyric_song_render OUT.wav OUT.csv [--baseline]\n");return 1;}
    if(!strcmp(argv[1],argv[2])){fprintf(stderr,"output paths must differ\n");return 1;}
    sg_init(&song);song.loop=0;song.bpm=120;song.vx.human=0;song.vx.scoop=0;song.trk[0].vib=0;
    if(ly_compile("mi-ku:2 _:4 |:2 a:3! / i",1,unit,NSTEP,&count,0)||ly_apply(&song,0,unit,count,note,60,12)){fprintf(stderr,"phrase build failed\n");return 1;}
    if(argc==4)for(at=0;at<NSTEP;at++)song.pat[0][0][at].v[SV_FLAG]&=15u;
    sg_row_insert(&song,0);sg_row_set(&song,0,TR_VOCAL,0);
    if(song_encode(&song,encoded,sizeof encoded)!=SONG_ENC_SIZE||song_decode(&loaded,encoded,sizeof encoded)){fprintf(stderr,"MKS2 roundtrip failed\n");return 1;}
    wav=fopen(argv[1],"wb");if(!wav){perror(argv[1]);return 1;}
    csv=fopen(argv[2],"w");if(!csv){perror(argv[2]);fclose(wav);return 1;}
    fwrite("RIFF",1,4,wav);ls_w32(wav,36+44100u*3u*4u);fwrite("WAVEfmt ",1,8,wav);ls_w32(wav,16);
    ls_w16(wav,1);ls_w16(wav,2);ls_w32(wav,44100);ls_w32(wav,44100*4);ls_w16(wav,4);ls_w16(wav,16);
    fwrite("data",1,4,wav);ls_w32(wav,44100u*3u*4u);
    fprintf(csv,"frame,playing,row,step,mora,mouth,consonant,level,gated\n");
    en_init(&engine,&loaded);for(at=0;at<NTRK;at++)engine.pan[at]=64;en_post(&engine,EN_PLAY,0,0,0);
    for(at=0;at<44100u*3u;){
        int32_t audio[512]={0};uint32_t n=44100u*3u-at;if(n>256)n=256;
        en_render(&engine,audio,n);
        for(uint32_t k=0;k<n*2u;k++){
            int32_t a=audio[k];uint32_t mag=(uint32_t)(a<0?-a:a);if(mag>peak)peak=mag;if(mag>=32767)clips++;
            ls_w16(wav,(uint16_t)a);
        }
        at+=n;
        fprintf(csv,"%u,%u,%u,%u,%u,%u,%u,%u,%u\n",at,engine.pos>>16,(engine.pos>>8)&255u,engine.pos&255u,engine.mora,engine.mouth,fv_cons(&engine.fv),engine.level,engine.fv.gated);
    }
    if(ferror(wav)||ferror(csv)){fclose(wav);fclose(csv);return 1;}
    {int a=fclose(wav),b=fclose(csv);if(a||b)return 1;}
    printf("lyric_song_render: v%u %u bytes, 3s, peak %u, full-scale samples %u, gated at end %u\n",encoded[4],SONG_ENC_SIZE,peak,clips,engine.fv.gated);
    return engine.fv.gated!=0;
}

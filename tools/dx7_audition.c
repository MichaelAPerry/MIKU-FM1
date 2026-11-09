/* SPDX-License-Identifier: GPL-3.0-only */
/* Render audition WAVs for packed DX7 voices through the device renderer (firmware/audio/dx7).
 *   dx7_audition <voices.packed> <outdir> <prefix>
 * Each voice plays the same 4-bar progression (C Am F G, 140 BPM): voices whose name has "BASS", "SUB",
 * "808", "BOUNC" or "GROWL" in it play a one-voice eighth-note bass line, the rest four-voice chords.
 * Prints peak, RMS and time-to-silence per voice. Review tool only; no device code depends on it. */
#include "firmware/audio/dx7/dx7.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SR 44100u
#define LEN (SR*8u)
static DxSynth s;static int32_t mix[2*LEN];
static void wr32(FILE *f,uint32_t v){fputc(v&255,f);fputc(v>>8&255,f);fputc(v>>16&255,f);fputc(v>>24&255,f);}
static void wav(const char *path){
    FILE *f=fopen(path,"wb");uint32_t i;if(!f){perror(path);exit(1);}
    fwrite("RIFF",1,4,f);wr32(f,36+LEN*4);fwrite("WAVEfmt ",1,8,f);wr32(f,16);wr32(f,1|2u<<16);wr32(f,SR);wr32(f,SR*4);wr32(f,4|16u<<16);
    fwrite("data",1,4,f);wr32(f,LEN*4);
    for(i=0;i<2*LEN;i++){int32_t x=mix[i];x=x>32767?32767:x<-32768?-32768:x;fputc(x&255,f);fputc((x>>8)&255,f);}
    fclose(f);
}
static int is_bass(const uint8_t *p){char n[11];memcpy(n,p+118,10);n[10]=0;return strstr(n,"BASS")||strstr(n,"SUB")||strstr(n,"808")||strstr(n,"BOUNC")||strstr(n,"GROWL");}
int main(int argc,char **argv){
    static uint8_t bank[128*64];FILE *f;size_t n;unsigned v;
    static const uint8_t chords[4][4]={{60,64,67,72},{57,60,64,69},{53,57,60,65},{55,59,62,67}},roots[4]={36,33,29,31};
    const uint32_t beat=SR*60u/140u;
    if(argc<4){fprintf(stderr,"usage: dx7_audition voices.packed outdir prefix\n");return 2;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 1;}n=fread(bank,1,sizeof bank,f);fclose(f);
    for(v=0;v<n/128u;v++){
        const uint8_t *p=bank+128u*v;int bass=is_bass(p);uint32_t at=0,ev,i;char path[512],name[11];int64_t sq=0;int32_t peak=0;uint32_t last=0;
        memcpy(name,p+118,10);name[10]=0;
        if(!dx_patch_valid(p)){printf("%u %-10s INVALID\n",v,name);continue;}
        dx_init(&s,bass?1:4);dx_patch(&s,p);dx_set_mix(&s,100,64);memset(mix,0,sizeof mix);
        /* events: bass = 8ths at 80% gate; chords = half-note stabs (gate 90%), 4 bars */
        for(ev=0;ev<(bass?32u:8u);ev++){
            uint32_t on=ev*(bass?beat/2u:beat*2u),gate=(bass?beat/2u:beat*2u)*(bass?8u:9u)/10u,bar=(ev*(bass?1u:4u))/(bass?8u:16u)%4u;
            if(on>at){dx_render(&s,mix+2u*at,on-at);at=on;}
            if(bass)dx_note_on(&s,(uint8_t)(roots[bar]+((ev&3u)==2u?12:0)),110);else for(i=0;i<4;i++)dx_note_on(&s,chords[bar][i],100);
            dx_render(&s,mix+2u*at,gate);at+=gate;
            if(bass)dx_all_off(&s);else for(i=0;i<4;i++)dx_note_off(&s,chords[bar][i]);
        }
        dx_render(&s,mix+2u*at,LEN-at);
        for(i=0;i<2u*LEN;i++){int32_t x=abs(mix[i]);if(x>peak)peak=x;sq+=(int64_t)mix[i]*mix[i];if(x>64)last=i/2u;}
        snprintf(path,sizeof path,"%s/%s-%u.wav",argv[2],argv[3],v);wav(path);
        printf("%u %-10s %-5s peak %6d rms %6.0f last-audible %.2fs\n",v,name,bass?"bass":"chord",peak,__builtin_sqrt((double)sq/(2.0*LEN)),last/(double)SR);
    }
    return 0;
}

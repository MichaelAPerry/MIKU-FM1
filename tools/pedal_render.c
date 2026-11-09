/* SPDX-License-Identifier: GPL-3.0-only */
/* Process computer-side stereo PCM16 WAV through the freestanding pedal pipeline.
 * pedal_render INPUT.wav OUTPUT.wav [DRIVE WET TONE] ; no USB transport is emulated. */
#include "firmware/audio/pedal/pedal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t get16(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8;}
static uint32_t get32(const unsigned char *p){return get16(p)|get16(p+2)<<16;}
static void put16(FILE *f,uint32_t n){fputc((int)(n&255u),f);fputc((int)((n>>8)&255u),f);}
static void put32(FILE *f,uint32_t n){put16(f,n);put16(f,n>>16);}
int main(int argc,char **argv){
    FILE *in,*out;unsigned char h[44];Pedal p;PedalSet s;uint32_t remaining,total=0,maximum=0;
    if(argc!=3&&argc!=6){fprintf(stderr,"usage: pedal_render INPUT.wav OUTPUT.wav [DRIVE WET TONE]\n");return 1;}
    if(!strcmp(argv[1],argv[2])){fprintf(stderr,"input and output must differ\n");return 1;}
    pedal_default(&s);s.drive=60;
    if(argc==6){char *end;unsigned long v[3];for(int k=0;k<3;k++){v[k]=strtoul(argv[k+3],&end,10);if(*end||v[k]>100){fprintf(stderr,"controls must be 0..100\n");return 1;}}
        s.drive=(uint8_t)v[0];s.wet=(uint8_t)v[1];s.tone=(uint8_t)v[2];}
    in=fopen(argv[1],"rb");if(!in){perror(argv[1]);return 1;}
    if(fread(h,1,44,in)!=44||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVEfmt ",8)||get32(h+16)!=16||
       get16(h+20)!=1||get16(h+22)!=2||get32(h+24)!=44100||get16(h+34)!=16||memcmp(h+36,"data",4)||get32(h+40)%4){
        fprintf(stderr,"requires canonical stereo PCM16 44100 Hz WAV\n");fclose(in);return 1;}
    remaining=get32(h+40);out=fopen(argv[2],"wb");if(!out){perror(argv[2]);fclose(in);return 1;}
    fwrite("RIFF",1,4,out);put32(out,36u+remaining);fwrite(h+8,1,36,out);pedal_init(&p);
    while(remaining){
        unsigned char bytes[1024];int32_t src[512],dst[512];uint32_t b=remaining<sizeof bytes?remaining:(uint32_t)sizeof bytes,n=b/4;
        if(fread(bytes,1,b,in)!=b){fprintf(stderr,"truncated input\n");fclose(in);fclose(out);return 1;}
        for(uint32_t k=0;k<n*2u;k++){uint32_t v=get16(bytes+2u*k);src[k]=v>=32768u?(int32_t)v-65536:(int32_t)v;}
        pedal_process(&p,&s,src,0,dst,n);if(p.peak>maximum)maximum=p.peak;
        for(uint32_t k=0;k<n*2u;k++)put16(out,(uint16_t)dst[k]);
        remaining-=b;total+=n;
    }
    fclose(in);if(ferror(out)){fclose(out);return 1;}if(fclose(out))return 1;
    printf("pedal_render: %u frames, input peak %u, input clips %u, output clips %u, state %u bytes\n",total,maximum,p.clipped_input,p.clipped_output,(unsigned)sizeof p);
    return 0;
}

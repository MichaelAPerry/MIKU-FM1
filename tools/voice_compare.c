/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-only evaluation: cc -O2 -I. tools/voice_compare.c firmware/audio/fvoice/fvoice.c -lm -o build/voice_compare
 * build/voice_compare build/evidence/voice
 * Writes PREFIX-before.wav / PREFIX-after.wav and CSV to stdout. Each entry:
 * fresh machine-exact MIKU, velocity127 bright100 breath0, 160*256 frames;
 * RMS/peak/clip/normalized waveform error measured over the final80 blocks.
 * WAV order: notes48,60,72,78,84,90,96, each AIUEO; 100ms silence between entries. */
#include "firmware/audio/fvoice/fvoice.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void u16(FILE*f,unsigned x){fputc(x&255,f);fputc((x>>8)&255,f);}
static void u32(FILE*f,unsigned x){u16(f,x);u16(f,x>>16);}
static FILE *wave(const char*p,unsigned frames){FILE*f=fopen(p,"wb");if(!f)return NULL;fwrite("RIFF",1,4,f);u32(f,36+frames*2);fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,1);u16(f,1);u32(f,44100);u32(f,88200);u16(f,2);u16(f,16);fwrite("data",1,4,f);u32(f,frames*2);return f;}
int main(int argc,char**argv){
    static const int notes[]={48,60,72,78,84,90,96};
    char p[1024];FILE*f[2];unsigned total=35u*(160u*256u+4410u);
    if(argc!=2)return 1;
    for(int k=0;k<2;k++){snprintf(p,sizeof p,"%s-%s.wav",argv[1],k?"after":"before");f[k]=wave(p,total);if(!f[k])return 1;}
    puts("note,vowel,rms_before,rms_after,db,peak_before,peak_after,clips_before,clips_after,waveform_error");
    for(unsigned ni=0;ni<sizeof notes/sizeof *notes;ni++)for(int w=0;w<5;w++){
        FvVoice v[2];double ss[2]={0},cross=0;int pk[2]={0},clips[2]={0};
        for(int k=0;k<2;k++){fv_init(&v[k]);v[k].human=v[k].scoop=v[k].breath=0;v[k].vib_depth=0;v[k].level_comp=k?100:0;fv_note_on(&v[k],notes[ni],w,127);}
        for(int b=0;b<160;b++){int32_t x[2][512]={{0}};for(int k=0;k<2;k++)fv_render(&v[k],x[k],256);
            for(int i=0;i<256;i++){for(int k=0;k<2;k++){int z=x[k][i*2];u16(f[k],(unsigned)z);if(b>=80){ss[k]+=(double)z*z;if(abs(z)>pk[k])pk[k]=abs(z);clips[k]+=abs(z)==32767;}}if(b>=80)cross+=(double)x[0][i*2]*x[1][i*2];}}
        printf("%d,%c,%.1f,%.1f,%.2f,%d,%d,%d,%d,%.8f\n",notes[ni],"AIUEO"[w],sqrt(ss[0]/20480),sqrt(ss[1]/20480),10*log10(ss[1]/ss[0]),pk[0],pk[1],clips[0],clips[1],1-cross*cross/(ss[0]*ss[1]));
        for(int i=0;i<4410;i++)for(int k=0;k<2;k++)u16(f[k],0);
    }
    return fclose(f[0])|fclose(f[1]);
}

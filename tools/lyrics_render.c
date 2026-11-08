/* SPDX-License-Identifier: GPL-3.0-only */
/* Host audition of reusable lyric units through the actual device formant engine.
 * lyrics_render OUT.wav ["mi-ku:2 _:4 |:2 a:3! / i"] */
#include "firmware/song/lyrics.h"
#include "firmware/song/mora.h"
#include "firmware/audio/fvoice/fvoice.h"
#include <stdio.h>
#include <stdint.h>
static void put16(FILE *f,uint32_t n){fputc((int)(n&255u),f);fputc((int)((n>>8)&255u),f);}
static void put32(FILE *f,uint32_t n){put16(f,n);put16(f,n>>16);}
int main(int argc,char **argv){
    LyUnit unit[128];uint32_t n,e,i,j,frames=22050;FvVoice v;FILE *f;int rc;
    if(argc<2||argc>3){fprintf(stderr,"usage: lyrics_render OUT.wav [PHRASE]\n");return 1;}
    rc=ly_compile(argc==3?argv[2]:"mi-ku:2 _:4 |:2 a:3! / i",1,unit,128,&n,&e);
    if(rc){fprintf(stderr,"ly_compile %d at byte %u\n",rc,e);return 1;}
    for(i=0;i<n;i++){
        if(unit[i].ticks>100u||frames>44100u*120u){fprintf(stderr,"audition limited to 100 ticks/unit and two minutes\n");return 1;}
        frames+=(uint32_t)unit[i].ticks*5292u;
    }
    f=fopen(argv[1],"wb");if(!f){perror(argv[1]);return 1;}
    fwrite("RIFF",1,4,f);put32(f,36u+frames*2u);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,1);put32(f,44100);put32(f,88200);put16(f,2);put16(f,16);
    fwrite("data",1,4,f);put32(f,frames*2u);
    fv_init(&v);v.human=0;v.vib_depth=0;v.scoop=0;
    puts("sample,unit,mora,flags,mouth,consonant");
    {uint32_t at=0;
    for(i=0;i<=n;i++){
        uint32_t length=i<n?(uint32_t)unit[i].ticks*5292u:22050u;
        if(i==n||(unit[i].flags&LY_REST))fv_note_off(&v);
        else{
            if(unit[i].flags&LY_SEPARATE)fv_note_off(&v);
            fv_note_on_mora(&v,(uint8_t)(60u+(i%4u)*2u),(uint8_t)ly_consonant(&unit[i]),(uint8_t)ly_vowel(&unit[i]),110);
        }
        for(j=0;j<length;){
            int32_t audio[32]={0};uint32_t k,m=length-j<16u?length-j:16u;
            fv_render(&v,audio,m);
            printf("%u,%u,%s,%u,%u,%u\n",at,i,i<n?mora_str(unit[i].mora):"END",i<n?unit[i].flags:0,fv_mouth(&v),fv_cons(&v));
            for(k=0;k<m;k++){
                int32_t a=audio[k*2u];a=a>32767?32767:a< -32768? -32768:a;put16(f,(uint16_t)a);
            }
            j+=m;at+=m;
        }
    }}
    if(ferror(f)){fclose(f);return 1;}return fclose(f)!=0;
}

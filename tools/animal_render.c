/* SPDX-License-Identifier: GPL-3.0-only */
/* Five-second ANIMAL comparison: four seconds held, one second natural release. Same firmware engine. */
#include "firmware/app/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Song g; static Engine e;
static void le(FILE *f, unsigned n, unsigned bytes) { while(bytes--) { fputc(n & 255, f); n >>= 8; } }
int main(int argc, char **argv) {
    FILE *w, *csv; unsigned frames = 44100 * 5, changes = 0, prev = 0; int32_t o[2];
    if(argc != 3) return 2;
    w=fopen(argv[1],"wb");csv=fopen(argv[2],"w"); if(!w||!csv)return 3;
    fwrite("RIFF",1,4,w); le(w,36+frames*4,4); fwrite("WAVEfmt ",1,8,w);le(w,16,4);le(w,1,2);le(w,2,2);le(w,44100,4);le(w,176400,4);le(w,4,2);le(w,16,2);fwrite("data",1,4,w);le(w,frames*4,4);
    song_demo(&g);g.vx.lang=VL_ANIMAL;en_init(&e,&g);memset((void *)e.pan,64,NTRK);en_post(&e,EN_ON,TR_VOCAL,64,7);
    fprintf(csv,"frame,ms,mora,interval_frames\n");
    for(unsigned t=0;t<frames;t++) { unsigned before=e.bt;if(t==44100*4)en_post(&e,EN_OFF,TR_VOCAL,0,0);o[0]=o[1]=0;en_render(&e,o,1);
      if(!t || (e.bt>before && e.fv.gated)){fprintf(csv,"%u,%.4f,%u,%u\n",t+1,(t+1)/44.1,e.mora,t+1-prev);prev=t+1;changes++;}
      for(unsigned ch=0;ch<2;ch++)le(w,(unsigned)o[ch],2);
    }
    fclose(w);fclose(csv);printf("%s: %u syllable onsets, gated after release %u\n",argv[1],changes,e.fv.gated);return 0;
}

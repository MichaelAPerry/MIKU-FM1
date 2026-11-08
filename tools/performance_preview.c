/* SPDX-License-Identifier: GPL-3.0-only */
/* cc -O2 -I. tools/performance_preview.c firmware/ui/performance.c -o build/performance_preview
 * build/performance_preview build/evidence/stage
 * Uses actual packed firmware sprites plus the shared stage geometry module.
 * Three 240x240 PPM files: original, grid, horizon. Character art remains PCL. */
#include "firmware/ui/performance.h"
#include "firmware/ui/singer_sprites.h"
#include <stdio.h>
static uint16_t sprite_pixel(const PerfState*s,unsigned x,unsigned y){
    const PerfOutfit *o=perf_outfit(s->outfit);int sx=((int)x-o->x),sy=((int)y-o->y);
    if(sx<0||sy<0||sx>=o->width*o->scale||sy>=o->height*o->scale)return perf_pixel(s,x,y);
    unsigned p=(unsigned)(sy/o->scale)*o->width+(unsigned)(sx/o->scale);
    unsigned b=SINGER_STAGE[o->first_frame+s->sprite][p>>1],i=p&1u?b&15u:b>>4;
    return i?SINGER_STAGE_PAL[i]:perf_pixel(s,x,y);
}
int main(int argc,char**argv){
    static const char*names[]={"original","grid","horizon"};char path[1024];
    if(argc!=2)return 1;
    for(unsigned bg=0;bg<PERF_BG_N;bg++){
        PerfState s;PerfInput in={1000,256,1,3,0,180,200,150};perf_init(&s);perf_select(&s,0,bg,bg,bg==2?1:0);perf_update(&s,&in);
        snprintf(path,sizeof path,"%s-%s.ppm",argv[1],names[bg]);FILE*f=fopen(path,"wb");if(!f)return 1;fprintf(f,"P6\n240 240\n255\n");
        for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++){
            uint16_t p=y<PERF_SCENE_H?sprite_pixel(&s,x,y):0;
            unsigned r=(p>>11)&31u,g=(p>>5)&63u,b=p&31u;
            fputc((r<<3)|(r>>2),f);fputc((g<<2)|(g>>4),f);fputc((b<<3)|(b>>2),f);
        }
        if(fclose(f))return 1;
    }
    return 0;
}

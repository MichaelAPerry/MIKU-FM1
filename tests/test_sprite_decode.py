#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the real native LCD draw functions against original pixel-exact atlases."""
import pathlib
import subprocess
import sys
import tempfile
root, cc = pathlib.Path(sys.argv[1]), sys.argv[2]
s = (pathlib.Path(sys.argv[3]) if len(sys.argv)>3 else root/'native/dinghy.c').read_text()
def function(name):
    start=s.index('static void '+name+'(')
    begin=s.index('{',start);depth=1;end=begin+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[start:end]
source=r'''
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "firmware/ui/singer_sprites.h"
#include "firmware/ui/wardrobe_sprites.h"
#include "firmware/ui/performance.h"
static PerfState performance;
static uint16_t swap16(uint16_t p){return (uint16_t)((p>>8)|(p<<8));}
#define CV_MAX 8192u
#define FX 30u
#define FY 2u
#define DX 28u
static uint16_t cv_px[CV_MAX],screen[240*240],pw[16],dpw[16],c_bg=1,c_disc=2;
static uint8_t hw[180];static int32_t cv_oy;
static void lcd_sync(void){}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *p){
    assert(x+w<=240 && y+h<=240);
    for(unsigned r=0;r<h;r++)for(unsigned c=0;c<w;c++)screen[(y+r)*240+x+c]=p[r*w+c];
}
FACE
DANCE
static unsigned idx(const uint8_t*d,unsigned w,unsigned x,unsigned y){unsigned p=y*w+x;return p&1?d[p/2]&15:d[p/2]>>4;}
int main(void){
    for(unsigned i=0;i<16;i++)pw[i]=dpw[i]=(uint16_t)(i+10);
    for(unsigned i=0;i<180;i++)hw[i]=90;
    for(unsigned f=0;f<SF_COUNT;f++){
        memset(screen,0,sizeof screen);face_draw(f,0,0,90,90);
        for(unsigned y=0;y<90;y++)for(unsigned x=0;x<90;x++){
            unsigned i=idx(SINGER_FACES[f],90,x,y);uint16_t want=i?pw[i]:c_disc;
            for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)assert(screen[(FY+2*y+dy)*240+FX+2*x+dx]==want);
        }
        /* Dirty rectangles can begin at odd x and odd y. */
        memset(screen,0,sizeof screen);face_draw(f,27,39,31,21);
        for(unsigned y=39;y<60;y++)for(unsigned x=27;x<58;x++){
            unsigned i=idx(SINGER_FACES[f],90,x,y);uint16_t want=i?pw[i]:c_disc;
            assert(screen[(FY+2*y)*240+FX+2*x]==want);
        }
        assert(screen[0]==0);
    }
    for(unsigned f=0;f<ST_COUNT;f++){
        memset(screen,0,sizeof screen);dance_draw(f);
        for(unsigned y=0;y<71;y++)for(unsigned x=0;x<61;x++){
            unsigned i=idx(SINGER_STAGE[f],61,x,y);uint16_t want=swap16(i?SC_STAGE_PAL[i]:perf_pixel(&performance,DX+3*x,3*y));
            for(unsigned dy=0;dy<3;dy++)for(unsigned dx=0;dx<3;dx++)assert(screen[(3*y+dy)*240+DX+3*x+dx]==want);
        }
    }
    clock_t start=clock();
    for(unsigned repeat=0;repeat<100;repeat++)for(unsigned f=0;f<ST_COUNT;f++)dance_draw(f);
    printf("host DANCE CPU draw mean_us=%.3f (800 full draws, no ISR or DMA timing)\n",(double)(clock()-start)*1e6/CLOCKS_PER_SEC/800.0);
    puts("native LCD face/dance: all frames, scales, odd-width rows and dirty rectangles pixel-exact");
}
'''.replace('FACE\n',function('face_draw')+'\n').replace('DANCE\n',function('dance_draw')+'\n')
with tempfile.TemporaryDirectory() as td:
    path=pathlib.Path(td);(path/'sprite.c').write_text(source)
    subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-I'+str(root),str(path/'sprite.c'),str(root/'firmware/ui/sprite_codec.c'),str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),'-o',str(path/'sprite')],check=True)
    subprocess.run([str(path/'sprite')],check=True)

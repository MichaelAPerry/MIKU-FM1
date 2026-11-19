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
#include "firmware/ui/mouth_sprites.h"
#include "firmware/ui/performance.h"
static PerfState performance;
static uint16_t swap16(uint16_t p){return (uint16_t)((p>>8)|(p<<8));}
#define CV_MAX 8192u
#define FX 30u
#define FY 2u
#define DX 28u
static uint16_t cv_px[CV_MAX],screen[240*240],pw[16],dpw[16],mpw[16],c_bg=1,c_disc=2;
static uint8_t hw[180];static int32_t cv_oy;
static const uint16_t *pending;static uint16_t held[CV_MAX];
static uint32_t px,py,pwth,ph;
static void lcd_sync(void){
    if(!pending)return;
    assert(!memcmp(pending,held,pwth*ph*sizeof *pending)); /* DMA source stayed immutable */
    for(unsigned r=0;r<ph;r++)for(unsigned c=0;c<pwth;c++)screen[(py+r)*240+px+c]=pending[r*pwth+c];
    pending=0;
}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t *p){
    assert(x+w<=240 && y+h<=240);
    lcd_sync();assert(w*h<=CV_MAX);pending=p;px=x;py=y;pwth=w;ph=h;
    memcpy(held,p,w*h*sizeof *p);
}
FACE_MOUTH_DEFS
MOUTHBOX
FACE
DANCE
static unsigned idx(const uint8_t*d,unsigned w,unsigned x,unsigned y){unsigned p=y*w+x;return p&1?d[p/2]&15:d[p/2]>>4;}
int main(void){
    for(unsigned i=0;i<16;i++)pw[i]=(uint16_t)(i+10),dpw[i]=swap16(SC_STAGE_PAL[i]);
    for(unsigned i=0;i<180;i++)hw[i]=90;
    for(unsigned f=0;f<SF_COUNT;f++){
        if(f>=SF_A&&f<=SF_N)continue; /* These slots alias NEUTRAL and are never drawn. */
        memset(screen,0,sizeof screen);face_draw(f,0,0,90,90);lcd_sync();
        for(unsigned y=0;y<90;y++)for(unsigned x=0;x<90;x++){
            unsigned i=idx(SINGER_FACES[f],90,x,y);uint16_t want=i?pw[i]:c_disc;
            for(unsigned dy=0;dy<2;dy++)for(unsigned dx=0;dx<2;dx++)assert(screen[(FY+2*y+dy)*240+FX+2*x+dx]==want);
        }
        /* Dirty rectangles can begin at odd x and odd y. */
        memset(screen,0,sizeof screen);face_draw(f,27,39,31,21);lcd_sync();
        for(unsigned y=39;y<60;y++)for(unsigned x=27;x<58;x++){
            unsigned i=idx(SINGER_FACES[f],90,x,y);uint16_t want=i?pw[i]:c_disc;
            assert(screen[(FY+2*y)*240+FX+2*x]==want);
        }
        assert(screen[0]==0);
    }
    /* Both languages: every outfit x every mouth picture uses the cleaned NEUTRAL face. */
    for(unsigned i=0;i<16;i++)mpw[i]=(uint16_t)(1000+i);
    for(unsigned outfit=0;outfit<WD_OUTFIT_N&&outfit<MOUTH_OUTFIT_COUNT;outfit++)for(unsigned v=0;v<MOUTH_COUNT;v++){
        uint8_t frow[90],mrow[MOUTH_W];unsigned mx=MOUTH_ANCHORS[2*outfit],my=MOUTH_ANCHORS[2*outfit+1];
        performance.outfit=(uint8_t)outfit;
        memset(screen,0,sizeof screen);face_draw(FACE_MOUTH|v,0,0,90,90);lcd_sync();
        for(unsigned y=0;y<90;y++){
            assert(spr_decode_row(WD_OUTFITS[outfit].faces,SF_NEUTRAL,y,frow,90));
            for(unsigned k=MOUTH_CLEAN_OFFSETS[outfit];k<MOUTH_CLEAN_OFFSETS[outfit+1];k++){
                unsigned at=MOUTH_CLEAN_PIXELS[2*k];
                if(y>=my&&at/MOUTH_W==y-my)frow[mx+at%MOUTH_W]=MOUTH_CLEAN_PIXELS[2*k+1];
            }
            if(y>=my&&y<my+MOUTH_H)assert(spr_decode_row(&MOUTH_ATLAS,v,y-my,mrow,MOUTH_W));
            for(unsigned x=0;x<90;x++){
                unsigned i=frow[x];uint16_t want=i?pw[i]:c_disc;
                if(y>=my&&y<my+MOUTH_H&&x>=mx&&x<mx+MOUTH_W&&mrow[x-mx])want=mpw[mrow[x-mx]];
                assert(screen[(FY+2*y)*240+FX+2*x]==want&&screen[(FY+2*y+1)*240+FX+2*x+1]==want);
            }
        }
    }
    performance.outfit=0;
    for(unsigned f=0;f<ST_COUNT;f++){
        memset(screen,0,sizeof screen);dance_draw(f);lcd_sync();
        for(unsigned y=0;y<71;y++)for(unsigned x=0;x<61;x++){
            unsigned i=idx(SINGER_STAGE[f],61,x,y);uint16_t want=swap16(i?SC_STAGE_PAL[i]:perf_pixel(&performance,DX+3*x,3*y));
            for(unsigned dy=0;dy<3;dy++)for(unsigned dx=0;dx<3;dx++)assert(screen[(3*y+dy)*240+DX+3*x+dx]==want);
        }
    }
    for(unsigned outfit=0;outfit<WD_OUTFIT_N;outfit++)for(unsigned f=0;f<ST_COUNT;f++){
        uint8_t row[61];perf_style(&performance,outfit);performance.sprite=f;performance.playing=1;performance.now_ms=479;
        for(unsigned i=0;i<16;i++)dpw[i]=swap16(WD_OUTFITS[outfit].stage_palette[i]);
        dance_draw(f);lcd_sync();
        for(unsigned y=0;y<214;y++){
            if(y<213)assert(spr_decode_row(WD_OUTFITS[outfit].stage,f,y/3,row,61));
            for(unsigned x=0;x<240;x++){
                unsigned i=y<213&&x>=DX&&x<DX+183?row[(x-DX)/3]:0;
                uint16_t want=i?WD_OUTFITS[outfit].stage_palette[i]:perf_pixel(&performance,x,y);
                assert(screen[y*240+x]==swap16(want));
            }
        }
    }
    clock_t start=clock();
    for(unsigned repeat=0;repeat<100;repeat++)for(unsigned f=0;f<ST_COUNT;f++){dance_draw(f);lcd_sync();}
    printf("host DANCE CPU draw mean_us=%.3f (800 full draws, no ISR or DMA timing)\n",(double)(clock()-start)*1e6/CLOCKS_PER_SEC/800.0);
    puts("native LCD face/dance: all frames, scales, odd-width rows and dirty rectangles pixel-exact");
}
'''.replace('FACE_MOUTH_DEFS\n','#define FACE_MOUTH 0x100u\n').replace('MOUTHBOX\n',function('mouth_box')+'\n').replace('FACE\n',function('face_draw')+'\n').replace('DANCE\n',function('dance_draw')+'\n')
with tempfile.TemporaryDirectory() as td:
    path=pathlib.Path(td);(path/'sprite.c').write_text(source)
    subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-I'+str(root),str(path/'sprite.c'),str(root/'firmware/ui/sprite_codec.c'),str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),'-o',str(path/'sprite')],check=True)
    subprocess.run([str(path/'sprite')],check=True)

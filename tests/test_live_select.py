#!/usr/bin/env python3
"""Compile the real native SELECT handler against the real engine, without LCD HAL."""
import pathlib
import subprocess
import sys
import tempfile

root, build, cc = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
s = (root / 'native/dinghy.c').read_text()
start = s.index('        if(page==PG_STAGE||page==PG_DANCE){', s.index('/* encoders, per page */'))
a = s.index('            if(dS){', start)
b = s.index('            if(dA)', a)
handler = s[a:b]
harness = r'''
#include "firmware/app/engine.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static Engine en;
static Song g;
static uint32_t ed_mora, dirty, playing, prev_keys, pv_off, fm1_ms=1000;
static int32_t oct;
static int32_t wr(int32_t v,int32_t n){v%=n;return v<0?v+n:v;}
static void mk(uint32_t mask){dirty|=mask;}
static void select_mora(int32_t dS){
HANDLER
}
static int32_t reference[44100], changed[44100];
static void run(int mode,int turn,int32_t *out){
    memset(&en,0,sizeof en); song_demo(&g); en_init(&en,&g);
    ed_mora=1; dirty=0; pv_off=0; playing=mode==2; prev_keys=mode==1;
    if(mode==1)assert(en_post(&en,EN_ON,TR_VOCAL,64,1));
    if(mode==2)assert(en_post(&en,EN_PLAY,2,0,0));
    for(unsigned at=0;at<22050;at+=126){
        unsigned n=22050-at<126?22050-at:126;
        if(at==2520 && turn){
            Engine before=en; uint32_t qh=en.qh;
            select_mora(1);
            assert(ed_mora==2 && (dirty&0x20));
            assert(en.qh==qh && pv_off==0);
            assert(!memcmp(&before,&en,sizeof en));
        }
        if(at==12600 && mode==1)assert(en_post(&en,EN_OFF,TR_VOCAL,0,0));
        en_render(&en,out+2*at,n);
    }
    if(mode==1)assert(!en.fv.gated);
    if(mode==0)assert(!en.fv.gated && en.qh==0);
    if(mode==2)assert(en.pos>>16);
}
int main(void){
    for(int mode=0;mode<3;mode++){
        memset(reference,0,sizeof reference);memset(changed,0,sizeof changed);
        run(mode,0,reference);run(mode,1,changed);
        assert(!memcmp(reference,changed,sizeof reference));
    }
    memset(&en,0,sizeof en);sg_init(&g);en_init(&en,&g);
    ed_mora=69;select_mora(1);assert(ed_mora==0);
    select_mora(-1);assert(ed_mora==69);
    ed_mora=1;select_mora(1);
    assert(en_post(&en,EN_ON,TR_VOCAL,64,ed_mora));
    memset(changed,0,sizeof changed);en_render(&en,changed,256);
    assert(en.mora==2 && en.fv.gated);
    assert(en_post(&en,EN_OFF,TR_VOCAL,0,0));
    memset(changed,0,sizeof changed);en_render(&en,changed,256);
    assert(!en.fv.gated);
    puts("native SELECT: silent idle/held/playing, exact waveform, next mora, release OK");
}
'''.replace('HANDLER', handler)
with tempfile.TemporaryDirectory() as td:
    source = pathlib.Path(td) / 'select.c'
    binary = pathlib.Path(td) / 'select'
    source.write_text(harness)
    subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Wno-unused-variable',
                    '-I'+str(root), str(source), str(build/'firmware/app/libmiku_engine.a'),
                    str(build/'firmware/song/libmiku_song.a'),
                    str(build/'firmware/audio/fmband/libfmband.a'), str(build/'libmiku_core.a'),
                    '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

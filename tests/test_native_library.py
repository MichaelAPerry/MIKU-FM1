#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the actual native library load/save/confirmation functions with flash mocks."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(sys.argv[1]);cc=sys.argv[2];text=(root/'native/dinghy.c').read_text()
def function(name):
 start=text.index('static ',text.index('/* ---- storage */'))
 import re
 m=re.search(r'static (?:void|uint32_t) '+name+r'\(',text[start:]);assert m,name
 start+=m.start();end=text.index('{',start)+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
source=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "firmware/song/factory.h"
#include "firmware/ui/performance.h"
#define NSLOT 10
#define EN_PANIC 0
static Song sg,prior;
static uint8_t enc[SONG_ENC_SIZE],flash[10][SONG_ENC_SIZE],valid[10];
static struct {uint32_t magic;uint8_t ver,slot,pan[4],pad[2];}pf,persist;
static struct {uint32_t act;}en;
static PerfState performance;
static uint32_t modified,pend,pend_t,pend_arg,fm1_ms,playing,toast_err;
static const char *last_toast;
static void toast(const char *s,uint32_t e){last_toast=s;toast_err=e;}
static void lk(void){}static void ulk(void){}static void platform_poll(void){fm1_ms++;}
static void stop_wait(void){playing=0;}static void after_load(void){modified=0;}
static int en_post(void *e,unsigned a,unsigned b,unsigned c,unsigned d){(void)e;(void)a;(void)b;(void)c;(void)d;return 1;}
static int settings_save(const void *p,unsigned n){assert(n==sizeof persist);memcpy(&persist,p,n);return 0;}
static int slot_save(unsigned s,const void *p,unsigned n){assert(s<10&&n==SONG_ENC_SIZE);memcpy(flash[s],p,n);valid[s]=1;return 0;}
static uint32_t sload(uint32_t s){assert(s<10);if(!valid[s])return 0;memcpy(enc,flash[s],sizeof enc);return sizeof enc;}
static void do_new(void){sg_init(&sg);modified=0;}static void do_demo(void){song_demo(&sg);modified=0;}
SLOT_NAME
DO_SAVE
DO_LOAD
GUARDED
int main(void){
 char name[9];
 for(unsigned i=0;i<10;i++){assert(slot_name(i,name)==(i<4));if(i<4){do_load(i);assert(pf.slot==i&&song_encode(&sg,enc,sizeof enc)==sizeof enc);}}
 for(unsigned i=0;i<10;i++){
  sf_load(&sg,i%4);sg.bpm=(uint8_t)(80+i);modified=1;performance.outfit=(uint8_t)(i%12);performance.intensity=80;do_save(i);
  assert(valid[i]&&!modified&&pf.slot==i&&!toast_err);assert(persist.slot==i);assert((persist.pad[0]&15u)==i%12);
  sg_init(&sg);do_load(i);assert(sg.bpm==80+i&&pf.slot==i&&!modified);
 }
 prior=sg;modified=1;fm1_ms=10;guarded(1,0);assert(!memcmp(&sg,&prior,sizeof sg)&&modified);
 fm1_ms=15;guarded(1,1);assert(!memcmp(&sg,&prior,sizeof sg)&&modified);
 fm1_ms=18;guarded(1,0);assert(!memcmp(&sg,&prior,sizeof sg)&&modified);
 fm1_ms=20;guarded(1,0);assert(pf.slot==0&&!modified&&sg.bpm==80);
 modified=1;fm1_ms=30;guarded(6,9);assert(pf.slot==0);fm1_ms=40;guarded(6,9);assert(pf.slot==9&&modified);
 modified=1;fm1_ms=50;guarded(5,2);assert(pf.slot==9);fm1_ms=60;guarded(5,2);assert(pf.slot==2&&modified&&sg.bpm==40);
 assert(valid[2]);do_load(2);assert(sg.bpm==82); /* restore never overwrites saved data */
 valid[9]=0;prior=sg;do_load(9);assert(!memcmp(&sg,&prior,sizeof sg)&&toast_err);
 puts("native library: 10 save/load round trips, factory fallback, non-destructive restore, unsaved guards, visual preference persistence");return 0;
}
'''
for key,name in [('SLOT_NAME','slot_name'),('DO_SAVE','do_save'),('DO_LOAD','do_load'),('GUARDED','guarded')]:source=source.replace(key,function(name))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'library.c').write_text(source)
 subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-I'+str(root),str(p/'library.c'),*[str(root/'firmware/song'/f) for f in ['song.c','song_io.c','factory.c','mora.c','demo.c']],'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

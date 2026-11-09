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
#include "firmware/song/dx7_edit.h"
#include "firmware/ui/performance.h"
#include "firmware/ui/scene_prefs.h"
#define NSLOT 10
#define EN_PANIC 0
#define EN_HUSH 1
static Song sg,prior;
static uint8_t enc[SONG_ENC_MAX],flash[10][SONG_ENC_MAX];static uint32_t valid[10]; /* valid[s] = stored record length, 0 = empty */
static ScenePrefs pf,persist;
static struct {uint32_t act;}en;
static PerfState performance;
static uint32_t settings_fail,modified,pend,pend_t,pend_arg,fm1_ms,playing,toast_err;
static const char *last_toast;
static void toast(const char *s,uint32_t e){last_toast=s;toast_err=e;}
static void lk(void){}static void ulk(void){}static void platform_poll(void){fm1_ms++;}
static void scene_load(uint32_t slot){assert(slot<10);perf_scene_decode(&performance,pf.scene[slot]);}
static void stop_wait(void){playing=0;}static void after_load(void){modified=0;}
static int en_post(void *e,unsigned a,unsigned b,unsigned c,unsigned d){(void)e;(void)a;(void)b;(void)c;(void)d;return 1;}
static int settings_save(const void *p,unsigned n){if(settings_fail)return -1;assert(n==sizeof persist);memcpy(&persist,p,n);return 0;}
static int slot_save(unsigned s,const void *p,unsigned n){assert(s<10&&(n==SONG_ENC_SIZE||n==SONG_ENC_MAX));memcpy(flash[s],p,n);valid[s]=n;return 0;}
static uint32_t sload(uint32_t s){assert(s<10);if(!valid[s])return 0;memcpy(enc,flash[s],valid[s]);return valid[s];}
static void do_new(void){sg_init(&sg);modified=0;}static void do_demo(void){song_demo(&sg);modified=0;}
SLOT_NAME
DO_SAVE
DO_LOAD
GUARDED
int main(void){
 char name[9];sp_defaults(&pf);perf_init(&performance);
 for(unsigned i=0;i<10;i++){assert(slot_name(i,name)==(i<4));if(i<4){do_load(i);assert(pf.slot==i&&song_encode(&sg,enc,sizeof enc)==SONG_ENC_SIZE);}}
 for(unsigned i=0;i<10;i++){
  sf_load(&sg,i%4);sg.bpm=(uint8_t)(80+i);modified=1;performance.outfit=(uint8_t)(i%12);performance.intensity=80;do_save(i);
  assert(valid[i]&&!modified&&pf.slot==i&&!toast_err);assert(persist.slot==i);assert((persist.scene[i][0]&15u)==i%12);
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
 /* v5: CHORDS on a DX7 preset saves as SONG_ENC_MAX, names and reloads; a v4 song saved over it in the same slot reloads as v4 */
 sf_load(&sg,1);db_select(&sg,1,3);sg.dx7[0][5]^=0x11;sg.trk[1].preset=SG_DX7_CUSTOM;memcpy(sg.name,"DXSONG  ",8);sg.bpm=111;modified=1;
 do_save(5);assert(valid[5]==SONG_ENC_MAX&&!modified&&!toast_err&&pf.slot==5);
 assert(slot_name(5,name)&&!strcmp(name,"DXSONG  "));
 prior=sg;sg_init(&sg);do_load(5);assert(sg.bpm==111&&sg.trk[1].preset==SG_DX7_CUSTOM&&!memcmp(sg.dx7[0],prior.dx7[0],128));
 sf_load(&sg,2);memcpy(sg.name,"CLASSIC ",8);sg.bpm=97;modified=1;do_save(5);assert(valid[5]==SONG_ENC_SIZE&&!toast_err);
 assert(slot_name(5,name)&&!strcmp(name,"CLASSIC "));sg_init(&sg);do_load(5);assert(sg.bpm==97&&!db_active(&sg,1)&&!db_active(&sg,2));
 /* the song is saved even when the look cannot be persisted: distinct message, song not marked unsaved */
 sf_load(&sg,3);sg.bpm=77;modified=1;settings_fail=1;do_save(6);settings_fail=0;
 assert(valid[6]==SONG_ENC_SIZE&&!modified&&toast_err&&!strcmp(last_toast,"SONG SAVED - LOOK NOT SAVED"));
 puts("native library: 10 save/load round trips, factory fallback, non-destructive restore, unsaved guards, visual preference persistence");return 0;
}
'''
for key,name in [('SLOT_NAME','slot_name'),('DO_SAVE','do_save'),('DO_LOAD','do_load'),('GUARDED','guarded')]:source=source.replace(key,function(name))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'library.c').write_text(source)
 subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-I'+str(root),str(p/'library.c'),*[str(root/'firmware/song'/f) for f in ['song.c','dx7_edit.c','song_io.c','factory.c','mora.c','demo.c']],str(root/'firmware/audio/dx7/dx7.c'),str(root/'firmware/audio/fmband/fmband_mx.c'),str(root/'firmware/audio/fmband/fmband_tables.c'),str(root/'firmware/ui/performance.c'),str(root/'firmware/ui/stagefx.c'),str(root/'firmware/ui/sprite_codec.c'),str(root/'firmware/ui/scene_prefs.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

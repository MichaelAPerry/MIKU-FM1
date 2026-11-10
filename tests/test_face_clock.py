#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the real STAGE update: fixed sampling cadence and short lip closures."""
import pathlib,subprocess,sys,tempfile
root=pathlib.Path(sys.argv[1]);s=(root/'native/dinghy.c').read_text()
a=s.index('static void face_update(');b=s.index('{',a)+1;depth=1
while depth:
 depth+=(s[b]=='{')-(s[b]=='}');b+=1
start=s.index('static uint32_t dance_status_ready(');end=s.index('static void dance_update(',start)
status=s[start:end]
start=end;end=s.index('/* ---- pages.',start)
dance=s[start:end]
source=r'''
#include <stdint.h>
#include <assert.h>
#include "firmware/ui/singer_sprites.h"
#include "firmware/ui/performance.h"
static uint32_t fm1_ms,face_ms,face_cur,blink_until,next_blink=100000,happy_until,count,last;
static struct{unsigned mouth,level;}en;
static uint32_t playing=1,dance_cur,dance_ms,dance_profile,draw_cost,draws;
static PerfState performance;
static uint32_t draw_start(void*p){(void)p;return fm1_ms;}
static void draw_end(void*p,uint32_t t){(void)p;(void)t;}
static void dance_draw(uint32_t f){(void)f;draws++;fm1_ms+=draw_cost;}
static unsigned rng(void){return 1;}
static void face_set(unsigned f){assert(fm1_ms%40==0);face_cur=f;count++;last=fm1_ms;}
FUNCTION
STATUS
DANCE
int main(void){
 en.level=100;
 for(fm1_ms=0;fm1_ms<=1000;fm1_ms++){en.mouth=(fm1_ms/13)%5;face_update();}
 assert(count>10&&count<=25&&face_ms==1000);
 /* A short closure between ticks must survive to the next presentation. */
 face_cur=SF_A;en.mouth=5;fm1_ms=1010;face_update();
 assert(face_cur==SF_A&&face_ms==1000);
 en.mouth=0;fm1_ms=1040;face_update();assert(face_cur==SF_N&&last==1040);
 fm1_ms=1080;face_update();assert(face_cur==SF_A&&last==1080);
 /* A late status change must leave the musical-clock frame slot free. */
 dance_ms=1000;assert(dance_status_ready(1079));assert(!dance_status_ready(1080));
 assert(!dance_status_ready(1099));assert(!dance_status_ready(1100));
 dance_ms=0xfffffff0u;assert(!dance_status_ready(0x40u));
 playing=0;assert(dance_status_ready(0x40u));
 playing=1;dance_ms=1000;dance_cur=0;fm1_ms=1100;dance_update();
 assert(dance_ms==1100&&!draws&&dance_status_ready(fm1_ms)); /* stable pose still advances clock */
 performance.effect=1;fm1_ms=1200;draw_cost=112;dance_update();
 assert(fm1_ms==1312&&draws==1&&!dance_status_ready(fm1_ms)); /* overdue frame has priority over status */
 draw_cost=70;dance_update();assert(dance_ms==1300&&draws==2&&!dance_status_ready(fm1_ms));
 fm1_ms=1400;dance_update();assert(dance_ms==1400&&draws==3&&dance_status_ready(fm1_ms));
 return 0;
}
'''.replace('FUNCTION',s[a:b]).replace('STATUS',status).replace('DANCE',dance)
with tempfile.TemporaryDirectory() as t:
 p=pathlib.Path(t);(p/'clock.c').write_text(source)
 subprocess.run([sys.argv[2],'-std=c11','-O2','-I'+str(root),str(p/'clock.c'),'-o',str(p/'clock')],check=True)
 subprocess.run([str(p/'clock')],check=True)

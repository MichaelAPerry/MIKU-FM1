#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the real STAGE update: fixed sampling cadence and short lip closures."""
import pathlib,subprocess,sys,tempfile
root=pathlib.Path(sys.argv[1]);s=(root/'native/dinghy.c').read_text()
a=s.index('static void face_update(');b=s.index('{',a)+1;depth=1
while depth:
 depth+=(s[b]=='{')-(s[b]=='}');b+=1
source=r'''
#include <stdint.h>
#include <assert.h>
#include "firmware/ui/singer_sprites.h"
static uint32_t fm1_ms,face_ms,face_cur,blink_until,next_blink=100000,happy_until,count,last;
static struct{unsigned mouth,level;}en;
static unsigned rng(void){return 1;}
static void face_set(unsigned f){assert(fm1_ms%40==0);face_cur=f;count++;last=fm1_ms;}
FUNCTION
int main(void){
 en.level=100;
 for(fm1_ms=0;fm1_ms<=1000;fm1_ms++){en.mouth=(fm1_ms/13)%5;face_update();}
 assert(count>10&&count<=25&&face_ms==1000);
 /* A short closure between ticks must survive to the next presentation. */
 face_cur=SF_A;en.mouth=5;fm1_ms=1010;face_update();
 assert(face_cur==SF_A&&face_ms==1000);
 en.mouth=0;fm1_ms=1040;face_update();assert(face_cur==SF_N&&last==1040);
 fm1_ms=1080;face_update();assert(face_cur==SF_A&&last==1080);
 return 0;
}
'''.replace('FUNCTION',s[a:b])
with tempfile.TemporaryDirectory() as t:
 p=pathlib.Path(t);(p/'clock.c').write_text(source)
 subprocess.run([sys.argv[2],'-std=c11','-O2','-I'+str(root),str(p/'clock.c'),'-o',str(p/'clock')],check=True)
 subprocess.run([str(p/'clock')],check=True)

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the real bank/library page functions and check every text rectangle.
Pinned Dinghy FONT_S advances 8px/height16; FONT_L advances16px/height32.
These are host layout checks, not emulator or hardware screenshots.
"""
import pathlib, subprocess, sys, tempfile
root=pathlib.Path(sys.argv[1]);cc=sys.argv[2]
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "firmware/song/song.h"
#include "firmware/song/dx7_edit.h"
#include "firmware/audio/dx7/bank.h"
typedef struct {int w,h;} Font;
static const Font FONT_S={8,16},FONT_L={16,32};
#define C_TEL 1
#define C_PNK 2
#define C_WHITE 3
#define LOOK_MUTED 4
#define NSLOT 10
#define SF_COUNT 4
#define UB_N 32
#define DXS_VOICE 128
static Song sg;static struct{unsigned slot,pan[4];}pf;
static unsigned trk,dx_page,dx_src,ub_cursor,dx_cursor,dx_op,modified,library_cursor,playing,fm1_ms,st_slot,pending;
static uint8_t ub[4096];static const char *pnm[4][1]={{"VOICE"},{"MARIMBA"},{"HARD ACID"},{"808"}};
static unsigned ub_ok=1,save_ok=1;static uint8_t saved[128];
static void toast(const char*s,int e){(void)s;(void)e;}
static int ub_put(unsigned n,const uint8_t*p){(void)n;memcpy(saved,p,128);return save_ok;}
static int wr(int x,int n){return (x%n+n)%n;}
static unsigned en_pre(unsigned t,unsigned p){(void)t;(void)p;return 0;}
unsigned db_active(const Song*g,unsigned t){(void)g;(void)t;return 1;}
unsigned db_value(const Song*g,unsigned t,unsigned op,unsigned page,unsigned k){(void)g;(void)t;(void)op;(void)page;(void)k;return 99;}
unsigned db_limit(unsigned p,unsigned k){(void)p;(void)k;return 99;}
static int st_pending(unsigned t){(void)t;return pending;}
static int slot_name(unsigned n,char *s){(void)n;strcpy(s,"WWWWWWWW");return 1;}
static const char *sf_title(unsigned n){(void)n;return "WWWWWWWWWWWWWWWWWWWW";}
static void cat(char*d,const char*s){strcat(d,s);}
static void catn(char*d,unsigned n){sprintf(d+strlen(d),"%u",n);}
static void ui_back(unsigned b){(void)b;}static void ui_header(void){}static void ui_badge(void){}
static void ui_panel(int x,int y,int w,int h,int s){(void)x;(void)y;(void)w;(void)h;(void)s;}
static void cv_rect(int x,int y,int w,int h,int c){(void)x;(void)y;(void)w;(void)h;(void)c;}
static struct{int x,y,w,h;char s[80];}box[100];static unsigned count,failures;
static void txt(int x,int y,const Font*f,const char*s,int c,int al){
 (void)c;int w=(int)strlen(s)*f->w;x-=al==1?w/2:al==2?w:0;if(!w)return;
 if(x<0||y<0||x+w>240||y+f->h>240){fprintf(stderr,"bounds: %s (%d,%d,%d,%d)\n",s,x,y,w,f->h);failures++;}
 for(unsigned i=0;i<count;i++)if(x<box[i].x+box[i].w&&x+w>box[i].x&&y<box[i].y+box[i].h&&y+f->h>box[i].y){fprintf(stderr,"overlap: %s / %s\n",s,box[i].s);failures++;}
 box[count].x=x;box[count].y=y;box[count].w=w;box[count].h=f->h;strcpy(box[count++].s,s);
}
#include "native/bank_ui.inc"
#include "native/library_ui.inc"
int main(void){unsigned cases=0;memset(sg.name,'W',8);memset(ub+118,'W',10);memset(sg.dx7[0]+118,'W',10);memset(sg.dx7[1]+118,'W',10);modified=1;st_slot=31;
 for(trk=1;trk<=2;trk++)for(dx_src=0;dx_src<2;dx_src++)for(dx_page=0;dx_page<DB_PAGES;dx_page++)for(dx_op=0;dx_op<6;dx_op++)for(pending=0;pending<2;pending++){
  count=0;pg_bank(0);pg_bank(1);cases++;
 }
 count=0;dx_src=1;playing=0;bank_name_begin();pg_bank(0);pg_bank(1);cases++;
 bank_name[0]='Z';save_ok=0;bank_name_save();if(!bank_naming)return 2;
 save_ok=1;bank_name_save();if(bank_naming||saved[118]!='Z'||memcmp(saved,ub,118))return 3;
 for(playing=0;playing<2;playing++)for(library_cursor=0;library_cursor<10;library_cursor++)for(pf.slot=0;pf.slot<10;pf.slot++){
 count=0;for(unsigned b=0;b<5;b++)pg_library(b);cases++;
 }
 printf("%u page states, %u text layout failures\n",cases,failures);return failures?1:0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'layout.c').write_text(code)
 subprocess.run([cc,'-std=c11','-O2','-I'+str(root),str(p/'layout.c'),'-o',str(p/'layout')],check=True)
 subprocess.run([str(p/'layout')],check=True)

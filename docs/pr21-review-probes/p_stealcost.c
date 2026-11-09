#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static DxSynth s;static int32_t out[512];
int main(void){double tn=0,ts=0;int N=4000;
 for(int k=0;k<N;k++){dx_init(&s,4);dx_patch(&s,dx7_bank[4]);for(int i=0;i<4;i++)dx_note_on(&s,60+i*3,100);
  for(int b=0;b<20;b++){memset(out,0,sizeof out);dx_render(&s,out,256);}for(int i=0;i<4;i++)dx_note_off(&s,60+i*3);
  memset(out,0,sizeof out);clock_t t=clock();dx_render(&s,out,256);tn+=clock()-t;
  t=clock();for(int i=0;i<4;i++)dx_note_on(&s,62+i*3,100);memset(out,0,sizeof out);dx_render(&s,out,256);ts+=clock()-t;}
 printf("host us per 256-frame block, 4 voices: normal %.1f, chord-change block (4 steals) %.1f\n",tn/CLOCKS_PER_SEC/N*1e6,ts/CLOCKS_PER_SEC/N*1e6);}

#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static DxSynth c,b;static int32_t out[512];
static double run(int alg,int fb){
  uint8_t p[128];memcpy(p,dx7_bank[0],128);p[110]=alg;p[111]=fb;for(int o=0;o<6;o++){p[o*17+14]=99;p[o*17+7]=99;p[o*17+6]=99;}
  dx_init(&c,4);dx_init(&b,1);dx_patch(&c,p);dx_patch(&b,p);for(int i=0;i<4;i++)dx_note_on(&c,60+i*4,100);dx_note_on(&b,36,100);
  clock_t t=clock();for(int i=0;i<20000;i++){memset(out,0,sizeof out);dx_render(&c,out,256);dx_render(&b,out,256);}return (double)(clock()-t)/CLOCKS_PER_SEC/20000*1e6;}
int main(void){double base=run(4,7);printf("host us/256-frame block, 5 voices: alg5 fb7 %.1f | alg1 %.1f | alg4 fb7 (sample-major) %.1f | alg6 fb7 %.1f | alg32 %.1f\n",base,run(0,7),run(3,7),run(5,7),run(31,7));}

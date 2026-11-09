/* Probe: chord change with 4 DX voices; tail amplitude that is hard-cut by stealing, and L4>0 termination. */
#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static DxSynth s;static int32_t out[2*44100*4];
static int32_t peak(const int32_t *x,unsigned a,unsigned b){int32_t m=0;for(unsigned i=a;i<b;i++){int32_t v=abs(x[2*i]);if(v>m)m=v;}return m;}
static int32_t jump(const int32_t *x,unsigned a,unsigned b){int32_t m=0;for(unsigned i=a+1;i<b;i++){int32_t v=abs(x[2*i]-x[2*i-2]);if(v>m)m=v;}return m;}
int main(void){
  for(int k=0;k<8;k++){
    dx_init(&s,4);dx_patch(&s,dx7_bank[k]);dx_set_mix(&s,100,64);memset(out,0,sizeof out);
    uint8_t a[4]={60,64,67,71},b[4]={62,65,69,72};
    for(int i=0;i<4;i++)dx_note_on(&s,a[i],100);
    unsigned at=0,len=44100;dx_render(&s,out,len);at+=len;               /* 1 s chord */
    for(int i=0;i<4;i++)dx_note_off(&s,a[i]);
    dx_render(&s,out+2*at,4410);at+=4410;                                 /* 100 ms release */
    int32_t tail=peak(out,at-256,at),steady=jump(out,at-4410,at);
    for(int i=0;i<4;i++)dx_note_on(&s,b[i],100);                          /* steals all 4 released voices */
    dx_render(&s,out+2*at,4410);
    int32_t j=jump(out,at-2,at+64);
    printf("%-10s tail peak at steal %6d | max step in prior release %5d | max step across steal %6d\n",dx7_names[k],tail,steady,j);
  }
  /* L4 = 99 (sustain after release, valid DX7 behaviour) */
  uint8_t p[128];dx_init(&s,1);memcpy(p,s.patch,128);
  for(int o=0;o<6;o++){uint8_t*q=p+17*o;q[0]=q[1]=q[2]=99;q[3]=50;q[4]=q[5]=q[6]=99;q[7]=99;q[14]=o==5?99:0;q[15]=2;}
  p[110]=31;dx_patch(&s,p);memset(out,0,sizeof out);dx_note_on(&s,60,100);dx_render(&s,out,22050);dx_note_off(&s,60);
  dx_render(&s,out+2*22050,22050);
  unsigned end=22050;for(unsigned i=22050;i<44100;i++)if(out[2*i])end=i;
  printf("L4=99 voice: peak before off %d, last nonzero sample %.1f ms after note-off, peak in last 5 ms before end %d, active=%u\n",peak(out,0,22050),(end-22050)/44.1,peak(out,end-220,end),dx_active(&s));
  return 0;
}

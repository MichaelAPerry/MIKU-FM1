#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
#include <string.h>
static DxSynth s;static int32_t out[512];
int main(void){
  for(int k=0;k<8;k++){dx_init(&s,4);dx_patch(&s,dx7_bank[k]);dx_note_on(&s,60,100);for(int i=0;i<172;i++){memset(out,0,sizeof out);dx_render(&s,out,256);}dx_all_off(&s);
    double t=0;while(dx_active(&s)&&t<900){memset(out,0,sizeof out);dx_render(&s,out,256);t+=256/44100.0;}printf("%-10s release-to-inactive after PANIC: %.2f s\n",dx7_names[k],t);}
  int rates[]={0,10,20,30,40};
  for(int j=0;j<5;j++){uint8_t p[128];dx_init(&s,1);memcpy(p,s.patch,128);for(int o=0;o<6;o++){uint8_t*q=p+17*o;q[3]=rates[j];q[7]=0;q[14]=o==5?99:0;}p[110]=31;dx_patch(&s,p);
    dx_note_on(&s,60,100);for(int i=0;i<20;i++){memset(out,0,sizeof out);dx_render(&s,out,256);}dx_all_off(&s);
    double t=0;while(dx_active(&s)&&t<900){memset(out,0,sizeof out);dx_render(&s,out,256);t+=256/44100.0;}printf("carrier R4=%d: release-to-inactive %.1f s\n",rates[j],t);}
}

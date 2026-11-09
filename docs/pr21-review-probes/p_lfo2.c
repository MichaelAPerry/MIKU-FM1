#include "firmware/audio/dx7/dx7.c"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
int main(void){
  for(int k=0;k<8;k++)printf("bank %d %s: lfo speed %u delay %u pmd %u amd %u wave %u pms %u\n",k,dx7_names[k],dx7_bank[k][112],dx7_bank[k][113],dx7_bank[k][114],dx7_bank[k][115],(dx7_bank[k][116]>>1)&7,dx7_bank[k][116]>>4);
  uint8_t p[128]={0};p[112]=35;p[113]=85;DxLfo l;dx_lfo_init(&l,p);int prev=-1;
  for(long b=0;b<14000;b++){int32_t d=dx_lfo_delay(&l);int st=d==0?0:d>=(1<<24)-(1<<17)?2:1;if(st!=prev){printf("t=%.2fs state=%s\n",b*64/44100.0,st==0?"off":st==1?"ramping":"full");prev=st;}}
}

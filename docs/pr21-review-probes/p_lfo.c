/* Probe: LFO delay accumulator wrap when delayinc2 > delayinc (long LFO delays). */
#include "firmware/audio/dx7/dx7.c"
#include <stdio.h>
int main(void){
  for(int dly=60;dly<=99;dly++){
    uint8_t p[128]={0};p[112]=35;p[113]=(uint8_t)dly;p[116]=0;
    DxLfo l;dx_lfo_init(&l,p);
    int reached=0;long first_full=-1,drop=-1;
    for(long b=0;b<200000;b++){ /* 200000 blocks ~ 290 s */
      int32_t d=dx_lfo_delay(&l);
      if(d>=(1<<24)-(1<<16)&&!reached){reached=1;first_full=b;}
      if(reached&&d<(1<<20)){drop=b;break;}
    }
    if(drop>=0)printf("delay=%d inc=%u inc2=%u full_at=%.2fs DROPS_TO_%s at %.2fs\n",dly,l.delayinc,l.delayinc2,first_full*64/44100.0,"ZERO",drop*64/44100.0);
  }
  return 0;
}

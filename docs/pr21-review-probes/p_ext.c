/* Probe: extreme legal patches under UBSan; peak output per DX engine and through en_render. */
#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static DxSynth s;static int32_t buf[2*256];
static uint32_t rng=12345;static uint32_t r(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void randpatch(uint8_t *p){
  for(int o=0;o<6;o++){uint8_t*q=p+17*o;for(int i=0;i<11;i++)q[i]=r()%100;q[11]=r()%16;q[12]=(uint8_t)((r()%15)<<3|(r()%8));q[13]=(uint8_t)((r()%8)<<2|(r()%4));q[14]=r()%100;q[15]=(uint8_t)((r()%32)<<1|(r()%2));q[16]=r()%100;}
  for(int i=102;i<110;i++)p[i]=r()%100;p[110]=r()%32;p[111]=r()%16;for(int i=112;i<116;i++)p[i]=r()%100;p[116]=(uint8_t)((r()%8)<<4|(r()%6)<<1|(r()%2));p[117]=r()%49;for(int i=118;i<128;i++)p[i]='A';
}
int main(void){
  uint8_t p[128];long long worst=0;int worst_alg=-1;
  /* 1: maximal loud carriers: alg32, all OL99, KVS7, positive level scaling depth 99 both sides */
  for(int alg=0;alg<32;alg++){
    dx_init(&s,4);memcpy(p,s.patch,128);
    for(int o=0;o<6;o++){uint8_t*q=p+17*o;q[0]=q[1]=q[2]=q[3]=99;q[4]=q[5]=q[6]=q[7]=99;q[8]=0;q[9]=q[10]=99;q[11]=3|(3<<2);q[12]=7<<3|7;q[13]=7<<2|3;q[14]=99;q[15]=2;q[16]=0;}
    p[110]=alg;p[111]=7;p[114]=99;p[115]=99;p[116]=7<<4|(3<<1);
    if(!dx_patch_valid(p)){puts("invalid");return 1;}
    dx_patch(&s,p);dx_set_mix(&s,127,0);
    dx_note_on(&s,127,127);dx_note_on(&s,120,127);dx_note_on(&s,110,127);dx_note_on(&s,100,127);
    for(int b=0;b<400;b++){memset(buf,0,sizeof buf);dx_render(&s,buf,256);for(int i=0;i<512;i++){long long v=llabs(buf[i]);if(v>worst){worst=v;worst_alg=alg;}}}
  }
  printf("max |dx out| (4 voices, alg sweep, level127 hard-left): %lld at alg %d (engine clamps at 131071)\n",worst,worst_alg+1);
  /* 2: random valid patches, random notes/velocity, all chunk sizes */
  long long rw=0;
  for(int t=0;t<3000;t++){randpatch(p);if(!dx_patch_valid(p)){puts("gen invalid");return 1;}
    dx_init(&s,4);dx_patch(&s,p);dx_set_mix(&s,127,r()%128);
    for(int n=0;n<6;n++)dx_note_on(&s,r()%128,1+r()%127);
    for(int b=0;b<40;b++){unsigned n=1+r()%256;memset(buf,0,sizeof buf);dx_render(&s,buf,n);for(unsigned i=0;i<2*n;i++){long long v=llabs(buf[i]);if(v>rw)rw=v;}if(b==20)dx_all_off(&s);}
  }
  printf("random valid patches: max |out| %lld\n",rw);
  return 0;
}

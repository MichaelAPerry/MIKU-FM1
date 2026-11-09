/* SPDX-License-Identifier: Apache-2.0 */
#include "firmware/audio/dx7/dx7.h"
#include "firmware/audio/dx7/bank.h"
#include "firmware/audio/dx7/reference_vectors.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static DxSynth s;
static uint8_t patch[128];
static int32_t a[32768],b[32768];
static void base_patch(void){
 dx_init(&s,4);memcpy(patch,s.patch,sizeof patch);
 /* Algorithm32: one ratio-1 carrier, no velocity scaling, neutral pitch envelope. */
 patch[111]=0;patch[114]=patch[115]=0;
 for(unsigned o=0;o<6;o++){uint8_t *p=patch+17*o;p[0]=p[1]=p[2]=p[3]=99;p[4]=p[5]=p[6]=99;p[7]=0;p[12]=56;p[14]=o==5?99:0;p[15]=2;}
}
static void capture(int32_t *out,unsigned chunk){
 dx_init(&s,4);assert(dx_patch(&s,patch));dx_set_mix(&s,100,64);dx_note_on(&s,60,100);memset(out,0,sizeof a);
 for(unsigned i=0;i<16384;){unsigned n=16384-i;if(n>chunk)n=chunk;dx_render(&s,out+2*i,n);i+=n;}
}
int main(void){
 base_patch();assert(dx_patch_valid(patch));
 for(unsigned alg=0;alg<32;alg++){
  patch[110]=(uint8_t)alg;capture(a,256);capture(b,17);assert(!memcmp(a,b,sizeof a));
  long long power=0;for(unsigned i=0;i<32768;i++){assert(a[i]>=-32768&&a[i]<=32767);power+=(long long)a[i]*a[i];}assert(power>1000000);
  dx_note_off(&s,60);for(unsigned i=0;i<300;i++){int32_t out[512]={0};dx_render(&s,out,256);}assert(!dx_active(&s));
 }
 /* Actual factory voices: independent original-MSFA raw reference (tools/gen_dx7_reference.py), LFO depth disabled. */
 for(unsigned preset=0;preset<DX7_BANK_N;preset++){
  memcpy(patch,dx7_bank[preset],128);assert(dx_patch_valid(patch));patch[114]=patch[115]=0;
  dx_init(&s,1);assert(dx_patch(&s,patch));dx_note_on(&s,60,100);
  for(unsigned block=0;block<64;block++){int32_t out[128]={0};dx_render(&s,out,64);
   if(!(block&1u))assert(abs(s.voice[0].audio[0]-dx_reference[preset][block/2u])<=128);
  }
  /* Full patch, including LFO, must render and release without invalid state. */
  dx_init(&s,4);assert(dx_patch(&s,dx7_bank[preset]));dx_note_on(&s,60,100);dx_note_on(&s,64,100);dx_note_on(&s,67,100);
  for(unsigned i=0;i<200;i++){int32_t out[512]={0};dx_render(&s,out,256);for(unsigned j=0;j<512;j++)assert(out[j]>-262144&&out[j]<262144);}
  dx_all_off(&s);for(unsigned i=0;i<44100u*30u;i+=256){int32_t out[512]={0};dx_render(&s,out,256);}assert(!dx_active(&s));
 }
 /* Feedback in algorithms4/6 must include their multi-operator loop. */
 for(unsigned alg=3;alg<=5;alg+=2){
  memcpy(patch,dx7_bank[0],128);patch[110]=(uint8_t)alg;patch[111]=0;patch[114]=patch[115]=0;
  for(unsigned op=0;op<6;op++)patch[op*17+14]=90;
  capture(a,256);patch[111]=7;capture(b,256);assert(memcmp(a,b,sizeof a));
  capture(a,17);assert(!memcmp(a,b,sizeof a));
 }
 /* A4 carrier frequency: count positive zero crossings after onset. */
 base_patch();dx_init(&s,1);assert(dx_patch(&s,patch));dx_note_on(&s,69,127);memset(a,0,sizeof a);dx_render(&s,a,16384);
 unsigned crossings=0;for(unsigned i=4097;i<16384;i++)if(a[2*(i-1)]<=0&&a[2*i]>0)crossings++;
 assert(crossings>=121&&crossings<=123); /* 122.58 periods at 440 Hz */
 /* Invalid patch is transactional. */
 uint8_t saved[128];memcpy(saved,s.patch,128);patch[14]=100;assert(!dx_patch(&s,patch)&&!memcmp(saved,s.patch,128));
 patch[14]=0;patch[110]=32;assert(!dx_patch_valid(patch));patch[110]=0;patch[15]=64;assert(!dx_patch_valid(patch));
 /* Patch edit while held keeps all per-voice oscillator/envelope state untouched. */
 base_patch();dx_init(&s,4);assert(dx_patch(&s,patch));dx_note_on(&s,60,100);DxVoice held=s.voice[0];patch[110]=5;patch[5*17+15]=4;assert(dx_patch(&s,patch));assert(!memcmp(&held,&s.voice[0],sizeof held));
 dx_note_on(&s,64,100);assert(s.voice[0].algorithm==31&&s.voice[1].algorithm==5);
 for(unsigned i=0;i<8;i++)dx_note_on(&s,(uint8_t)(65+i),100);
 assert(dx_active(&s)==4);
 dx_all_off(&s);for(unsigned i=0;i<300;i++){int32_t out[512]={0};dx_render(&s,out,256);}assert(!dx_active(&s));
 base_patch();patch[111]=8;dx_init(&s,4);dx_patch(&s,patch);{int32_t out[512]={0};dx_render(&s,out,256);}dx_note_on(&s,60,100);assert(!s.voice[0].phase[5]);
 patch[111]=0;dx_patch(&s,patch);dx_note_on(&s,64,100);assert(s.voice[1].phase[5]);
 printf("DX7: 32 algorithms, arbitrary block sizes, 440Hz pitch, patch validation/snapshots, voice stealing and release PASS; state=%zu bytes\n",sizeof(DxSynth));
 return 0;
}

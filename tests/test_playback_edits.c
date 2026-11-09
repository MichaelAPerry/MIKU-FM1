/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
#include "firmware/song/dx7_edit.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>
static Engine e;static Song g;static int32_t out[512];
static void block(void){memset(out,0,sizeof out);en_render(&e,out,256);}
static void post(unsigned k,unsigned t,unsigned n){assert(en_post(&e,k,t,n,0));block();}
int main(void){
 sg_init(&g);db_select(&g,1,0);en_init(&e,&g);block();assert(e.dx_loaded[0]);
 /* Lock the renderer, mutate, publish, then release: edits must be noticed;
    unrelated structural edits must not reload/retrigger the current patch. */
 e.lock=1;block();assert(e.ack);g.dx7[0][14]^=1;en_refresh_patches(&e);assert(!e.dx_loaded[0]);e.lock=0;block();assert(e.dx_loaded[0]);
 uint32_t crc=e.dx_crc[0];en_refresh_patches(&e);assert(e.dx_loaded[0]&&e.dx_crc[0]==crc);
 for(unsigned t=0;t<3;t++){
  post(EN_PANIC,0,0);g.trk[t].preset=0;block();post(EN_SUSTAIN,t,1);post(EN_ON,t,60);post(EN_OFF,t,t==1?60:0);
  assert(e.deferred[t][t==1?7:0]);if(t==0)assert(e.fv.gated);
  post(EN_SUSTAIN,t,0);assert(!e.deferred[t][t==1?7:0]);if(t==0)assert(!e.fv.gated);
  post(EN_SUSTAIN,t,1);post(EN_ON,t,60);post(EN_OFF,t,t==1?60:0);post(EN_ON,t,60);
  assert(!e.deferred[t][t==1?7:0]);post(EN_PANIC,0,0);assert(!e.pedal[t]);
 }
 /* Sequenced and delayed vocal releases obey the same pedal as live notes. */
 for(unsigned test=0;test<4;test++){
  unsigned t=test%3;
  memset(&e,0,sizeof e);sg_init(&g);g.nrows=1;g.loop=0;g.bpm=120;g.vx.obeat=test==3?100:0;
  memset(g.row[0].pat,ROW_OFF,NTRK);g.row[0].pat[t]=0;
  Step *step=&g.pat[t][0][0];step->v[0]=60;step->v[1]=1;step->v[2]=t==TR_CHORDS?1:0;step->v[3]=15;
  if(t==TR_CHORDS)step->v[1]=CQ_MAJ;
  en_init(&e,&g);post(EN_SUSTAIN,t,1);post(EN_PLAY,0,0);
  for(unsigned b=0;b<48;b++)block();
  assert(e.deferred[t][t==TR_CHORDS?7:0]);
  if(t==TR_VOCAL)assert(e.fv.gated);
  post(EN_SUSTAIN,t,0);assert(!e.deferred[t][t==TR_CHORDS?7:0]);
  if(t==TR_VOCAL)assert(!e.fv.gated);
  post(EN_PANIC,0,0);
 }
 for(unsigned lang=0;lang<VL_N;lang++){
  memset(&e,0,sizeof e);sg_init(&g);g.vx.lang=(uint8_t)lang;en_init(&e,&g);
  post(EN_SUSTAIN,TR_VOCAL,1);post(EN_ON,TR_VOCAL,60);post(EN_OFF,TR_VOCAL,0);
  for(unsigned b=0;b<80;b++)block();
  assert(e.fv.gated&&e.deferred[TR_VOCAL][0]);
  post(EN_SUSTAIN,TR_VOCAL,0);assert(!e.fv.gated&&!e.deferred[TR_VOCAL][0]);
 }
 return 0;
}

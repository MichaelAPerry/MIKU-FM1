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
 return 0;
}

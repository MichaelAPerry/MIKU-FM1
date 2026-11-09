/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
#include "firmware/song/dx7_edit.h"
#include "firmware/song/gen.h"
#include "firmware/song/factory.h"
#include "firmware/song/gen_next.h"
#include "firmware/audio/dx7/bank.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <stdio.h>
static Song g,h;static Engine e;static uint8_t bytes[SONG_ENC_MAX];
static int32_t a[32768],b[32768];
static void init(void){memset(&e,0,sizeof e);en_init(&e,&g);for(unsigned i=0;i<NTRK;i++)e.pan[i]=64;}
static void capture(int32_t *out){init();memset(out,0,sizeof a);en_post(&e,EN_ON,TR_CHORDS,60,0);for(unsigned i=0;i<16384;i+=256)en_render(&e,out+2*i,256);}
static void crc(void){uint32_t c=sg_crc32(bytes,SONG_ENC_MAX-4);for(unsigned i=0;i<4;i++)bytes[SONG_ENC_MAX-4+i]=(uint8_t)(c>>(8*i));}
int main(void){
 sg_init(&g);assert(song_encode(&g,bytes,sizeof bytes)==SONG_ENC_SIZE&&bytes[4]==4);
 for(unsigned preset=0;preset<8;preset++){
  assert(db_select(&g,1,preset)&&dx_patch_valid(g.dx7[0]));capture(a);long long energy=0;for(unsigned i=0;i<32768;i++){energy+=(long long)a[i]*a[i];assert(a[i]>=-32768&&a[i]<=32767);}assert(energy>1000000);
  assert(e.pre[1]==64+preset&&dx_active(&e.dx[0])==1&&!ch_active(&e.ch));
 }
 db_select(&g,1,0);db_select(&g,2,3);capture(a);
 assert(song_encode(&g,bytes,sizeof bytes)==SONG_ENC_MAX&&bytes[4]==5);
 assert(song_decode(&h,bytes,sizeof bytes)==SIO_OK&&!memcmp(&g,&h,sizeof g));
 Song prior=h;bytes[4504+14]=100;crc();assert(song_decode(&h,bytes,sizeof bytes)==SIO_FIELD&&!memcmp(&h,&prior,sizeof h));
 assert(!db_select(&g,0,0)&&!db_select(&g,1,8));
 for(unsigned page=1;page<DB_PAGES;page++)for(unsigned op=0;op<6;op++)for(unsigned k=0;k<4;k++){
  if(page==9&&k==1)continue;
  db_select(&g,1,0);unsigned limit=db_limit(page,k);
  assert(db_set(&g,1,op,page,k,limit));assert(db_value(&g,1,op,page,k)==limit);assert(dx_patch_valid(g.dx7[0]));assert(!db_set(&g,1,op,page,k,limit+1));
 }
 db_select(&g,1,0);assert(db_toggle(&g,1,0,2)&&db_toggle(&g,1,0,6)&&dx_patch_valid(g.dx7[0]));
 /* Actual OP1 output edit affects the next note; existing held state is unchanged. */
 db_select(&g,1,0);capture(a);unsigned age=e.dx[0].age;DxVoice held=e.dx[0].voice[0];
 db_set(&g,1,0,2,0,0);{int32_t out[512]={0};en_render(&e,out,256);}assert(e.dx[0].age==age&&e.dx[0].voice[0].base[0]==held.base[0]);
 capture(b);assert(memcmp(a,b,sizeof a));
 en_post(&e,EN_OFF,1,60,0);for(unsigned i=0;i<44100u*30u;i+=256){int32_t out[512]={0};en_render(&e,out,256);}assert(!dx_active(&e.dx[0]));
 assert(song_encode(&g,bytes,sizeof bytes)==SONG_ENC_MAX&&song_decode(&h,bytes,sizeof bytes)==SIO_OK);
 sf_load(&g,0);uint8_t keep[128];db_select(&g,1,4);memcpy(keep,g.dx7[0],128);
 GenNextCfg modern={0};modern.style=0;modern.seed=5;modern.lock=2;modern.energy=70;modern.complexity=50;GenNextWorkspace work;
 assert(gen_next_song_ws(&g,&modern,&work));assert(g.trk[1].preset==68&&!memcmp(keep,g.dx7[0],128));
 GenCfg classic={0};classic.lock=2;classic.seed=5;gen_song(&g,&classic);assert(g.trk[1].preset==68&&!memcmp(keep,g.dx7[0],128));
 puts("DX7 bank: actual engine routing, all eight presets, parameter fields, portable v5 snapshots, transactional validation, held pitch and release PASS");return 0;
}

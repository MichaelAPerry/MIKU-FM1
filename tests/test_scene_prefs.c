/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/scene_prefs.h"
#include "firmware/ui/stagefx.h"
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint32_t hash_background(PerfState *s){uint32_t h=2166136261u;s->effect=0;for(unsigned y=0;y<214;y++)for(unsigned x=0;x<240;x++)h=(h^perf_pixel(s,x,y))*16777619u;return h;}
int main(void){
    ScenePrefs p,old;PerfState s,prior;uint32_t hashes[PERF_OUTFIT_N];uint8_t backgrounds[4];
    assert(sizeof p==44);sp_defaults(&p);assert(sp_valid(&p));perf_init(&s);
    for(unsigned i=0;i<4;i++){assert(perf_scene_decode(&s,p.scene[i]));backgrounds[i]=s.background;for(unsigned j=0;j<i;j++)assert(backgrounds[i]!=backgrounds[j]);}
    for(unsigned i=0;i<PERF_OUTFIT_N;i++){assert(perf_style(&s,i));hashes[i]=hash_background(&s);for(unsigned j=0;j<i;j++)assert(hashes[i]!=hashes[j]);}
    old=p;perf_style(&s,11);s.background=PERF_BG_LED;s.effect=STFX_CONFETTI;s.intensity=60;s.palette=1;assert(perf_scene_encode(&s,p.scene[7]));
    for(unsigned i=0;i<SP_SLOTS;i++)if(i!=7)assert(!memcmp(p.scene[i],old.scene[i],3));
    ScenePrefs reboot=p;assert(sp_valid(&reboot));perf_init(&s);assert(perf_scene_decode(&s,reboot.scene[7]));assert(s.outfit==11&&s.background==PERF_BG_LED&&s.effect==STFX_CONFETTI&&s.intensity==60&&s.palette==1);
    prior=s;uint8_t bad[3]={15,0,0};assert(!perf_scene_decode(&s,bad)&&!memcmp(&s,&prior,sizeof s));
    uint8_t legacy[12]={0x32,0x56,0x4b,0x4d,2,6,64,44,64,64,0xcb,0x26};
    assert(sp_migrate(&p,legacy));assert(sp_valid(&p)&&p.slot==6&&p.pan[1]==44);assert(perf_scene_decode(&s,p.scene[6]));assert(s.outfit==11&&s.background==2&&s.frame==1&&s.effect==2);
    assert(perf_scene_decode(&s,p.scene[0])&&s.outfit==1); /* migration preserves other demos' distinct scenes */
    legacy[4]=1;assert(sp_migrate(&p,legacy)&&sp_valid(&p));
    p.ver=9;assert(!sp_valid(&p));puts("scene preferences: 12 distinct backgrounds, per-slot persistence, legacy migration, transactional bounds");return 0;
}

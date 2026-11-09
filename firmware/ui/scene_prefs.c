/* SPDX-License-Identifier: GPL-3.0-only */
#include "scene_prefs.h"
void sp_reset_scene(ScenePrefs *p,uint32_t slot){
    static const uint8_t factory_styles[4]={1,5,4,11};PerfState s;
    if(!p||slot>=SP_SLOTS)return;
    perf_init(&s);perf_style(&s,slot<4u?factory_styles[slot]:0);perf_scene_encode(&s,p->scene[slot]);
}
void sp_defaults(ScenePrefs *p){
    uint32_t i;uint8_t *b=(uint8_t*)p;if(!p)return;
    for(i=0;i<sizeof *p;i++)b[i]=0;
    p->magic=SP_MAGIC;p->ver=3;p->pan[0]=64;p->pan[1]=44;p->pan[2]=p->pan[3]=64;
    for(i=0;i<SP_SLOTS;i++)sp_reset_scene(p,i);
}
uint32_t sp_valid(const ScenePrefs *p){
    PerfState s;uint32_t i;if(!p||p->magic!=SP_MAGIC||p->ver!=3||p->slot>=SP_SLOTS)return 0;
    for(i=0;i<4;i++)if(p->pan[i]>127)return 0;
    perf_init(&s);for(i=0;i<SP_SLOTS;i++)if(!perf_scene_decode(&s,p->scene[i]))return 0;return 1;
}
uint32_t sp_migrate(ScenePrefs *p,const uint8_t old[12]){
    uint32_t magic,i;PerfState s;if(!p||!old)return 0;
    magic=(uint32_t)old[0]|(uint32_t)old[1]<<8|(uint32_t)old[2]<<16|(uint32_t)old[3]<<24;
    if(magic!=SP_MAGIC||(old[4]!=1u&&old[4]!=2u)||old[5]>=SP_SLOTS)return 0;
    sp_defaults(p);p->slot=old[5];for(i=0;i<4;i++)p->pan[i]=old[6+i]<=127?old[6+i]:64;
    if(old[4]==2u){
        perf_init(&s);
        if(perf_select(&s,old[10]&15u,old[11]&3u,(old[11]>>2)&3u,0)&&(old[11]>>4)<6u){s.effect=old[11]>>4;s.intensity=(uint8_t)((old[10]>>4)*100u/15u);perf_scene_encode(&s,p->scene[p->slot]);}
    }
    return 1;
}

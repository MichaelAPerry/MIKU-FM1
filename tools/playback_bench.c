/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "firmware/app/engine.h"
#include "firmware/song/gen_next.h"
static Engine e; static Song g; static GenNextWorkspace ws; static int32_t out[512];
static double ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
int main(void){GenNextCfg c={0};c.seed=8;c.energy=80;c.complexity=80;if(!gen_next_song_ws(&g,&c,&ws))return 1;en_init(&e,&g);for(unsigned t=0;t<NTRK;t++)e.pan[t]=64;en_post(&e,EN_PLAY,0,0,0);
 double total=0,max=0;unsigned over=0;uint32_t hash=2166136261u;unsigned blocks=44100*60/256;
 for(unsigned b=0;b<blocks;b++){memset(out,0,sizeof out);double start=ns();en_render(&e,out,256);double dt=ns()-start;total+=dt;if(dt>max)max=dt;if(dt>256e9/44100)over++;for(unsigned i=0;i<512;i++)hash=(hash^(uint32_t)out[i])*16777619u;}
 printf("blocks=%u mean_us=%.3f worst_us=%.3f host_deadline_misses=%u pcm_hash=%08x\n",blocks,total/blocks/1000,max/1000,over,hash);return 0;}

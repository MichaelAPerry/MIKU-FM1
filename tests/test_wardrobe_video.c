/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/usb/jpeg/mjpeg.h"
#include "firmware/usb/jpeg/performance_frame.h"
#include "firmware/ui/stagefx.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
static MjEncoder encoder;static uint8_t bytes[MJ_FRAME_CAP];
int main(void){
    PerfState s;unsigned largest=0,frames=0;perf_init(&s);
    for(unsigned outfit=0;outfit<PERF_OUTFIT_N;outfit++)for(unsigned bg=0;bg<PERF_BG_N;bg++)for(unsigned fx=0;fx<STFX_N;fx++)for(unsigned pose=0;pose<8;pose+=6){
        assert(perf_select(&s,outfit,bg,1,0));s.sprite=(uint8_t)pose;s.effect=(uint8_t)fx;s.intensity=100;s.now_ms=480;
        assert(mj_begin(&encoder,160,120,65,bytes,sizeof bytes,mj_perf_pixel,&s));assert(mj_step(&encoder,80)==1);
        unsigned n=mj_size(&encoder);assert(n>600&&n<=sizeof bytes);if(n>largest)largest=n;frames++;
    }
    printf("wardrobe video: %u outfit/effect/pose combinations, maximum JPEG %u/%u bytes\n",frames,largest,(unsigned)sizeof bytes);return 0;
}

/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/ui/stagefx.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
int main(void){
    unsigned mode,t,x,y,count;
    for(mode=1;mode<STFX_N;mode++){
        count=0;
        for(t=0;t<960;t+=120)for(y=0;y<214;y++)for(x=0;x<240;x++){
            unsigned p=stagefx_pixel(mode,t,100,x,y);
            assert(p==stagefx_pixel(mode,t,100,x,y));count+=p!=0;
            assert(!stagefx_pixel(mode,t,0,x,y));
        }
        assert(count);assert(!stagefx_pixel(mode,960,100,8,28));
        assert(!stagefx_pixel(mode,0,100,240,28));
    }
    assert(!stagefx_pixel(STFX_N,0,100,0,0));puts("stagefx: deterministic effects, expiry, bounds, silence passed");return 0;
}

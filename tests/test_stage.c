/* SPDX-License-Identifier: MIT */
#include "firmware/ui/stage.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
int main(void){
    MikuStage s; miku_stage_init(&s);assert(s.bpm==120&&!s.playing);
    uint16_t *pixels=calloc(240*240,sizeof(*pixels));assert(pixels);
    miku_stage_draw(&s,pixels,240*240);assert(pixels[0]!=pixels[100*240+100]);
    miku_stage_button(&s,MIKU_BTN_PLAY|MIKU_BTN_VOWEL);
    assert(s.playing&&s.vowel==1);
    miku_stage_draw(&s,pixels,240*240);
    assert(pixels[223*240+15]!=pixels[10*240+10]);
    miku_stage_button(&s,MIKU_BTN_STOP|MIKU_BTN_NEXT);
    assert(!s.playing&&s.page==1);
    miku_stage_button(&s,MIKU_BTN_PREV);assert(s.page==0);
    free(pixels);puts("stage UI tests passed");return 0;
}

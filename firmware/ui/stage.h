/* SPDX-License-Identifier: MIT */
#ifndef MIKU_STAGE_H
#define MIKU_STAGE_H
#include <stdint.h>
enum {MIKU_LCD_W=240,MIKU_LCD_H=240};
enum {MIKU_BTN_PLAY=1,MIKU_BTN_STOP=2,MIKU_BTN_NEXT=4,MIKU_BTN_PREV=8,MIKU_BTN_VOWEL=16};
typedef struct {uint8_t playing,vowel,page,blink;uint16_t bpm;uint32_t beats;} MikuStage;
void miku_stage_init(MikuStage *s);
void miku_stage_button(MikuStage *s,uint32_t pressed);
void miku_stage_draw(const MikuStage *s,uint16_t *rgb565,uint32_t pixels);
#endif

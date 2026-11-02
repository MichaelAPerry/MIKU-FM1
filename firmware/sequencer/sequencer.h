/* SPDX-License-Identifier: MIT */
#ifndef MIKU_SEQUENCER_H
#define MIKU_SEQUENCER_H
#include <stdint.h>
#include "shared/song.h"
typedef struct {
    const MikuPattern *pattern;
    uint16_t bpm;
    uint32_t sample_rate;
    uint64_t cursor;
} MikuSequencer;
typedef struct { uint8_t kind; MikuNote note; } MikuEvent;
enum { MIKU_NOTE_ON=1, MIKU_NOTE_OFF=2 };
int miku_seq_init(MikuSequencer *s, const MikuPattern *p, uint16_t bpm, uint32_t sample_rate);
uint64_t miku_tick_to_sample(uint32_t tick, uint16_t bpm, uint32_t sample_rate);
/* One event per call; stable note order, NOTE_OFF before NOTE_ON at same sample.
   Caller owns events and must retain note state between callbacks. */
int miku_seq_next(MikuSequencer *s, uint64_t frame, MikuEvent *event);
#endif

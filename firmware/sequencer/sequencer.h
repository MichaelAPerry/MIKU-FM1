/* SPDX-License-Identifier: MIT */
#ifndef MIKU_SEQUENCER_H
#define MIKU_SEQUENCER_H
#include <stdint.h>
#include "shared/song.h"
enum { MIKU_MAX_EVENTS = 2 * MIKU_MAX_NOTES };
typedef struct { uint8_t kind; MikuNote note; } MikuEvent;
enum { MIKU_NOTE_ON=1, MIKU_NOTE_OFF=2 };
typedef struct {
    MikuEvent events[MIKU_MAX_EVENTS];
    uint64_t sample_times[MIKU_MAX_EVENTS];
    uint16_t count, cursor;
} MikuSequencer;
/* Precompute a sorted, bounded event queue. No allocations in playback. */
int miku_seq_init(MikuSequencer *s, const MikuPattern *p, uint16_t bpm, uint32_t sample_rate);
uint64_t miku_tick_to_sample(uint32_t tick, uint16_t bpm, uint32_t sample_rate);
int miku_seq_next(MikuSequencer *s, uint64_t frame, MikuEvent *event);
#endif

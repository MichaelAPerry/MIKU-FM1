/* SPDX-License-Identifier: MIT */
#include "firmware/sequencer/sequencer.h"
#include <stddef.h>
uint64_t miku_tick_to_sample(uint32_t tick, uint16_t bpm, uint32_t sr) {
    if(!bpm || !sr) return 0;
    return (uint64_t)tick * (uint64_t)sr * 60u / ((uint64_t)bpm * MIKU_PPQN);
}
int miku_seq_init(MikuSequencer *s, const MikuPattern *p, uint16_t bpm, uint32_t sr) {
    if(!s || !p || p->count>MIKU_MAX_NOTES || bpm<20 || bpm>300 || sr<8000) return 0;
    s->pattern=p; s->bpm=bpm; s->sample_rate=sr; s->cursor=0; return 1;
}
/* Stateless lookup intentionally favors correctness over speed in v0.1.
   Equal-time events are deterministic by array index. */
int miku_seq_next(MikuSequencer *s, uint64_t frame, MikuEvent *out) {
    if(!s || !out || !s->pattern) return 0;
    uint64_t best=UINT64_MAX; int best_index=-1; int best_kind=0;
    uint64_t ordinal=s->cursor;
    for(uint16_t i=0;i<s->pattern->count;i++) {
        const MikuNote *n=&s->pattern->notes[i];
        if(!miku_note_valid(n)) continue;
        for(int kind=MIKU_NOTE_OFF;kind>=MIKU_NOTE_ON;kind--) {
            uint32_t tick=n->start_tick+(kind==MIKU_NOTE_OFF?n->duration_tick:0u);
            uint64_t t=miku_tick_to_sample(tick,s->bpm,s->sample_rate);
            /* rank across all event positions, including skipped/previous */
            uint64_t rank=0;
            for(uint16_t j=0;j<s->pattern->count;j++){
                const MikuNote *m=&s->pattern->notes[j];
                if(!miku_note_valid(m)) continue;
                for(int k=MIKU_NOTE_OFF;k>=MIKU_NOTE_ON;k--){
                    uint32_t mtick=m->start_tick+(k==MIKU_NOTE_OFF?m->duration_tick:0u);
                    uint64_t mt=miku_tick_to_sample(mtick,s->bpm,s->sample_rate);
                    if(mt<t || (mt==t && (k>kind || (k==kind && j<i)))) rank++;
                }
            }
            if(rank==ordinal && t<=frame && t<best){best=t;best_index=i;best_kind=kind;}
        }
    }
    if(best_index<0) return 0;
    out->kind=(uint8_t)best_kind;out->note=s->pattern->notes[best_index];s->cursor++;
    return 1;
}

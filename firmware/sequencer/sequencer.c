/* SPDX-License-Identifier: MIT */
#include "firmware/sequencer/sequencer.h"
#include <string.h>
uint64_t miku_tick_to_sample(uint32_t tick, uint16_t bpm, uint32_t sr) {
    if (!bpm || !sr) return 0;
    return (uint64_t)tick * sr * 60u / ((uint64_t)bpm * MIKU_PPQN);
}
int miku_seq_init(MikuSequencer *s, const MikuPattern *p, uint16_t bpm, uint32_t sr) {
    if (!s || !p || p->count>MIKU_MAX_NOTES || bpm<20 || bpm>300 || sr<8000) return 0;
    memset(s,0,sizeof(*s));
    for(uint16_t i=0;i<p->count;i++){
        MikuNote n=p->notes[i];
        if(!miku_note_valid(&n)) return 0;
        for(int k=0;k<2;k++){
            uint8_t kind=k?MIKU_NOTE_OFF:MIKU_NOTE_ON;
            uint32_t tick=n.start_tick+(k?n.duration_tick:0);
            uint64_t when=miku_tick_to_sample(tick,bpm,sr);
            /* Stable insertion sort, OFF before ON for collisions. */
            uint16_t pos=s->count++;
            while(pos>0 && (s->sample_times[pos-1]>when ||
                  (s->sample_times[pos-1]==when &&
                   s->events[pos-1].kind<kind))){
                s->sample_times[pos]=s->sample_times[pos-1];
                s->events[pos]=s->events[pos-1];pos--;
            }
            s->sample_times[pos]=when;s->events[pos]=(MikuEvent){kind,n};
        }
    }
    return 1;
}
int miku_seq_next(MikuSequencer *s, uint64_t frame, MikuEvent *out) {
    if(!s || !out || s->cursor>=s->count || s->sample_times[s->cursor]>frame) return 0;
    *out=s->events[s->cursor++];return 1;
}

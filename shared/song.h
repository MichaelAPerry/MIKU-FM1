/* SPDX-License-Identifier: MIT */
#ifndef MIKU_SONG_H
#define MIKU_SONG_H
#include <stdint.h>
/* PPQN deliberately fixed for the initial binary-independent event model. */
enum { MIKU_PPQN=96, MIKU_TRACKS=4, MIKU_MAX_NOTES=128, MIKU_MAX_PATTERNS=16 };
enum { MIKU_VOCAL=0, MIKU_CHORDS=1, MIKU_BASS=2, MIKU_DRUMS=3 };
typedef enum { MIKU_A=0, MIKU_I=1, MIKU_U=2, MIKU_E=3, MIKU_O=4 } MikuVowel;
typedef struct {
    uint32_t start_tick;
    uint32_t duration_tick;
    uint8_t pitch;       /* MIDI 0..127 */
    uint8_t velocity;    /* MIDI 1..127 */
    uint8_t vowel;       /* vowel enum; first vocal prototype */
    uint8_t track;       /* track role 0..3 */
} MikuNote;
typedef struct {
    uint16_t count;
    MikuNote notes[MIKU_MAX_NOTES];
} MikuPattern;
typedef struct {
    uint16_t version;    /* currently 1, in-memory ABI is NOT a disk format */
    uint16_t bpm;
    uint8_t mute[MIKU_TRACKS];
    uint8_t pattern_count;
    MikuPattern patterns[MIKU_MAX_PATTERNS];
} MikuSong;
static inline int miku_note_valid(const MikuNote *n) {
    return n && n->track<MIKU_TRACKS && n->pitch<=127 && n->velocity>=1 &&
           n->velocity<=127 && n->duration_tick>0 &&
           n->start_tick<=UINT32_MAX-n->duration_tick &&
           (n->track!=MIKU_VOCAL || n->vowel<=MIKU_O);
}
static inline int miku_pattern_add(MikuPattern *p, MikuNote n) {
    if(!p || p->count>=MIKU_MAX_NOTES || !miku_note_valid(&n)) return 0;
    p->notes[p->count++]=n; return 1;
}
#endif

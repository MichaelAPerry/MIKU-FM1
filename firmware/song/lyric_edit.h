/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LYRIC_EDIT_H
#define LYRIC_EDIT_H
#include "firmware/song/lyrics.h"
#include "firmware/song/song.h"
enum { LE_OK=0, LE_ARGUMENT=-10, LE_CAPACITY=-11, LE_RANGE=-12, LE_OVERLAP=-13, LE_FLAGS=-14, LE_SUSTAIN=-15 };
/* Editor-control-time adapter: one tick is one existing sixteenth. No allocation, no ISR work.
 * Replaces an entire vocal pattern transactionally. Units must fit its 16-step grid.
 * Optional notes[count] supplies one MIDI pitch per unit (rest entries ignored); otherwise retain
 * the old note at each new onset, falling back to fallback_note. Velocity is 0..15.
 * Word/phrase markers and articulation persist in MKS2 v4 without changing the four-track model.
 * Use under the existing song edit lock; callers push pattern undo before successful commit. */
int ly_apply(Song *g,uint32_t pat,const LyUnit *units,uint32_t count,const uint8_t *notes,
             uint32_t fallback_note,uint32_t velocity);
/* Reconstruct editable units plus their pitches. Includes implicit gaps as rest units so the result
 * spans all 16 ticks. Legacy notes crossing the bar or overlapping another onset are explicitly refused.
 * Semantic normalization: a separated attack at the first note/after a rest needs no separate flag.
 * On any error outputs and *count are untouched. Optional notes may be NULL; units/count must exist. */
int ly_read(const Song *g,uint32_t pat,LyUnit *units,uint8_t *notes,uint32_t capacity,uint32_t *count);
/* Explicit lyric rest marker, separate from old empty-step bytes whose ignored length may be nonzero. */
int ly_rest_step(const Step *s);
#endif

/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_PHRASE_H
#define MIKU_PHRASE_H

#include "firmware/song/song.h"
#include "firmware/audio/fvoice/fvoice.h"

#define PH_N 64
#define PH_REC 1024
#define PH_SYL 48
#define PH_TEXT 128
#define PH_WORD_END 128u
#define PH_MELODY 1u
enum { PH_EN, PH_JP };
typedef struct { uint8_t ph[8]; char spell[6]; uint8_t note, len; } PhSyl;
typedef struct {
    uint8_t lang, bpm, n, deflen, flags;
    char name[12], text[PH_TEXT]; /* Space-padded name and zero-padded text. */
    PhSyl s[PH_SYL];
} Phrase;

/* Codecs return 0 on success and leave the destination untouched on error. */
int ph_encode(const Phrase *p, uint8_t rec[PH_REC]);
/* A null destination validates the record without materializing a Phrase. */
int ph_decode(const uint8_t rec[PH_REC], Phrase *p);
/* Input is a C string. Reports the failing byte offset, or -1 on success. */
int ph_compile(const char *text, uint32_t lang, Phrase *p, int *err_at);
/* Replaces the touched VOCAL patterns under the caller's song edit lock.
 * Returns 0 on success. On failure the song and used count are untouched. */
int ph_place(Song *g, uint32_t first_pat, uint32_t npat, const Phrase *p,
             uint8_t fallback_note, uint32_t *used);
void ph_fvsyl(const PhSyl *s, FvSyl *out);

#endif

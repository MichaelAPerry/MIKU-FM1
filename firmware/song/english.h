/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jameson Hodge (CHIMERA, https://github.com/jamesondh/chimera-vocal-synth)
 * C port for the M-VAVE FM-1: Copyright (c) 2026 Shan-Sherif.
 * Adapted for Miku-FM: integer-only English text-to-phoneme and syllabification. */
#ifndef MIKU_ENGLISH_H
#define MIKU_ENGLISH_H

#include <stdint.h>

enum {
    EP_NONE, EP_AA, EP_AE, EP_AH, EP_AO, EP_AW, EP_AY, EP_EH, EP_ER, EP_EY, EP_IH, EP_IY,
    EP_OW, EP_OY, EP_UH, EP_UW, EP_B, EP_CH, EP_D, EP_DH, EP_F, EP_G, EP_HH, EP_JH, EP_K,
    EP_L, EP_M, EP_N, EP_NG, EP_P, EP_R, EP_S, EP_SH, EP_T, EP_TH, EP_V, EP_W, EP_Y, EP_Z,
    EP_ZH, EP_COUNT
};
#define EP_IS_VOWEL(p) ((p) >= EP_AA && (p) <= EP_UW)

typedef struct { uint8_t on[3], v, co[4]; } EnSyl;
enum { EN_WORD_END = 1, EN_PHRASE_END = 2, EN_TRUNCATED = 4 };
enum { EN_ERR_CAP = -1, EN_ERR_INPUT = -2 };
typedef struct {
    EnSyl s;
    char text[8];   /* NUL-terminated spelling, at most seven bytes */
    uint8_t stress, flags;
} EnSylOut;
typedef const char *(*en_lookup_fn)(const char *word, void *user);

/* Invalid ARPAbet tokens are skipped. Phone and stress buffers hold cap bytes each.
 * On EN_ERR_CAP these two functions leave the written prefix in the buffers.
 * Words are ASCII, lowercased and limited to 47 bytes, with outer apostrophes removed.
 * The lookup receives the cleaned word and may return NULL to use the built-in data/rules. */
int en_arpabet(const char *arpa, uint8_t *ph, uint8_t *stress, int cap);
int en_word(const char *word, int reduce, en_lookup_fn lookup, void *user,
            uint8_t *ph, uint8_t *stress, int cap);

/* One word. Stress may be NULL (zero stress). Consonant-only words use v = EP_NONE.
 * Uses maximal legal onset, moving coda overflow into the following onset if legal.
 * Otherwise sets EN_TRUNCATED and keeps the onset nearest the vowel and earliest coda.
 * With no nucleus, keeps the first seven consonants in onset/coda order.
 * On error out is untouched. On success the last unit has EN_WORD_END. */
int en_syllabify(const uint8_t *ph, const uint8_t *stress, int n, const char *spelling,
                EnSylOut *out, int cap);

/* Returns 0 or EN_ERR_*. n receives the written count, err_at (optional) the byte
 * offset of the failing token or -1 on success. On error, previously completed
 * words remain, and entries from out[*n] onwards are untouched. Expanded number
 * words share the source digit run's offset. Each word may use up to 95 phones
 * (including caller-provided pronunciations). An unfinished final phrase is closed
 * on success. Hyphens are compound separators on fallback, as in running speech. */
int en_text(const char *text, en_lookup_fn lookup, void *user,
            EnSylOut *out, int cap, int *n, int *err_at);
/* en_text with a mode: EN_SPEECH (= en_text: weak forms of function words, "for" -> F AH0 R) or EN_SUNG (full
 * forms, as songs sing them). Miku-FM adaptation. */
enum { EN_SPEECH, EN_SUNG };
int en_text_ex(const char *text, int mode, en_lookup_fn lookup, void *user,
               EnSylOut *out, int cap, int *n, int *err_at);
/* AH/ER include unstressed variants, L includes dark L. T/D retain their
 * underlying codes in tap contexts. EP_NONE and invalid codes map to "". */
const char *en_ph_name(uint8_t ph);

#endif

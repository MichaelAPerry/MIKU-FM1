/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef LYRICS_H
#define LYRICS_H
#include <stdint.h>
/* Reusable, caller-owned phrase units. Timing is in caller-defined ticks; no pitch or tempo is baked in. */
enum { LY_REST=1, LY_SEPARATE=2, LY_SUSTAIN=4, LY_WORD_END=8, LY_PHRASE_END=16 };
typedef struct { uint16_t ticks; uint8_t mora, flags; } LyUnit;
enum { LY_OK=0, LY_SYNTAX=-1, LY_MORA=-2, LY_CAPACITY=-3, LY_DURATION=-4, LY_ARGUMENT=-5 };
/* Grammar: mora[:1..65535][!] separated by '-' within words, spaces between words.
 * '_' sustains the preceding vowel without a consonant; '/' is a rest; '|' is a phrase rest.
 * Durations default to default_ticks. ! requests a separate onset. Leading/trailing spaces allowed.
 * Parses twice: on failure output and *count are untouched; *error_at is a byte offset.
 * Null out with capacity=0 validates/counts only. No libc, allocation or floating point. */
int ly_compile(const char *text, uint16_t default_ticks, LyUnit *out, uint32_t capacity,
               uint32_t *count, uint32_t *error_at);
uint32_t ly_consonant(const LyUnit *unit); /* MC_NONE for rests and sustained vowels */
uint32_t ly_vowel(const LyUnit *unit);    /* MV_* from the actual mora table */
#endif

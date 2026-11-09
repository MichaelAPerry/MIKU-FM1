/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_FACTORY_SONG_H
#define MIKU_FACTORY_SONG_H
#include "song.h"
#define SF_COUNT 4u
const char *sf_name(uint32_t id); /* <=8 byte song name; NULL for invalid ID */
const char *sf_title(uint32_t id);
/* Reconstruct an ordinary editable Song. No flash writes, heap or extra Song
 * scratch. Invalid arguments leave destination unchanged. */
uint32_t sf_load(Song *song,uint32_t id);
#endif

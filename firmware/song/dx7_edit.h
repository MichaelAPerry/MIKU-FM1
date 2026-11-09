/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_DX7_EDIT_H
#define MIKU_DX7_EDIT_H
#include "song.h"
#define DB_PAGES 11u
uint32_t db_active(const Song *g,uint32_t track);
uint32_t db_select(Song *g,uint32_t track,uint32_t bank);
uint32_t db_value(const Song *g,uint32_t track,uint32_t op,uint32_t page,uint32_t knob);
uint32_t db_limit(uint32_t page,uint32_t knob);
uint32_t db_set(Song *g,uint32_t track,uint32_t op,uint32_t page,uint32_t knob,uint32_t value);
uint32_t db_toggle(Song *g,uint32_t track,uint32_t op,uint32_t page);
#endif

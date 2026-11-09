/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_SCENE_PREFS_H
#define MIKU_SCENE_PREFS_H
#include "performance.h"
#define SP_MAGIC 0x4D4B5632u
#define SP_SLOTS 10u
typedef struct {uint32_t magic;uint8_t ver,slot,pan[4],pad[2],scene[SP_SLOTS][3];} ScenePrefs;
void sp_defaults(ScenePrefs *p);
void sp_reset_scene(ScenePrefs *p,uint32_t slot);
uint32_t sp_valid(const ScenePrefs *p);
uint32_t sp_migrate(ScenePrefs *p,const uint8_t legacy[12]);
#endif

/* SPDX-License-Identifier: MIT */
#ifndef MIKU_SONG_IO_H
#define MIKU_SONG_IO_H
#include <stddef.h>
#include <stdint.h>
#include "shared/song.h"
/* Stable little-endian MIK1 format, fixed length header and variable notes.
   No raw struct serialization, bounded decode, CRC32 over preceding bytes. */
size_t miku_song_encoded_size(const MikuSong *s);
int miku_song_encode(const MikuSong *s,uint8_t *buf,size_t capacity,size_t *used);
int miku_song_decode(MikuSong *s,const uint8_t *buf,size_t length);
#endif

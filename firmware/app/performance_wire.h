/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_PERFORMANCE_WIRE_H
#define MIKU_PERFORMANCE_WIRE_H
#include <stdint.h>
#define PW_SIZE 28u
/* Version 1 evaluation protocol; 0x7d educational/non-commercial SysEx ID.
   All levels 0..127; sequence wraps at 16384; sample clock wraps at 2^32.
   Caller owns rate limiting, transport queue and disconnection policy. */
typedef struct {
    uint32_t sample;
    uint16_t sequence;
    uint8_t mouth, vocal_level, mora, section, row, step, playing, sprite;
    uint8_t outfit, background, frame, palette, audio_level, transient;
} PwState;
/* Exactly PW_SIZE bytes on success; zero on invalid state/capacity/message.
   Failure leaves destination untouched. No allocation, I/O, blocking or libc. */
uint32_t pw_encode(uint8_t *out,uint32_t capacity,const PwState *s);
uint32_t pw_decode(PwState *out,const uint8_t *message,uint32_t length);
#endif

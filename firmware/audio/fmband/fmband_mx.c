/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_tables.h"
void mx_gain(uint8_t level, uint8_t pan, int32_t *gl, int32_t *gr) {
    int32_t l = level > 127 ? 127 : level, g = l * l * 2;           /* 127 -> ~unity, quadratic taper */
    int32_t i = ((pan > 127 ? 127 : pan) + 1) >> 1;                 /* 0..64, 32 = centre (-3 dB each side) */
    *gl = (g * fmb_pan[i]) >> 15; *gr = (g * fmb_pan[64 - i]) >> 15;   /* 0 = hard left */
}
/* soft knee: linear to 24576, then y = k + t - t^2/32768 (slope 1 -> 0 at t = 16384), hard cap 32767 */
void mx_limit(int32_t *b, uint32_t n) {
    for (uint32_t i = 0; i < 2 * n; i++) {
        int32_t x = b[i] < -2147483647 ? -2147483647 : b[i], a = x < 0 ? -x : x;
        if (a > 24576) {
            int32_t t = a - 24576;
            a = t > 16383 ? 32767 : 24576 + t - ((t * t) >> 15);
            if (a > 32767) a = 32767;
            x = x < 0 ? -a : a;
        }
        b[i] = x;
    }
}

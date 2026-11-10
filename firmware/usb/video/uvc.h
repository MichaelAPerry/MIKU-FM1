/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded UVC1 MJPEG class core. Caller serializes all access, flushes hardware before stop/reset,
 * and never renders/encodes from a USB/audio interrupt. No allocation, libc, float or hardware access. */
#ifndef MIKU_UVC_H
#define MIKU_UVC_H
#include <stdint.h>
#define MUVC_WIDTH 160u
#define MUVC_HEIGHT 120u
#define MUVC_INTERVAL 666667u /* 100 ns; nominal15fps */
#define MUVC_FRAME_US 66667u
#define MUVC_FRAME_CAP 8192u
#define MUVC_PACKET_CAP 512u
#define MUVC_PROBE_LEN 26u
#ifndef MIKU_VIDEO_BENCH_FRAMES
#define MIKU_VIDEO_BENCH_FRAMES 3u
#endif
#ifndef MIKU_VIDEO_BENCH_END_MS
#define MIKU_VIDEO_BENCH_END_MS 0u
#endif
#ifndef MUVC_VC_IF
#define MUVC_VC_IF 0u
#endif
#ifndef MUVC_VS_IF
#define MUVC_VS_IF 1u
#endif
#define MUVC_EP 0x84u
/* A standalone VIDEO ONLY descriptor for feasibility: EP0 control + EP4 IN. No MIDI/audio/update claim.
 * Never substitute this descriptor into the rescue/update loader. */
extern const uint8_t muvc_device_descriptor[18];
extern const uint8_t muvc_configuration_descriptor[];
extern const uint16_t muvc_configuration_length;
enum { MUVC_RELEASE_SENT=1, MUVC_RELEASE_ERROR, MUVC_RELEASE_STOP, MUVC_RELEASE_RESET };
enum { MUVC_CTRL_STALL=-1, MUVC_CTRL_OUT=0, MUVC_CTRL_REPLY=1 };
typedef struct {
    const uint8_t *source;
    uint32_t length, offset, token, released_token, last_start_us;
    uint32_t frames, dropped, packets, rejected, stalls;
    uint16_t pending_bytes;
    uint8_t streaming, committed, probe_set, fid, started, clocked;
    uint8_t packet_pending, pending_eof, pending_error, error, release_reason, control_pending;
} Muvc;
void muvc_init(Muvc *s);
/* Caller handles standard SET_CONFIGURATION/SET_INTERFACE and invokes this only after flushing EP4.
 * alt1 requires committed controls; alt0 cancels ownership. Invalid alt leaves state intact. */
int muvc_stream(Muvc *s,uint32_t alt);
void muvc_reset(Muvc *s); /* bus reset/disconnect, after endpoint flush; statistics retained */
/* Accept one immutable complete JPEG. Caller retains memory until take_release; no replacement while busy.
 * offer requires SOI/EOI and <=8192 bytes but does not parse JPEG dimensions: encoder must match descriptor. */
int muvc_offer(Muvc *s,const uint8_t *jpeg,uint32_t bytes,uint32_t token);
/* Copies one payload, then waits for ack: a second prepare returns0. Source offset advances only on ack(1).
 * ack(0) retries same data. Buffer and source remain stable while hardware owns a packet.
 * now_us wraps naturally; call frequently enough to avoid >2^32us gaps while streaming. */
uint32_t muvc_prepare(Muvc *s,uint32_t now_us,uint8_t *packet,uint32_t capacity);
int muvc_ack(Muvc *s,uint32_t sent);
/* Mark current frame bad; finish an already queued packet first, then send an ERR|EOF header.
 * ERR completion releases ownership. Streaming can continue with a later frame. */
void muvc_frame_error(Muvc *s);
int muvc_take_release(Muvc *s,uint32_t *token,uint32_t *reason);
/* EP0 class interface requests: setup8 in USB byte order, reply capacity>=26 for probe replies.
 * OUT result means collect exactly26 bytes, then control_data. Every setup clears previous pending OUT.
 * GET replies report length; adapter applies USB wLength short reply behavior. Invalid requests STALL. */
int muvc_control_setup(Muvc *s,const uint8_t setup8[8],uint8_t *reply,uint32_t capacity,uint32_t *length);
int muvc_control_data(Muvc *s,const uint8_t *data,uint32_t length);
#endif

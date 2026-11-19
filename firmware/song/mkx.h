/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MIKU_MKX_H
#define MIKU_MKX_H
#include "firmware/song/phrase.h"

#define MKX_PROTO 1u
#define MKX_PAYLOAD_MAX 3000u
#define MKX_RAW_CAP (MKX_PAYLOAD_MAX + 4u)
#define MKX_FRAME_CAP (8u + MKX_RAW_CAP + (MKX_RAW_CAP + 6u) / 7u)
#define MKX_CHUNK_MAX 2048u
#define MKX_LIST_SIZE (PH_N * 14u)
#define MKX_SONG_SLOTS 10u

enum {
    MKX_HELLO = 0x01, MKX_PHRASE_GET, MKX_PHRASE_PUT, MKX_BANK_LIST,
    MKX_SONG_BEGIN, MKX_SONG_CHUNK, MKX_SONG_END,
    MKX_IDENT = 0x41, MKX_PHRASE, MKX_LIST = 0x44, MKX_ACK = 0x7f
};
enum { MKX_OK, MKX_BAD_FRAME, MKX_BAD_RECORD, MKX_BUSY, MKX_REFUSED };

/* Byte counts on success (including zero), negative on invalid input or capacity.
 * Source and destination buffers must be disjoint. */
int mkx_pack(const uint8_t *raw, uint32_t n, uint8_t *packed, uint32_t cap);
int mkx_unpack(const uint8_t *packed, uint32_t n, uint8_t *raw, uint32_t cap);
int mkx_frame_build(uint8_t cmd, uint8_t seq, const uint8_t *raw, uint32_t n,
                    uint8_t *frame, uint32_t cap);
/* raw needs room for the unpacked payload and its four CRC bytes. */
int mkx_frame_parse(const uint8_t *frame, uint32_t n, uint8_t *cmd, uint8_t *seq,
                    uint8_t *raw, uint32_t cap);

/* arg = index, total length, chunk offset, or acknowledged command.
 * status is for ACK only. data/len = record, chunk, LIST entries, or the
 * complete IDENT payload (including its three leading bytes and final NUL).
 * Parsed data borrows the supplied raw buffer. Typed codecs validate the
 * command envelope. PHRASE_PUT record validation is handled by the dispatcher. */
typedef struct {
    uint8_t cmd, seq, status;
    uint16_t arg;
    const uint8_t *data;
    uint32_t len;
} MkxMessage;

int mkx_message_build(const MkxMessage *m, uint8_t scratch[MKX_RAW_CAP],
                      uint8_t *frame, uint32_t cap);
int mkx_message_parse(const uint8_t *frame, uint32_t n, uint8_t raw[MKX_RAW_CAP],
                      MkxMessage *m);

/* Callbacks return 0 for success. An empty phrase_get fills its record with
 * zeros. Song callbacks are optional: begin/chunk gate acceptance, end is
 * notified after a successful decode into the caller's staging Song.
 * Initialize with zeroed upload lengths. All buffers must be disjoint. */
typedef struct {
    void *user;
    int (*phrase_get)(void *, uint8_t, uint8_t record[PH_REC]);
    int (*phrase_put)(void *, uint8_t, const uint8_t record[PH_REC]);
    int (*bank_list)(void *, uint8_t entries[MKX_LIST_SIZE]);
    int (*song_begin)(void *, uint16_t);
    int (*song_chunk)(void *, uint16_t, const uint8_t *, uint32_t);
    void (*song_end)(void *, const Song *);
    uint8_t busy;
    const char *version;
    uint8_t *scratch;        /* MKX_RAW_CAP bytes */
    uint8_t *song_buffer;    /* SONG_ENC_CAP bytes */
    Song *song;
    uint16_t song_total, song_received;
} MkxHost;

/* Returns reply byte count, or a negative value if no reply can be framed.
 * Serialize calls on a given host and hold the song edit lock as needed. */
int mkx_handle(const uint8_t *frame, uint32_t n, MkxHost *h, uint8_t *reply, uint32_t cap);
#endif

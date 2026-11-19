/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/mkx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static uint8_t raw[MKX_RAW_CAP], scratch[MKX_RAW_CAP];
static uint8_t frame[MKX_FRAME_CAP], reply[MKX_FRAME_CAP];

static void packing(void)
{
    uint8_t source[MKX_PAYLOAD_MAX], cmd, seq;
    for (uint32_t i = 0; i < sizeof source; i++) source[i] = (uint8_t)(i * 173u + 0xb5u);
    for (uint32_t n = 0; n <= 3000; n += n == 20 ? 2980 : 1) {
        int size = mkx_pack(source, n, frame, sizeof frame);
        CHECK(size == (int)(n + (n + 6u) / 7u));
        CHECK(mkx_unpack(frame, (uint32_t)size, raw, sizeof raw) == (int)n);
        CHECK(!memcmp(source, raw, n));
        size = mkx_frame_build(7, 67, source, n, frame, sizeof frame);
        CHECK(size > 0 && mkx_frame_parse(frame, (uint32_t)size, &cmd, &seq, raw, sizeof raw) == (int)n);
        CHECK(cmd == 7 && seq == 67 && !memcmp(source, raw, n));
        CHECK(mkx_frame_parse(frame + 1, (uint32_t)size - 2, 0, 0, raw, sizeof raw) == (int)n);
        CHECK(mkx_frame_parse(frame, (uint32_t)size, 0, 0, raw, n + 3) < 0);
        frame[8] ^= 1;
        CHECK(mkx_frame_parse(frame, (uint32_t)size, 0, 0, raw, sizeof raw) < 0);
        frame[8] ^= 1;
        frame[size - 1] = 0;
        CHECK(mkx_frame_parse(frame, (uint32_t)size, 0, 0, raw, sizeof raw) < 0);
    }
    CHECK(mkx_unpack((const uint8_t[]){0}, 1, raw, sizeof raw) < 0);
    CHECK(mkx_unpack((const uint8_t[]){2, 0}, 2, raw, sizeof raw) < 0);
    CHECK(mkx_unpack((const uint8_t[]){0, 128}, 2, raw, sizeof raw) < 0);
    CHECK(mkx_frame_build(1, 0, source, 3001, frame, sizeof frame) < 0);
    CHECK(mkx_frame_build(1, 128, source, 0, frame, sizeof frame) < 0);
}

static void commands(void)
{
    static const uint8_t ident[] = {1, 64, 10, '0', '.', '9', 0};
    static const uint8_t record[PH_REC], list[MKX_LIST_SIZE], chunk[MKX_CHUNK_MAX];
    const MkxMessage messages[] = {
        {.cmd = MKX_HELLO}, {.cmd = MKX_PHRASE_GET, .arg = 63},
        {.cmd = MKX_PHRASE_PUT, .arg = 6, .data = record, .len = sizeof record},
        {.cmd = MKX_BANK_LIST}, {.cmd = MKX_SONG_BEGIN, .arg = SONG_ENC_CAP},
        {.cmd = MKX_SONG_CHUNK, .arg = 2048, .data = chunk, .len = sizeof chunk},
        {.cmd = MKX_SONG_END}, {.cmd = MKX_IDENT, .data = ident, .len = sizeof ident},
        {.cmd = MKX_PHRASE, .arg = 2, .data = record, .len = sizeof record},
        {.cmd = MKX_LIST, .data = list, .len = sizeof list},
        {.cmd = MKX_ACK, .arg = MKX_PHRASE_PUT, .status = MKX_BUSY}
    };
    for (size_t i = 0; i < sizeof messages / sizeof messages[0]; i++) {
        MkxMessage m = messages[i], parsed;
        m.seq = (uint8_t)(127 - i);
        int size = mkx_message_build(&m, scratch, frame, sizeof frame);
        CHECK(size > 0 && mkx_message_parse(frame, (uint32_t)size, raw, &parsed) == 0);
        CHECK(parsed.cmd == m.cmd && parsed.seq == m.seq && parsed.arg == m.arg &&
              parsed.status == m.status && parsed.len == m.len);
        CHECK(!m.len || !memcmp(parsed.data, m.data, m.len));
    }
}

typedef struct {
    uint8_t record[PH_REC];
    unsigned writes, begins, chunks, ends;
    Song staged;
    uint8_t upload[SONG_ENC_CAP];
} Fake;

static int put(void *user, uint8_t index, const uint8_t *rec)
{
    Fake *f = user;
    if (index != 7) return -1;
    memcpy(f->record, rec, PH_REC);
    f->writes++;
    return 0;
}

static int get(void *user, uint8_t index, uint8_t *rec)
{
    if (index == 7) memcpy(rec, ((Fake *)user)->record, PH_REC);
    else memset(rec, 0, PH_REC);
    return 0;
}

static int bank(void *user, uint8_t *entries)
{
    Fake *f = user;
    memset(entries, 0, MKX_LIST_SIZE);
    if (f->writes) {
        entries[7 * 14] = 1;
        entries[7 * 14 + 1] = f->record[5];
        memcpy(entries + 7 * 14 + 2, f->record + 12, 12);
    }
    return 0;
}

static int begin(void *user, uint16_t n)
{
    CHECK(n >= SONG_ENC_V1);
    ((Fake *)user)->begins++;
    return 0;
}

static int chunk(void *user, uint16_t offset, const uint8_t *data, uint32_t n)
{
    CHECK((offset % 2048) == 0 && data && n);
    ((Fake *)user)->chunks++;
    return 0;
}

static void end(void *user, const Song *song)
{
    Fake *f = user;
    CHECK(song == &f->staged);
    f->ends++;
}

static MkxMessage exchange(MkxHost *h, MkxMessage m, int corrupt)
{
    MkxMessage r;
    m.seq = 99;
    int size = mkx_message_build(&m, raw, frame, sizeof frame);
    CHECK(size > 0);
    if (corrupt) frame[8] ^= 1;
    int result = mkx_handle(frame + 1, (uint32_t)size - 2, h, reply, sizeof reply);
    CHECK(result > 0 && mkx_message_parse(reply, (uint32_t)result, raw, &r) == 0 && r.seq == 99);
    if (r.cmd == MKX_ACK) CHECK(r.arg == m.cmd);
    return r;
}

static void dispatcher(void)
{
    Fake f = {0};
    MkxHost h = {.user = &f, .phrase_put = put, .phrase_get = get, .bank_list = bank,
        .song_begin = begin, .song_chunk = chunk, .song_end = end, .version = "0.9",
        .scratch = scratch, .song_buffer = f.upload, .song = &f.staged};
    Phrase p = {.bpm = 123, .deflen = 2, .name = "TEST        "};
    uint8_t record[PH_REC], encoded[SONG_ENC_CAP];
    Song song;
    MkxMessage r = exchange(&h, (MkxMessage){.cmd = MKX_HELLO}, 0);
    CHECK(r.cmd == MKX_IDENT && r.data[1] == PH_N && r.data[2] == MKX_SONG_SLOTS &&
          !strcmp((const char *)r.data + 3, h.version));
    r = exchange(&h, (MkxMessage){.cmd = MKX_PHRASE_GET, .arg = 0}, 0);
    CHECK(r.cmd == MKX_PHRASE && !memcmp(f.record, r.data, PH_REC));

    CHECK(ph_compile("Miku", PH_EN, &p, 0) == 0 && ph_encode(&p, record) == 0);
    MkxMessage m = {.cmd = MKX_PHRASE_PUT, .arg = 7, .data = record, .len = PH_REC};
    CHECK(exchange(&h, m, 1).status == MKX_BAD_FRAME && f.writes == 0);
    h.busy = 1;
    CHECK(exchange(&h, m, 0).status == MKX_BUSY && f.writes == 0);
    h.busy = 0;
    record[0] ^= 1;
    CHECK(exchange(&h, m, 0).status == MKX_BAD_RECORD && f.writes == 0);
    record[0] ^= 1;
    CHECK(exchange(&h, m, 0).status == MKX_OK && f.writes == 1);
    r = exchange(&h, (MkxMessage){.cmd = MKX_PHRASE_GET, .arg = 7}, 0);
    CHECK(r.cmd == MKX_PHRASE && r.arg == 7 && !memcmp(record, r.data, PH_REC));
    r = exchange(&h, (MkxMessage){.cmd = MKX_BANK_LIST}, 0);
    CHECK(r.cmd == MKX_LIST && r.data[98] == 1 && !memcmp(r.data + 100, p.name, 12));

    sg_init(&song);
    song.bpm = 151;
    CHECK(ph_place(&song, 0, 1, &p, 60, 0) == 0);
    uint32_t size = song_encode(&song, encoded, sizeof encoded);
    CHECK(size > 4096);
    for (int corrupt = 1; corrupt >= 0; corrupt--) {
        encoded[20] ^= 1;
        CHECK(exchange(&h, (MkxMessage){.cmd = MKX_SONG_BEGIN, .arg = (uint16_t)size}, 0).status == MKX_OK);
        m = (MkxMessage){.cmd = MKX_SONG_CHUNK, .arg = 2048, .data = encoded + 2048, .len = 2048};
        CHECK(exchange(&h, m, 0).status == MKX_REFUSED && h.song_received == 0);
        CHECK(exchange(&h, (MkxMessage){.cmd = MKX_SONG_END}, 0).status == MKX_REFUSED);
        for (uint32_t at = 0; at < size; at += MKX_CHUNK_MAX) {
            uint32_t n = size - at;
            if (n > MKX_CHUNK_MAX) n = MKX_CHUNK_MAX;
            m = (MkxMessage){.cmd = MKX_SONG_CHUNK, .arg = (uint16_t)at, .data = encoded + at, .len = n};
            CHECK(exchange(&h, m, 0).status == MKX_OK && f.staged.bpm == 0);
        }
        r = exchange(&h, (MkxMessage){.cmd = MKX_SONG_END}, 0);
        CHECK(r.status == (corrupt ? MKX_BAD_RECORD : MKX_OK));
        CHECK(f.staged.bpm == (corrupt ? 0 : 151));
    }
    CHECK(f.begins == 2 && f.chunks == 6 && f.ends == 1 && f.staged.nsyl == 2);
    CHECK(!memcmp(f.staged.pat, song.pat, sizeof song.pat));
}

int main(void)
{
    packing();
    commands();
    dispatcher();
    return 0;
}

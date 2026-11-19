/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/mkx.h"

static uint32_t mkx_packed_size(uint32_t n)
{
    return n + (n + 6u) / 7u;
}

/* Append groups from a payload and an optional four-byte CRC suffix. */
static void mkx_pack_bytes(const uint8_t *raw, uint32_t n, uint32_t extra,
                           uint32_t crc, uint8_t *packed)
{
    for (uint32_t i = 0; i < n + extra;) {
        uint8_t *high = packed++;
        *high = 0;
        for (uint32_t j = 0; j < 7 && i < n + extra; j++, i++) {
            uint8_t b = i < n ? raw[i] : (uint8_t)(crc >> (8u * (i - n)));
            *high |= (uint8_t)((b >> 7) << j);
            *packed++ = b & 127u;
        }
    }
}

int mkx_pack(const uint8_t *raw, uint32_t n, uint8_t *packed, uint32_t cap)
{
    uint32_t size;
    if (n > MKX_RAW_CAP || (n && (!raw || !packed))) return -1;
    size = mkx_packed_size(n);
    if (cap < size) return -1;
    mkx_pack_bytes(raw, n, 0, 0, packed);
    return (int)size;
}

int mkx_unpack(const uint8_t *packed, uint32_t n, uint8_t *raw, uint32_t cap)
{
    uint32_t size;
    if (n > mkx_packed_size(MKX_RAW_CAP) || (n && (!packed || !raw))) return -1;
    size = n - (n + 7u) / 8u;
    if (n % 8u == 1 || cap < size) return -1;
    for (uint32_t i = 0, out = 0; i < n;) {
        uint32_t high = packed[i++], group = n - i;
        if (group > 7) group = 7;
        if (high >> group) return -1;
        for (uint32_t j = 0; j < group; j++, i++) {
            if (packed[i] > 127) return -1;
            raw[out++] = (uint8_t)(packed[i] | ((high >> j) & 1u) << 7);
        }
    }
    return (int)size;
}

int mkx_frame_build(uint8_t cmd, uint8_t seq, const uint8_t *raw, uint32_t n,
                    uint8_t *frame, uint32_t cap)
{
    uint32_t size;
    if (!frame || (n && !raw) || cmd > 127 || seq > 127 || n > MKX_PAYLOAD_MAX) return -1;
    size = 8u + mkx_packed_size(n + 4u);
    if (cap < size) return -1;
    frame[0] = 0xf0;
    frame[1] = 0x7d;
    frame[2] = 0x4d;
    frame[3] = 0x4b;
    frame[4] = MKX_PROTO;
    frame[5] = cmd;
    frame[6] = seq;
    mkx_pack_bytes(raw, n, 4, sg_crc32(raw, n), frame + 7);
    frame[size - 1u] = 0xf7;
    return (int)size;
}

/* Exposes the routing bytes even when the payload or terminator is damaged. */
static const uint8_t *mkx_header(const uint8_t *frame, uint32_t n)
{
    if (!frame || !n) return 0;
    if (frame[0] == 0xf0) { frame++; n--; }
    if (n < 6 || frame[0] != 0x7d || frame[1] != 0x4d || frame[2] != 0x4b ||
        frame[3] != MKX_PROTO || frame[4] > 127 || frame[5] > 127) return 0;
    return frame;
}

int mkx_frame_parse(const uint8_t *frame, uint32_t n, uint8_t *cmd, uint8_t *seq,
                    uint8_t *raw, uint32_t cap)
{
    const uint8_t *head = mkx_header(frame, n);
    uint32_t crc = 0;
    int len;
    if (!head || n > MKX_FRAME_CAP) return -1;
    if (head != frame) {
        if (frame[n - 1u] != 0xf7) return -1;
        n -= 2;
    }
    len = mkx_unpack(head + 6, n - 6, raw, cap);
    if (len < 4) return -1;
    len -= 4;
    for (uint32_t i = 0; i < 4; i++) crc |= (uint32_t)raw[len + i] << (8u * i);
    if (crc != sg_crc32(raw, (uint32_t)len)) return -1;
    if (cmd) *cmd = head[4];
    if (seq) *seq = head[5];
    return len;
}

static int mkx_message_ok(const MkxMessage *m)
{
    const uint8_t *d = m->data;
    uint32_t n = m->len;
    if (n > MKX_PAYLOAD_MAX || (n && !d)) return 0;
    switch (m->cmd) {
    case MKX_HELLO: case MKX_BANK_LIST: case MKX_SONG_END:
        return !n;
    case MKX_PHRASE_GET:
        return !n && m->arg < PH_N;
    case MKX_PHRASE_PUT: case MKX_PHRASE:
        return n == PH_REC && m->arg < PH_N;
    case MKX_SONG_BEGIN:
        return !n && m->arg >= SONG_ENC_V1 && m->arg <= SONG_ENC_CAP;
    case MKX_SONG_CHUNK:
        return n && n <= MKX_CHUNK_MAX && m->arg + n <= SONG_ENC_CAP;
    case MKX_IDENT:
        if (n < 4 || d[0] != MKX_PROTO || d[n - 1]) return 0;
        for (uint32_t i = 3; i + 1 < n; i++)
            if (d[i] < 32 || d[i] > 126) return 0;
        return 1;
    case MKX_LIST:
        if (n != MKX_LIST_SIZE) return 0;
        for (uint32_t i = 0; i < n; i += 14) {
            if (d[i] > 1 || (d[i] && d[i + 1] > PH_JP)) return 0;
            if (d[i]) for (uint32_t j = i + 2; j < i + 14; j++) {
                uint8_t c = d[j];
                if (!(c == ' ' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
                    return 0;
            }
        }
        return 1;
    case MKX_ACK:
        return !n && m->arg <= 127 && m->status <= MKX_REFUSED;
    }
    return 0;
}

static uint32_t mkx_prefix(uint8_t cmd)
{
    switch (cmd) {
    case MKX_PHRASE_GET: case MKX_PHRASE_PUT: case MKX_PHRASE: return 1;
    case MKX_SONG_BEGIN: case MKX_SONG_CHUNK: case MKX_ACK: return 2;
    default: return 0;
    }
}

int mkx_message_build(const MkxMessage *m, uint8_t scratch[MKX_RAW_CAP],
                      uint8_t *frame, uint32_t cap)
{
    uint32_t prefix;
    if (!m || !scratch || !mkx_message_ok(m)) return -1;
    prefix = mkx_prefix(m->cmd);
    /* data may already be exactly scratch + prefix (e.g. a bank callback). */
    for (uint32_t i = 0; i < m->len; i++) scratch[prefix + i] = m->data[i];
    if (prefix) scratch[0] = (uint8_t)m->arg;
    if (prefix == 2) scratch[1] = m->cmd == MKX_ACK ? m->status : (uint8_t)(m->arg >> 8);
    return mkx_frame_build(m->cmd, m->seq, scratch, prefix + m->len, frame, cap);
}

static int mkx_read_message(uint8_t cmd, uint8_t seq, const uint8_t *raw,
                            uint32_t n, MkxMessage *m)
{
    MkxMessage v = {0, 0, 0, 0, 0, 0};
    uint32_t prefix = mkx_prefix(cmd);
    if (n < prefix) return -1;
    v.cmd = cmd;
    v.seq = seq;
    v.data = raw + prefix;
    v.len = n - prefix;
    if (prefix) v.arg = raw[0];
    if (cmd == MKX_ACK) v.status = raw[1];
    else if (prefix == 2) v.arg |= (uint16_t)raw[1] << 8;
    if (!mkx_message_ok(&v)) return -1;
    m->cmd = v.cmd;
    m->seq = v.seq;
    m->status = v.status;
    m->arg = v.arg;
    m->data = v.data;
    m->len = v.len;
    return 0;
}

int mkx_message_parse(const uint8_t *frame, uint32_t n, uint8_t raw[MKX_RAW_CAP],
                      MkxMessage *m)
{
    uint8_t cmd, seq;
    int len = mkx_frame_parse(frame, n, &cmd, &seq, raw, MKX_RAW_CAP);
    return len < 0 || !m ? -1 : mkx_read_message(cmd, seq, raw, (uint32_t)len, m);
}

static uint8_t mkx_upload(MkxHost *h, const MkxMessage *m)
{
    if (!h->song_buffer || !h->song) return MKX_REFUSED;
    switch (m->cmd) {
    case MKX_SONG_BEGIN:
        if (h->song_begin && h->song_begin(h->user, m->arg)) return MKX_REFUSED;
        h->song_total = m->arg;
        h->song_received = 0;
        return MKX_OK;
    case MKX_SONG_CHUNK:
        if (!h->song_total || m->arg != h->song_received || m->arg + m->len > h->song_total ||
            (h->song_chunk && h->song_chunk(h->user, m->arg, m->data, m->len)))
            return MKX_REFUSED;
        for (uint32_t i = 0; i < m->len; i++) h->song_buffer[m->arg + i] = m->data[i];
        h->song_received = (uint16_t)(m->arg + m->len);
        return MKX_OK;
    case MKX_SONG_END: {
        uint16_t size = h->song_total;
        if (!size || h->song_received != size) return MKX_REFUSED;
        h->song_total = h->song_received = 0;
        if (song_decode(h->song, h->song_buffer, size)) return MKX_BAD_RECORD;
        if (h->song_end) h->song_end(h->user, h->song);
        return MKX_OK;
    }
    }
    return MKX_REFUSED;
}

int mkx_handle(const uint8_t *frame, uint32_t n, MkxHost *h, uint8_t *reply, uint32_t cap)
{
    const uint8_t *head = mkx_header(frame, n);
    MkxMessage m, r = {0, 0, 0, 0, 0, 0};
    uint8_t *raw;
    int len;
    /* An ACK needs 15 bytes. Reserve its space before invoking any writer. */
    if (!head || !h || !h->scratch || !reply || cap < 15) return -1;
    raw = h->scratch;
    r.cmd = MKX_ACK;
    r.arg = head[4];
    r.seq = head[5];
    len = mkx_frame_parse(frame, n, 0, 0, raw, MKX_RAW_CAP);
    if (len < 0) r.status = MKX_BAD_FRAME;
    else if (r.arg < MKX_HELLO || r.arg > MKX_SONG_END ||
             mkx_read_message((uint8_t)r.arg, r.seq, raw, (uint32_t)len, &m))
        r.status = MKX_REFUSED;
    else switch (m.cmd) {
    case MKX_HELLO: {
        uint32_t i = 3;
        const char *v = h->version ? h->version : "";
        raw[0] = MKX_PROTO;
        raw[1] = PH_N;
        raw[2] = MKX_SONG_SLOTS;
        while (*v && i < MKX_PAYLOAD_MAX - 1u) raw[i++] = (uint8_t)*v++;
        raw[i++] = 0;
        r.cmd = MKX_IDENT;
        r.data = raw;
        r.len = i;
        break;
    }
    case MKX_PHRASE_GET:
        if (!h->phrase_get || h->phrase_get(h->user, (uint8_t)m.arg, raw + 1)) r.status = MKX_REFUSED;
        else {
            r.cmd = MKX_PHRASE;
            r.arg = m.arg;
            r.data = raw + 1;
            r.len = PH_REC;
        }
        break;
    case MKX_PHRASE_PUT:
        if (h->busy) r.status = MKX_BUSY;
        else if (ph_decode(m.data, 0)) r.status = MKX_BAD_RECORD;
        else if (!h->phrase_put || h->phrase_put(h->user, (uint8_t)m.arg, m.data))
            r.status = MKX_REFUSED;
        break;
    case MKX_BANK_LIST:
        if (!h->bank_list || h->bank_list(h->user, raw)) r.status = MKX_REFUSED;
        else {
            r.cmd = MKX_LIST;
            r.data = raw;
            r.len = MKX_LIST_SIZE;
        }
        break;
    default:
        r.status = mkx_upload(h, &m);
        break;
    }
    len = mkx_message_build(&r, raw, reply, cap);
    if (len >= 0 || r.cmd == MKX_ACK) return len;
    r.cmd = MKX_ACK;
    r.arg = head[4];
    r.status = MKX_REFUSED;
    r.len = 0;
    return mkx_message_build(&r, raw, reply, cap);
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* Host tool: sings English syllables with fvoice to a 16-bit 44.1 kHz stereo WAV, for listening.
   usage: english_voice_render out.wav [BPM] [SYL ...]
   A syllable is ONSET-VOWEL-CODA@NOTE:EIGHTHS, phonemes as ARPAbet names joined by '.', parts may be empty:
     HH-AH-@67:1  L-OW-@69:2  W-ER-L.D@72:3  -AY-@67:1  S.T.R-IH-NG@69:2
   AX is the schwa. Without syllables it sings a built-in phrase. The vowel lands on the beat (the onset starts early)
   and the coda ends with the note, as the sequencer will do. */
#include "firmware/audio/fvoice/fvoice.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FS 44100
static const char *EPN[] = {"", "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH", "IY", "OW", "OY", "UH", "UW",
    "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K", "L", "M", "N", "NG", "P", "R", "S", "SH", "T",
    "TH", "V", "W", "Y", "Z", "ZH", "AX"};
static void w16(FILE *f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
static void w32(FILE *f, unsigned v) { w16(f, v & 65535); w16(f, v >> 16); }
static int wav(const char *path, const int32_t *b, uint32_t n) {
    FILE *f = fopen(path, "wb"); if (!f) return 0;
    fwrite("RIFF", 1, 4, f); w32(f, 36 + n * 4); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w16(f, 1); w16(f, 2); w32(f, FS); w32(f, FS * 4); w16(f, 4); w16(f, 16);
    fwrite("data", 1, 4, f); w32(f, n * 4);
    for (uint32_t i = 0; i < n * 2; i++) { int32_t s = b[i]; w16(f, (unsigned)(s > 32767 ? 32767 : s < -32768 ? -32768 : s) & 65535); }
    return fclose(f) == 0;
}
static int code(const char *s, size_t n) {
    for (int i = 1; i < (int)(sizeof EPN / sizeof *EPN); i++) if (strlen(EPN[i]) == n && !strncmp(EPN[i], s, n)) return i;
    fprintf(stderr, "unknown phoneme %.*s\n", (int)n, s); exit(2);
}
static int part(const char *s, const char *e, uint8_t *out, int cap) {   /* "S.T.R" -> codes */
    int k = 0;
    while (s < e) {
        const char *d = s; while (d < e && *d != '.') d++;
        if (d > s) { if (k == cap) { fprintf(stderr, "too many phonemes\n"); exit(2); } out[k++] = (uint8_t)code(s, (size_t)(d - s)); }
        s = d + (d < e);
    }
    return k;
}
typedef struct { FvSyl s; uint8_t note, len; } Item;
static Item parse(const char *a) {
    Item it; memset(&it, 0, sizeof it); it.note = 67; it.len = 1;
    const char *d1 = strchr(a, '-'), *d2 = d1 ? strchr(d1 + 1, '-') : 0, *at = strchr(a, '@');
    if (!d1 || !d2) { fprintf(stderr, "bad syllable %s\n", a); exit(2); }
    const char *end = at ? at : a + strlen(a);
    part(a, d1, it.s.on, 3);
    uint8_t v[1] = {0}; if (part(d1 + 1, d2, v, 1)) it.s.v = v[0];
    part(d2 + 1, end, it.s.co, 4);
    if (at) { it.note = (uint8_t)atoi(at + 1); const char *c = strchr(at, ':'); if (c) it.len = (uint8_t)atoi(c + 1); }
    return it;
}
int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: english_voice_render out.wav [BPM] [SYL ...]\n"); return 2; }
    static const char *demo[] = {"HH-AX-@67:1", "L-OW-@69:2", "W-ER-L.D@72:3", "-AY-@72:1", "S-IH-NG@74:1", "F-AO-R@72:1",
        "Y-UW-@69:3", "TH-AE-NG.K.S@67:2", "S.T.R-IY-M@69:1", "-AH-V@67:1", "L-AY-T@64:4"};
    int bpm = argc > 2 ? atoi(argv[2]) : 100, n = argc > 3 ? argc - 3 : (int)(sizeof demo / sizeof *demo);
    if (bpm < 40 || bpm > 300) bpm = 100;
    Item *it = calloc((size_t)n, sizeof *it);
    uint32_t eighth = (uint32_t)(FS * 30 / bpm), total = FS / 2;
    for (int i = 0; i < n; i++) { it[i] = parse(argc > 3 ? argv[3 + i] : demo[i]); total += it[i].len * eighth; }
    int32_t *b = calloc((size_t)(total + FS * 2) * 2, sizeof *b);
    if (!it || !b) return 1;
    FvVoice v; fv_init(&v);
    v.vib_delay = 160; v.vib_depth = 25; v.porta = 50; v.release = 70; v.breath = 10;
    /* schedule: vowel i on beat start[i]; its onset starts fv_syl_onset_ms earlier; its coda starts so it ends at the
       next beat; a gap of at least 2 ms between a coda and the next onset (a real re-articulation) */
    uint32_t t = 0, beat = FS / 4;
    for (int i = 0; i < n; i++) {
        uint32_t on = (fv_syl_onset_ms(&it[i].s) * FS) / 1000, co = (fv_syl_coda_ms(&it[i].s) * FS) / 1000;
        uint32_t vstart = beat, nstart = vstart > on ? vstart - on : 0;
        if (nstart > t) { fv_render(&v, b + 2 * t, nstart - t); t = nstart; }
        fv_note_on_syl(&v, it[i].note, &it[i].s, 100);
        beat += it[i].len * eighth;
        uint32_t next_on = i + 1 < n ? (fv_syl_onset_ms(&it[i + 1].s) * FS) / 1000 : 0;
        uint32_t end_by = beat - next_on - FS / 500, off = end_by > co ? end_by - co : 0;   /* coda done before the next onset */
        if (off < t + FS / 50) off = t + FS / 50;
        fv_render(&v, b + 2 * t, off - t); t = off;
        fv_note_off_syl(&v, &it[i].s);
    }
    fv_render(&v, b + 2 * t, total + FS - t); t = total + FS;
    if (!wav(argv[1], b, t)) { fprintf(stderr, "cannot write %s\n", argv[1]); return 1; }
    printf("%s: %d syllables, %.2f s\n", argv[1], n, t / (double)FS);
    free(b); free(it);
    return 0;
}

/* SPDX-License-Identifier: GPL-3.0-only */
/* Miku-FM song model: 4 tracks x 16 patterns x 16 sixteenth steps, a 64-row arrangement. Fixed capacity, no heap.
 * sizeof(Song) = 4718 bytes, sizeof(Undo) = 2596 (8 entries x 324 + ring state). */
#ifndef SONG_H
#define SONG_H
#include <stdint.h>
#define NTRK 4
#define NPAT 16
#define NSTEP 16
#define NROW 64
enum { TR_VOCAL, TR_CHORDS, TR_BASS, TR_DRUMS };
/* A step is 4 bytes, meaning per track (index constants below). 0 in the note/root/mask byte = nothing. */
typedef struct { uint8_t v[4]; } Step;
enum { SV_NOTE, SV_LEN, SV_MORA, SV_FLAG };   /* VOCAL: MIDI 1..127, len 1..64 steps (may cross the bar), mora index, flags: velocity 0..15 low nibble, high bits VF_* (MKS2 v4) */
enum { VF_LEGATO=16, VF_SUSTAIN=32, VF_WORD_END=64, VF_PHRASE_END=128 }; /* outgoing hold; incoming no consonant; lyric boundaries */
enum { SC_ROOT, SC_TYPE, SC_LEN, SC_VEL };    /* CHORDS: root MIDI, type = mode<<4 | quality, len 1..64, vel 0..15 */
enum { SB_NOTE, SB_LEN, SB_FLAG, SB_VEL };    /* BASS: note, len, flags BF_*, vel 0..15 */
enum { SD_MASK, SD_ACC };                     /* DRUMS: instrument mask, accent mask (bits D_*), 0, 0 */
enum { BF_ACCENT = 1, BF_SLIDE = 2 };         /* BF_SLIDE: this note glides (legato) into the next bass note */
enum { CQ_MAJ, CQ_MIN, CQ_7, CQ_MAJ7, CQ_MIN7, CQ_SUS4, CQ_SUS2, CQ_DIM, CQ_AUG, CQ_ADD9 };
enum { CM_BLOCK, CM_UP, CM_DOWN, CM_UPDOWN, CM_STRUM, CM_UP32, CM_DOWN32, CM_UPDOWN32 };   /* high nibble: arp at 16ths or 32nds */
enum { D_KICK = 1, D_SNARE = 2, D_CLAP = 4, D_CHAT = 8, D_OHAT = 16, D_PERC = 32 };
enum { SEC_NONE, SEC_INTRO, SEC_VERSE, SEC_CHORUS, SEC_BRIDGE, SEC_OUTRO, SEC_N };
#define ROW_OFF 0xFFu                         /* pattern index: track silent in this row */
typedef struct { uint8_t level, mute, solo, preset, vib, porta, breath, bright; } Trk;  /* level 0..127, preset 0..7 classic, 64..72 DX7 on chords/bass; voice 0..100 */
typedef struct { uint8_t pat[NTRK]; uint8_t sect; } Row;
/* The voice's sound (STAGE knobs), 0..100 each unless noted. Defaults (sg_init) give 0.5's sound exactly. */
enum { VC_DEEP, VC_LOW, VC_MIKU, VC_YOUNG, VC_TINY, VC_N };          /* character: formant size */
enum { VL_JP, VL_ANIMAL, VL_SCAT, VL_VOWEL, VL_N };                  /* language: morae as written, animal babble, scat, vowels only */
typedef struct {
    uint8_t atk, rel, scoop, human;           /* attack, release (v*v/20 ms: 0..500), onset scoop (cents), human feel */
    uint8_t vrate, vdel, trem;                /* vibrato rate (3..8 Hz), vibrato delay (v*v/10 ms), tremolo depth */
    uint8_t cho, dly, rev, drv;               /* effects: chorus, delay (dotted eighth), reverb, drive; wet levels and drive gain go with v*v */
    uint8_t chr, lang;                        /* VC_*, VL_* */
    uint8_t rsv[3];                           /* 0 */
    /* version 3 (the hold-a-button layers); 0..100 each, 50 = neutral where it says so */
    uint8_t tens, fry, whsp, fmt;             /* SOURCE: tension (50 neutral), vocal fry, whisper, formant fine shift (50 neutral) */
    uint8_t rsize, dtime, dfb, crate;         /* FX DEEP: reverb size, delay time (5 divisions), delay feedback, chorus rate */
    uint8_t clen, clev, vspd, obeat;          /* ARTICULATION: consonant length, consonant level, vowel speed (50 = 100 %), on-beat */
    uint8_t hint, hmix, hdet, hchr;           /* HARMONY: interval (7 steps, first = off), mix, detune, its character (0..4) */
} Voice;
typedef struct {
    char name[8];                             /* A-Z 0-9 space */
    uint8_t bpm, swing, master, loop, nrows;  /* 40..240, 0..75 (% of a sixteenth that odd sixteenths are delayed), 0..127, 0/1, 0..NROW */
    int8_t transpose;                         /* -24..24 semitones on VOCAL/CHORDS/BASS */
    Trk trk[NTRK];
    uint8_t dx7[2][128];                     /* v5 embedded chord/bass DX7 snapshots; ignored for classic presets */
    Voice vx;                                 /* the voice's sound (MKS2 version 2; version 1 files get the defaults) */
    Row row[NROW];
    Step pat[NTRK][NPAT][NSTEP];
} Song;
void sg_init(Song *g);                                        /* empty song, 120 BPM */
int sg_step_ok(int trk, const Step *s);                       /* field bounds for a track */
int sg_set_step(Song *g, int trk, int pat, int step, const Step *s);   /* all editing helpers: 1 = done, 0 = refused (nothing changed) */
int sg_clear_step(Song *g, int trk, int pat, int step);
int sg_copy(const Song *g, int trk, int pat, int from, int n, Step *out);
int sg_paste(Song *g, int trk, int pat, int at, const Step *in, int n);
int sg_pat_empty(const Song *g, int trk, int pat);
int sg_clear_pat(Song *g, int trk, int pat);
int sg_dup_pat(Song *g, int trk, int src);                    /* new slot index, or -1 */
int sg_find_free(const Song *g, int trk);                     /* empty and unused pattern, or -1 */
int sg_transpose(Song *g, int trk, int pat, int semis);       /* not DRUMS; refused if a note would leave 1..127 */
int sg_row_insert(Song *g, int at);                           /* blank row (all tracks off) before row `at` (0..nrows) */
int sg_row_delete(Song *g, int at);
int sg_row_dup(Song *g, int at);                              /* copy inserted after row `at` */
int sg_row_set(Song *g, int row, int trk, int pat);           /* pat 0..NPAT-1 or ROW_OFF */
int sg_row_sect(Song *g, int row, int sect);
/* Undo: snapshot BEFORE editing. A ring of UN_N entries, oldest dropped. */
#define UN_N 8
typedef struct { uint8_t kind, trk, pat, nrows; uint8_t d[NROW * 5]; } UnEnt;
typedef struct { UnEnt e[UN_N]; uint8_t head, cnt; } Undo;
void un_init(Undo *u);
int un_push_pat(Undo *u, const Song *g, int trk, int pat);
int un_push_rows(Undo *u, const Song *g);
int un_pop(Undo *u, Song *g);                                 /* restore the newest snapshot; 0 if none */
/* MKS2 binary format (song_io.c) */
#define SONG_ENC_V1 (10 + 8 + 6 + NTRK * 8 + NROW * 5 + NTRK * NPAT * NSTEP * 4 + 4)   /* 4476: version 1, still read */
#define SONG_ENC_V2 (SONG_ENC_V1 + 16)                                                    /* 4492: version 2, still read */
#define SONG_ENC_V3 (SONG_ENC_V1 + 32)                                                    /* 4508: legacy voice block */
#define SG_DX7_FIRST 64u
#define SG_DX7_CUSTOM 72u
#define SONG_ENC_MAX (SONG_ENC_V3 + 256u) /* v5 DX7 snapshots; classic records retain their size */
#define SONG_ENC_SIZE SONG_ENC_V3                                                        /* 4508: v4, same layout; vocal flag semantics */
enum { SIO_OK = 0, SIO_SIZE = -1, SIO_MAGIC = -2, SIO_VERSION = -3, SIO_LEN = -4, SIO_CRC = -5, SIO_FIELD = -6 };
uint32_t sg_crc32(const uint8_t *p, uint32_t n);
uint32_t song_encode(const Song *g, uint8_t *buf, uint32_t cap);   /* bytes written, 0 if cap too small */
int song_decode(Song *g, const uint8_t *buf, uint32_t len);        /* SIO_OK or an error; g untouched on error. len: SONG_ENC_MAX (DX7 v5), SONG_ENC_SIZE (v4/v3), SONG_ENC_V2 or SONG_ENC_V1 */
void sg_voice_default(Voice *v);
void song_demo(Song *g);
#endif

/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
#include "firmware/song/phrase.h"

static Phrase phrase;
static Song song;
static Engine engine;
static uint8_t record[PH_REC], song_record[SONG_ENC_CAP];
static char input[PH_TEXT + 1], pronunciation[512];
static int32_t audio[512];
static int error_at;

uint8_t *ed_record(void) { return record; }
const uint8_t *ed_song(void) { return song_record; }
char *ed_input(void) { return input; }
char *ed_pronunciation(void) { return pronunciation; }
int ed_error_at(void) { return error_at; }

int ed_compile(unsigned lang, unsigned deflen)
{
    phrase.deflen = deflen;
    for (unsigned i = 0; i < sizeof phrase.name; i++) phrase.name[i] = ' ';
    phrase.bpm = 120;
    int rc = ph_compile(input, lang, &phrase, &error_at);
    return rc ? rc : ph_encode(&phrase, record);
}

int ed_roundtrip(void)
{
    return ph_decode(record, &phrase) || ph_encode(&phrase, record);
}

/* Both playback and upload start with the same fresh arrangement. */
int ed_place(unsigned character, unsigned reverb, int syllable)
{
    unsigned rows;
    if (ph_decode(record, &phrase) || !phrase.n) return -1;
    if (syllable >= 0) {
        if ((unsigned)syllable >= phrase.n) return -1;
        phrase.s[0] = phrase.s[syllable];
        phrase.n = 1;
        phrase.flags |= PH_MELODY;
    }
    sg_init(&song);
    if (ph_place(&song, 0, NPAT, &phrase, 60, &rows)) return -1;
    song.bpm = phrase.bpm;
    song.loop = 0;
    song.vx.chr = character;
    song.vx.rev = reverb;
    for (unsigned row = 0; row < rows; row++) {
        sg_row_insert(&song, row);
        sg_row_set(&song, row, TR_VOCAL, row);
    }
    return rows;
}

unsigned ed_encode_song(void)
{
    return song_encode(&song, song_record, sizeof song_record);
}

void ed_start(void)
{
    engine.qh = engine.qt = engine.lock = engine.ack = 0;
    en_init(&engine, &song);
    engine.pan[TR_VOCAL] = 64;
    en_post(&engine, EN_PLAY, 0, 0, 0);
}

const int32_t *ed_render(unsigned frames)
{
    if (frames > 256) return 0;
    for (unsigned i = 0; i < frames * 2; i++) audio[i] = 0;
    en_render(&engine, audio, frames);
    return audio;
}

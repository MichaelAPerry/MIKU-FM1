/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/english.h"
#include "firmware/song/lyric_edit.h"
#include "firmware/song/mora.h"
#include "firmware/song/phrase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void fix_crc(uint8_t rec[PH_REC])
{
    uint32_t crc = sg_crc32(rec, 1020);
    for (int i = 0; i < 4; i++) rec[1020 + i] = (uint8_t)(crc >> (8 * i));
}

static void records(const Phrase *p)
{
    Phrase q, before;
    uint8_t rec[PH_REC], other[PH_REC];
    CHECK(ph_encode(p, rec) == 0);
    CHECK(!memcmp(rec, "MPH1\1", 5));
    CHECK(!memcmp(rec + 160, p->s, 16u * p->n));
    CHECK(ph_decode(rec, &q) == 0 && !memcmp(p, &q, sizeof q));
    CHECK(ph_encode(&q, other) == 0 && !memcmp(rec, other, sizeof rec));
    memset(&q, 0xa5, sizeof q);
    before = q;
    for (int i = 0; i < PH_REC; i++) {
        rec[i] ^= 1;
        CHECK(ph_decode(rec, &q) < 0 && !memcmp(&q, &before, sizeof q));
        rec[i] ^= 1;
    }
    const struct { uint16_t at; uint8_t value; } bad[] = {
        {0, 0}, {4, 2}, {5, 2}, {6, 39}, {6, 241}, {7, 49}, {8, 0}, {8, 17},
        {9, 2}, {10, 1}, {12, 'a'}, {24, 1}, {32, 31}, {159, 'X'},
        {160, p->lang == PH_JP ? MORA_N : 41}, {168, 127}, {173, 'X'},
        {174, 128}, {928, 1}, {1019, 1}, {(uint16_t)(160 + 16 * p->n), 1}
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memcpy(other, rec, sizeof other);
        other[bad[i].at] = bad[i].value;
        fix_crc(other);
        CHECK(ph_decode(other, &q) < 0 && !memcmp(&q, &before, sizeof q));
    }
    q = *p;
    q.s[0].note = 128;
    memcpy(other, rec, sizeof other);
    CHECK(ph_encode(&q, other) < 0 && !memcmp(other, rec, sizeof rec));
}

static void compile_tests(void)
{
    Phrase p = {.name = "MIKU        ", .bpm = 120, .deflen = 2}, before;
    FvSyl v;
    char limit[100];
    int err;
    const char *words[] = {"Hel", "lo", "world", "I", "sing", "for", "you"};
    const uint8_t vowels[] = {FV_EP_AX, EP_OW, EP_ER, EP_AY, EP_IH, EP_AO, EP_UW};
    CHECK(ph_compile("Hello world, I sing for you", PH_EN, &p, &err) == 0 && err == -1);
    CHECK(p.n == sizeof vowels && p.bpm == 120 && p.deflen == 2 && !memcmp(p.name, "MIKU        ", 12));
    for (uint32_t i = 0; i < p.n; i++) {
        CHECK(!strcmp(p.s[i].spell, words[i]));
        CHECK(p.s[i].ph[3] == vowels[i]);
        CHECK(!!(p.s[i].len & PH_WORD_END) == (i != 0));
    }
    records(&p);
    CHECK(ph_compile("Miku", PH_EN, &p, &err) == 0 && p.n == 2);
    ph_fvsyl(&p.s[0], &v);
    CHECK(v.on[0] == EP_M && v.v == EP_IY && p.s[1].ph[0] == EP_K && p.s[1].ph[3] == EP_UW);
    for (int i = 0; i < 49; i++) { limit[2 * i] = 'a'; limit[2 * i + 1] = ' '; }
    limit[95] = 0;
    CHECK(ph_compile(limit, PH_EN, &p, &err) == 0 && p.n == 48);
    before = p;
    limit[95] = ' ';
    limit[97] = 0;
    CHECK(ph_compile(limit, PH_EN, &p, &err) < 0 && err == 96 && !memcmp(&p, &before, sizeof p));
    CHECK(ph_compile(limit, PH_JP, &p, &err) < 0 && err == 96 && !memcmp(&p, &before, sizeof p));
    CHECK(ph_compile("mi-ku:3 _:2 / a! | i", PH_JP, &p, &err) == 0 && p.n == 7);
    CHECK(p.s[0].ph[0] == mora_find("mi") && p.s[1].ph[0] == mora_find("ku"));
    CHECK(p.s[2].ph[0] == p.s[1].ph[0] && !strcmp(p.s[2].spell, "_"));
    CHECK(!p.s[3].ph[0] && !strcmp(p.s[3].spell, "/") &&
          !p.s[5].ph[0] && !strcmp(p.s[5].spell, "|"));
    records(&p);
}

static void placement(void)
{
    Phrase p = {.name = "MIKU        ", .bpm = 120, .deflen = 2};
    Song g, before;
    uint32_t used;
    sg_init(&g);
    g.pat[TR_VOCAL][2][0] = (Step){{73, 1, 0, 10}};
    g.pat[TR_VOCAL][2][5] = (Step){{80, 1, 0, 10}};
    CHECK(ph_compile("Miku", PH_EN, &p, 0) == 0);
    CHECK(ph_place(&g, 2, 1, &p, 60, &used) == 0 && used == 1);
    Step *s = g.pat[TR_VOCAL][2];
    CHECK(s[0].v[SV_NOTE] == 73 && s[0].v[SV_LEN] == 2 && s[0].v[SV_MORA] == SG_SYL0);
    CHECK(s[2].v[SV_NOTE] == 60 && s[2].v[SV_MORA] == SG_SYL0 + 1 && !s[5].v[SV_NOTE]);
    CHECK((s[2].v[SV_FLAG] & 0xf0) == (VF_WORD_END | VF_PHRASE_END));
    CHECK(!strcmp(sg_syl(&g, s[0].v[SV_MORA])->text, "Mi"));

    p.s[0].len = 16;
    p.s[0].note = 81;
    p.s[1].note = 82;
    g.pat[TR_VOCAL][3][0] = (Step){{75, 1, 0, 10}};
    before = g;
    CHECK(ph_place(&g, 2, 1, &p, 60, &used) < 0 && used == 1 && !memcmp(&g, &before, sizeof g));
    CHECK(ph_place(&g, 2, 2, &p, 60, &used) == 0 && used == 2);
    CHECK(s[0].v[SV_NOTE] == 73 && g.pat[TR_VOCAL][3][0].v[SV_NOTE] == 75);
    p.flags = PH_MELODY;
    CHECK(ph_place(&g, 2, 2, &p, 60, &used) == 0 && used == 2);
    CHECK(s[0].v[SV_NOTE] == 81 && g.pat[TR_VOCAL][3][0].v[SV_NOTE] == 82 && g.nsyl == 2);
    p.s[0].len = 3;
    CHECK(ph_place(&g, 2, 1, &p, 60, &used) == 0 && used == 1);
    CHECK(s[3].v[SV_NOTE] == 82 && g.pat[TR_VOCAL][3][0].v[SV_NOTE] == 82);

    CHECK(ph_compile("mi-ku:3 _:2 / a! | i", PH_JP, &p, 0) == 0);
    CHECK(ph_place(&g, 2, 1, &p, 60, &used) == 0 && used == 1);
    CHECK(s[0].v[SV_FLAG] & VF_LEGATO);
    CHECK(s[5].v[SV_FLAG] & VF_SUSTAIN);
    CHECK(ly_rest_step(&s[7]) && s[7].v[SV_LEN] == 2);
    CHECK(ly_rest_step(&s[11]) && (s[11].v[SV_FLAG] & VF_PHRASE_END));
    CHECK((s[13].v[SV_FLAG] & (VF_WORD_END | VF_PHRASE_END)) == (VF_WORD_END | VF_PHRASE_END));

    sg_init(&g);
    g.nsyl = SG_NSYL - 1;
    before = g;
    CHECK(ph_compile("Miku", PH_EN, &p, 0) == 0);
    CHECK(ph_place(&g, 0, 1, &p, 60, &used) < 0 && !memcmp(&g, &before, sizeof g));
    CHECK(ph_compile("I I", PH_EN, &p, 0) == 0);
    CHECK(ph_place(&g, 0, 1, &p, 60, &used) == 0 && g.nsyl == SG_NSYL);
    CHECK(ph_place(&g, 0, 1, &p, 60, &used) == 0 && g.nsyl == SG_NSYL);
}

int main(void)
{
    compile_tests();
    placement();
    return 0;
}

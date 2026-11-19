/* SPDX-License-Identifier: GPL-3.0-only */
/* MKS2 v6: the English syllable table (round trip, compaction, refusals, older versions unchanged). */
#include "firmware/song/song.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint8_t buf[SONG_ENC_CAP + 64], buf2[SONG_ENC_CAP + 64];
static Song a, b;
static void put(Song *g, int pat, int step, int note, int len, int mora) {
    Step s = {{(uint8_t)note, (uint8_t)len, (uint8_t)mora, 10}};
    CHECK(sg_set_step(g, TR_VOCAL, pat, step, &s), "set step %d", step);
}
int main(void) {
    static const uint8_t hel[8] = {22, 0, 0, 40, 0, 0, 0, 0}, lo[8] = {25, 0, 0, 12, 0, 0, 0, 0}, wld[8] = {36, 0, 0, 8, 25, 18, 0, 0};
    /* a song without English keeps writing v4 / v5 exactly as 0.8 did */
    sg_init(&a); song_demo(&a);
    uint32_t n = song_encode(&a, buf, sizeof buf);
    CHECK(n == SONG_ENC_SIZE || n == SONG_ENC_MAX, "no English: v4/v5 size (%u)", n);
    CHECK(buf[4] == 4 || buf[4] == 5, "no English: version %u", buf[4]);
    CHECK(song_decode(&b, buf, n) == SIO_OK && b.nsyl == 0, "no English: decodes");
    /* add: equal syllables are shared, the table fills up, then refuses */
    sg_init(&a);
    int m0 = sg_syl_add(&a, hel, "hel"), m1 = sg_syl_add(&a, lo, "lo"), m2 = sg_syl_add(&a, hel, "hel"), m3 = sg_syl_add(&a, wld, "world!!!long");
    CHECK(m0 == (int)SG_SYL0 && m1 == (int)SG_SYL0 + 1 && m2 == m0 && m3 == (int)SG_SYL0 + 2 && a.nsyl == 3, "add/reuse %d %d %d %d", m0, m1, m2, m3);
    CHECK(!strcmp(a.syl[2].text, "world!!"), "spelling truncated to 7 (%s)", a.syl[2].text);
    CHECK(sg_syl(&a, (uint32_t)m1) == &a.syl[1] && sg_syl(&a, 5) == 0 && sg_syl(&a, SG_SYL0 + 3) == 0, "lookup");
    put(&a, 0, 0, 64, 2, m0); put(&a, 0, 2, 67, 2, m1); put(&a, 0, 4, 69, 4, m3); put(&a, 1, 0, 60, 2, 12);   /* + a Japanese mora */
    a.row[0].pat[TR_VOCAL] = 0; a.nrows = 1;
    n = song_encode(&a, buf, sizeof buf);
    CHECK(n == SONG_ENC_V6(3) && buf[4] == 6, "v6 size %u version %u", n, buf[4]);
    memset(&b, 0x55, sizeof b);
    CHECK(song_decode(&b, buf, n) == SIO_OK, "v6 decodes");
    CHECK(b.nsyl == 3 && !memcmp(b.syl, a.syl, sizeof a.syl), "syllables round trip");
    CHECK(!memcmp(b.pat, a.pat, sizeof a.pat) && !memcmp(&b.vx, &a.vx, sizeof a.vx), "steps and voice round trip");
    CHECK(song_encode(&b, buf2, sizeof buf2) == n && !memcmp(buf, buf2, n), "re-encode is identical");
    /* refusals: dangling reference, bad phoneme, unprintable spelling, wrong length, step >= count, CRC */
    { Song c = a; c.nsyl = 2; CHECK(song_encode(&c, buf2, sizeof buf2) == 0, "encoder refuses a missing syllable"); }
    { memcpy(buf2, buf, n); buf2[SONG_ENC_MAX - 4] = 2; CHECK(song_decode(&b, buf2, n) != SIO_OK, "count/length mismatch refused"); }
    {   /* patch a field, then fix the CRC so only the field check can refuse it */
        uint32_t off[3] = {SONG_ENC_MAX - 3 + 3, SONG_ENC_MAX - 3 + 8, SONG_ENC_MAX - 3 + 16 * 2 + 8 + 7};
        uint8_t val[3] = {41, 7, 'x'};
        for (int k = 0; k < 3; k++) {
            memcpy(buf2, buf, n); buf2[off[k]] = val[k];
            uint32_t c = sg_crc32(buf2, n - 4);
            buf2[n - 4] = (uint8_t)c; buf2[n - 3] = (uint8_t)(c >> 8); buf2[n - 2] = (uint8_t)(c >> 16); buf2[n - 1] = (uint8_t)(c >> 24);
            Song keep; memcpy(&keep, &b, sizeof b);
            CHECK(song_decode(&b, buf2, n) == SIO_FIELD && !memcmp(&keep, &b, sizeof b), "field refusal %d leaves the song untouched", k);
        }
    }
    { memcpy(buf2, buf, n); buf2[n - 1] ^= 1; CHECK(song_decode(&b, buf2, n) == SIO_CRC, "CRC"); }
    { memcpy(buf2, buf, n); buf2[4] = 5; CHECK(song_decode(&b, buf2, n) != SIO_OK, "an English length with version 5 refused"); }
    /* compaction: unused syllables are dropped and steps renumbered */
    {
        Song c = a;
        int extra = sg_syl_add(&c, lo, "LO2");
        CHECK(extra == (int)SG_SYL0 + 3, "extra");
        sg_clear_step(&c, TR_VOCAL, 0, 2);                 /* "lo" (index 1) now unused */
        put(&c, 0, 8, 70, 2, extra);
        sg_syl_compact(&c);
        CHECK(c.nsyl == 3 && !strcmp(c.syl[1].text, "world!!") && !strcmp(c.syl[2].text, "LO2"), "compacted (%u)", c.nsyl);
        CHECK(c.pat[TR_VOCAL][0][4].v[SV_MORA] == SG_SYL0 + 1 && c.pat[TR_VOCAL][0][8].v[SV_MORA] == SG_SYL0 + 2 &&
              c.pat[TR_VOCAL][1][0].v[SV_MORA] == 12, "renumbered, Japanese untouched");
        sg_clear_step(&c, TR_VOCAL, 0, 0); sg_clear_step(&c, TR_VOCAL, 0, 4); sg_clear_step(&c, TR_VOCAL, 0, 8);
        sg_syl_compact(&c);
        CHECK(c.nsyl == 0, "all unused: empty");
        n = song_encode(&c, buf2, sizeof buf2);
        CHECK(buf2[4] == 4 && n == SONG_ENC_SIZE, "no English left: back to v4");
    }
    /* a full table */
    {
        Song c; sg_init(&c);
        uint8_t ph[8] = {0};
        int last = 0;
        for (int i = 0; i < (int)SG_NSYL; i++) { ph[0] = (uint8_t)(16 + i % 24); ph[3] = (uint8_t)(1 + i / 24); last = sg_syl_add(&c, ph, "x"); }
        CHECK(last == (int)(SG_SYL0 + SG_NSYL - 1) && c.nsyl == SG_NSYL, "fills");
        ph[4] = 31; CHECK(sg_syl_add(&c, ph, "y") == -1, "full: refused");
        put(&c, 0, 0, 60, 1, last); c.row[0].pat[TR_VOCAL] = 0; c.nrows = 1;
        n = song_encode(&c, buf, sizeof buf);
        CHECK(n == SONG_ENC_CAP, "largest record %u", n);
        CHECK(song_decode(&b, buf, n) == SIO_OK && b.nsyl == SG_NSYL, "largest decodes");
    }
    printf(fails ? "song_english: %d FAILED\n" : "song_english: ok\n", fails);
    return fails != 0;
}

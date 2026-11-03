/* SPDX-License-Identifier: GPL-3.0-only */
/* Step 4: generator (firmware/song/gen.c). Host test. */
#include "firmware/song/gen.h"
#include "firmware/song/mora.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const int scl[2][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 } };
static int in_key(int note, int key, int sc) { int pc = ((note - key) % 12 + 12) % 12, i; for (i = 0; i < 7; i++) if (scl[sc][i] == pc) return 1; return 0; }
static int triad(const Step *c, int pc) {   /* pitch class pc belongs to the chord (root, third, fifth; sevenths too) */
    static const int iv[10][4] = { { 0, 4, 7, -1 }, { 0, 3, 7, -1 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 5, 7, -1 }, { 0, 2, 7, -1 }, { 0, 3, 6, -1 }, { 0, 4, 8, -1 }, { 0, 4, 7, 2 } };
    int q = c->v[1] & 15, i;
    for (i = 0; i < 4; i++) if (iv[q][i] >= 0 && (c->v[0] + iv[q][i]) % 12 == pc) return 1;
    return 0;
}
static const Step *first(const Song *g, int t, int p) { int i; for (i = 0; i < NSTEP; i++) if (g->pat[t][p][i].v[0]) return &g->pat[t][p][i]; return 0; }
static uint8_t A[SONG_ENC_SIZE], B[SONG_ENC_SIZE];
static Song g, h;
static void check_song(const Song *s, const GenCfg *c, int *strong, int *strong_ok, int *lo, int *hi) {
    int r, i, t;
    CHECK(song_encode(s, A, sizeof A) == SONG_ENC_SIZE && song_decode(&h, A, sizeof A) == SIO_OK, "generated song does not round-trip");
    for (r = 0; r < s->nrows; r++) {
        int pc = s->row[r].pat[TR_CHORDS], pv = s->row[r].pat[TR_VOCAL], pb = s->row[r].pat[TR_BASS];
        const Step *ch = pc < NPAT ? first(s, TR_CHORDS, pc) : 0;
        for (t = 0; t < NTRK; t++) CHECK(s->row[r].pat[t] < NPAT || s->row[r].pat[t] == ROW_OFF, "row %d bad pattern", r);
        CHECK(ch != 0, "row %d has no chord", r);
        if (!ch) continue;
        CHECK(in_key(ch->v[0], c->key, c->scale), "row %d chord root %d not in key", r, ch->v[0]);
        for (i = 0; i < NSTEP; i++) {
            const Step *v = pv < NPAT ? &s->pat[TR_VOCAL][pv][i] : 0, *b = pb < NPAT ? &s->pat[TR_BASS][pb][i] : 0;
            if (v && v->v[0]) {
                CHECK(in_key(v->v[0], c->key, c->scale), "row %d step %d melody %d not in key", r, i, v->v[0]);
                if (v->v[0] < *lo) *lo = v->v[0];
                if (v->v[0] > *hi) *hi = v->v[0];
                CHECK(v->v[2] > 0 && v->v[2] < MORA_N, "row %d step %d no mora", r, i);
                if (i == 0 || i == 8) { (*strong)++; if (triad(ch, v->v[0] % 12)) (*strong_ok)++; }
            }
            if (b && b->v[0]) CHECK(b->v[0] % 12 == ch->v[0] % 12 || (b->v[0] + 5) % 12 == ch->v[0] % 12, "row %d bass %d is not root or fifth of %d", r, b->v[0], ch->v[0]);
        }
        if (s->row[r].sect == SEC_INTRO || s->row[r].sect == SEC_OUTRO) CHECK(pv == ROW_OFF, "vocal sings in the intro/outro");
    }
}
static void t_all(void) {
    int st, k, sc, sd, strong = 0, ok = 0, lo = 127, hi = 0, n = 0;
    for (st = 0; st < GS_N; st++) for (sc = 0; sc < 2; sc++) for (k = 0; k < 12; k++) for (sd = 0; sd < 3; sd++) {
        GenCfg c = { (uint8_t)st, (uint8_t)k, (uint8_t)sc, 0, (uint16_t)(sd * 7919 + k) };
        sg_init(&g); gen_song(&g, &c);
        CHECK(g.nrows == 20 && g.row[0].sect == SEC_INTRO && g.row[19].sect == SEC_OUTRO && g.row[6].sect == SEC_CHORUS, "arrangement");
        CHECK(g.loop == 1 && g.bpm >= 80 && g.bpm <= 160, "tempo %d", g.bpm);
        check_song(&g, &c, &strong, &ok, &lo, &hi);
        n++;
    }
    printf("gen: %d songs (4 styles x 2 scales x 12 keys x 3 seeds): melody %d..%d, beats 1 and 3 on a chord tone %d of %d\n", n, lo, hi, ok, strong);
    CHECK(ok == strong, "melody off the chord on a strong beat (%d of %d)", strong - ok, strong);
    CHECK(lo >= 48 && hi <= 88, "melody range %d..%d", lo, hi);
}
static void t_det(void) {
    GenCfg c = { GS_POP, 0, GK_MAJOR, 0, 1234 }, d = c;
    uint32_t i, diff = 0;
    sg_init(&g); gen_song(&g, &c); h = g;
    sg_init(&g); song_demo(&g); gen_song(&g, &c);   /* what was there before does not matter (except the mix, which stays) */
    CHECK(!memcmp(g.row, h.row, sizeof g.row[0] * h.nrows) && !memcmp(g.pat, h.pat, sizeof g.pat) && !memcmp(g.name, h.name, 8) && g.nrows == h.nrows && g.bpm == h.bpm, "not deterministic");
    CHECK(g.trk[TR_CHORDS].level == 112, "the mix was reset");
    sg_init(&g); gen_song(&g, &c); song_encode(&g, A, sizeof A);
    d.seed = 1235; sg_init(&g); gen_song(&g, &d); song_encode(&g, B, sizeof B);
    for (i = 0; i < sizeof A; i++) diff += A[i] != B[i];
    CHECK(diff > 100, "seed 1234 and 1235 differ in only %u bytes", diff);
    CHECK(!memcmp(g.name, "POP 1235", 8), "name %.8s", g.name);
}
static void t_lock(void) {
    GenCfg c = { GS_BALLAD, 2, GK_MINOR, 0, 99 };
    int t, r, strong = 0, ok = 0, lo = 127, hi = 0;
    for (t = 0; t < NTRK; t++) {
        sg_init(&g); song_demo(&g); h = g;
        c.lock = (uint8_t)(1u << t); c.key = 0; c.scale = GK_MAJOR;     /* the demo is in C major */
        gen_song(&g, &c);
        CHECK(g.nrows == h.nrows && !memcmp(g.name, h.name, 8) && g.bpm == h.bpm, "lock %d: arrangement or song settings changed", t);
        CHECK(!memcmp(g.pat[t], h.pat[t], sizeof g.pat[t]), "lock %d: locked patterns changed", t);
        for (r = 0; r < g.nrows; r++) CHECK(g.row[r].pat[t] == h.row[r].pat[t] && g.row[r].sect == h.row[r].sect, "lock %d: row %d changed", t, r);
        for (r = 0; r < NTRK; r++) if (r != t) CHECK(memcmp(g.pat[r], h.pat[r], sizeof g.pat[r]), "lock %d: track %d not regenerated", t, r);
        if (t == TR_CHORDS) { int s0 = strong; check_song(&g, &c, &strong, &ok, &lo, &hi); CHECK(strong > s0, "no melody over the locked chords"); }
    }
    printf("gen: locked demo chords, new melody: beats 1 and 3 on a chord tone %d of %d\n", ok, strong);
    CHECK(ok == strong, "melody does not fit locked chords");
    sg_init(&g); song_demo(&g); h = g; c.lock = 15; gen_song(&g, &c);
    CHECK(!memcmp(&g, &h, sizeof g), "everything locked, but the song changed");
}
static void t_phrase(void) {
    GenCfg c = { GS_DANCE, 9, GK_MINOR, 0, 4242 };
    int t, p, q, r = 6;
    for (t = 0; t < NTRK; t++) {
        sg_init(&g); gen_song(&g, &c); h = g;
        p = g.row[r].pat[t];
        CHECK(gen_phrase(&g, &c, r, t, 0) && !memcmp(&g, &h, sizeof g), "track %d: variation 0 is not what gen_song wrote", t);
        { int v, nd = 0; Song o = g;
          for (v = 1; v <= 3; v++) { g = o; CHECK(gen_phrase(&g, &c, r, t, (uint32_t)v), "track %d refused", t); nd += memcmp(g.pat[t][p], h.pat[t][p], sizeof g.pat[t][p]) != 0; }
          CHECK(nd > 0, "track %d: variations 1-3 all equal 0", t); }
        for (q = 0; q < NPAT; q++) if (q != p) CHECK(!memcmp(g.pat[t][q], h.pat[t][q], sizeof g.pat[t][q]), "track %d: pattern %d changed too", t, q);
        for (q = 0; q < NTRK; q++) if (q != t) CHECK(!memcmp(g.pat[q], h.pat[q], sizeof g.pat[q]), "track %d: track %d changed", t, q);
        CHECK(!memcmp(g.row, h.row, sizeof g.row), "track %d: rows changed", t);
        h = g; gen_phrase(&g, &c, r, t, 1); h = g; gen_phrase(&g, &c, r, t, 1);
        CHECK(!memcmp(&g, &h, sizeof g), "track %d: variation 1 not reproducible", t);
    }
    sg_init(&g); gen_song(&g, &c);
    CHECK(gen_phrase(&g, &c, 0, TR_VOCAL, 3) && g.row[0].pat[TR_VOCAL] < NPAT && first(&g, TR_VOCAL, g.row[0].pat[TR_VOCAL]), "a silent row gets a free pattern");
    c.lock = 1u << TR_BASS; h = g;
    CHECK(!gen_phrase(&g, &c, r, TR_BASS, 2) && !memcmp(&g, &h, sizeof g), "locked track rewritten");
    CHECK(!gen_phrase(&g, &c, g.nrows, TR_VOCAL, 2), "row past the end accepted");
}
int main(void) {
    t_all(); t_det(); t_lock(); t_phrase();
    if (fails) { printf("%d FAILED\n", fails); return 1; }
    printf("gen tests passed\n"); return 0;
}

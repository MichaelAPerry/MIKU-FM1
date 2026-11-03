/* SPDX-License-Identifier: GPL-3.0-only */
/* Step 4: song generator (see gen.h). Chords follow a progression per section (diatonic, from tables); bass and melody are
 * written against the chord of each row as it stands in the song, so they also fit a locked or hand-written CHORDS track.
 * The melody walks in scale steps and lands on chord tones on beats 1 and 3, ends a section on the chord root, and repeats
 * its rhythm every two bars. Every pattern is seeded from (seed, track, its musical context, variation): equal contexts give
 * equal patterns, and the arrangement reuses them. Private names start with gn_ (the device build is one translation unit). */
#include "firmware/song/gen.h"
#include "firmware/song/mora.h"
static const uint8_t gn_scl[2][7] = { { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 8, 10 } };
static const uint8_t gn_tri[2][7] = { { CQ_MAJ, CQ_MIN, CQ_MIN, CQ_MAJ, CQ_MAJ, CQ_MIN, CQ_DIM }, { CQ_MIN, CQ_DIM, CQ_MAJ, CQ_MIN, CQ_MIN, CQ_MAJ, CQ_MAJ } };
static const uint8_t gn_prog[2][6][4] = {   /* scale degrees, one chord per bar; no diminished chords */
    { { 0, 4, 5, 3 }, { 5, 3, 0, 4 }, { 0, 5, 3, 4 }, { 3, 4, 2, 5 }, { 0, 3, 4, 4 }, { 3, 4, 0, 0 } },
    { { 0, 5, 6, 0 }, { 0, 3, 6, 2 }, { 5, 6, 0, 0 }, { 0, 6, 5, 6 }, { 0, 3, 4, 0 }, { 5, 3, 0, 6 } } };
static const uint8_t gn_alt[2][6] = { { 0, 1, 2, 3, 4, 5 }, { 0, 2, 3, 4, 5, 6 } };   /* chords a phrase variation may pick */
static const char *const gn_snm[GS_N] = { "POP", "BALLAD", "DANCE", "CHIP" };
static const char gn_sab[GS_N][4] = { "POP", "BAL", "DNC", "CHP" };
static const uint8_t gn_bpm[GS_N] = { 128, 84, 136, 150 };
static const uint8_t gn_pre[GS_N][3] = { { 0, 0, 0 }, { 1, 1, 0 }, { 5, 3, 0 }, { 2, 2, 1 } };   /* CHORDS, BASS, DRUMS preset */
static const uint16_t gn_vr[GS_N][4] = {    /* melody rhythms: onset masks over the 16 steps */
    { 0x5555, 0x1555, 0x1549, 0x1515 }, { 0x1111, 0x1151, 0x1515, 0x0555 }, { 0x5555, 0x1549, 0x5554, 0x1515 }, { 0x5757, 0x5555, 0x1555, 0x5575 } };
static const uint16_t gn_dr[GS_N][3][5] = { /* [style][verse, chorus, intro/outro]{kick, snare, clap, closed hat, open hat} */
    { { 0x0141, 0x1010, 0, 0x5555, 0 }, { 0x4141, 0x1010, 0x1010, 0x1555, 0x4000 }, { 0x0101, 0, 0, 0x5555, 0 } },
    { { 0x0401, 0x0100, 0, 0x1111, 0 }, { 0x0C01, 0x0100, 0x0100, 0x5555, 0 }, { 0x0001, 0, 0, 0x1111, 0 } },
    { { 0x1111, 0, 0x1010, 0, 0x4444 }, { 0x1111, 0x1010, 0x1010, 0xAAAA, 0x4444 }, { 0x1111, 0, 0, 0x4444, 0 } },
    { { 0x0101, 0x1010, 0, 0x5555, 0 }, { 0x0505, 0x1010, 0, 0xFFFF, 0 }, { 0x0101, 0, 0, 0x5555, 0 } } };
static const char *const gn_ly[] = { "a", "i", "u", "e", "o", "ka", "ki", "ku", "ko", "sa", "shi", "su", "so", "ta", "te", "to", "na", "ni",
    "no", "ha", "hi", "ma", "mi", "mo", "ya", "yu", "yo", "ra", "ri", "ru", "re", "ro", "wa", "ga", "de", "do", "n" };
static const char *const gn_lo[] = { "a", "o", "e", "ka", "sa", "ta", "na", "ra", "ya", "wa", "ko", "to", "no", "mo", "yo", "ro" };   /* long notes */
#define GN_NLY (sizeof gn_ly / sizeof gn_ly[0])
#define GN_NLO (sizeof gn_lo / sizeof gn_lo[0])
typedef struct { int sect, bar, last, deg, root; } GnCtx;   /* section, bar in it, last bar?, chord degree 0..6, chord root pitch class */
static uint32_t gn_r;
static uint32_t gn_rnd(uint32_t n) { gn_r ^= gn_r << 13; gn_r ^= gn_r >> 17; gn_r ^= gn_r << 5; return ((gn_r >> 16) * n) >> 16; }
static void gn_seed(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t x = a * 0x9E3779B1u ^ b * 0x85EBCA6Bu ^ c * 0xC2B2AE35u ^ d * 0x27D4EB2Fu;
    x ^= x >> 15; x *= 0x2C1B3C6Du; x ^= x >> 12; x *= 0x297A2D39u; x ^= x >> 15;
    gn_r = x ? x : 0x6D2B79F5u;
}
static void gn_put(Song *g, int t, int p, int s, int a, int b, int c, int d) {
    Step x;
    x.v[0] = (uint8_t)a; x.v[1] = (uint8_t)b; x.v[2] = (uint8_t)c; x.v[3] = (uint8_t)d;
    sg_set_step(g, t, p, s, &x);
}
const char *gen_style(uint32_t s) { return s < GS_N ? gn_snm[s] : ""; }
static int gn_sect(const Song *g, int r) { int s = g->row[r].sect; return s == SEC_NONE ? SEC_VERSE : s; }
static void gn_pos(const Song *g, int r, GnCtx *x) {
    int b = 0, s = gn_sect(g, r);
    while (r - b - 1 >= 0 && gn_sect(g, r - b - 1) == s) b++;
    x->sect = s; x->bar = b; x->last = r + 1 >= g->nrows || gn_sect(g, r + 1) != s;
}
static int gn_pdeg(const GenCfg *c, const GnCtx *x) {   /* the chord degree of the song's progression at this position */
    uint32_t v, k, b;
    if (x->sect == SEC_OUTRO && x->last) return 0;
    gn_seed(c->seed, 77, c->style, c->scale);
    v = gn_rnd(6); k = (v + 1 + gn_rnd(5)) % 6; b = (k + 1 + gn_rnd(4)) % 6;
    if (b == v) b = (b + 1) % 6;
    return gn_prog[c->scale][x->sect == SEC_CHORUS || x->sect == SEC_OUTRO ? k : x->sect == SEC_BRIDGE ? b : v][x->bar & 3];
}
static void gn_chord(const Song *g, const GenCfg *c, int r, GnCtx *x) {   /* chord of row r as it is in the song (else the last one before) */
    int pc = -1, i, best = 0, bd = 99;
    for (; r >= 0 && pc < 0; r--) {
        uint32_t p = g->row[r].pat[TR_CHORDS];
        if (p < NPAT) for (i = 0; i < NSTEP; i++) if (g->pat[TR_CHORDS][p][i].v[0]) { pc = g->pat[TR_CHORDS][p][i].v[0] % 12; break; }
    }
    if (pc < 0) pc = c->key;
    for (i = 0; i < 7; i++) {
        int d = (pc + 24 - c->key - gn_scl[c->scale][i]) % 12;
        if (12 - d < d) d = 12 - d;
        if (d < bd) { bd = d; best = i; }
    }
    x->root = pc; x->deg = best;
}
static uint32_t gn_key(const Song *g, const GenCfg *c, int t, int r) {   /* rows with equal keys share a pattern */
    GnCtx x;
    gn_pos(g, r, &x);
    if (t == TR_DRUMS) return (uint32_t)(x.sect * 2 + x.last);
    if (t == TR_CHORDS) { x.deg = gn_pdeg(c, &x); return (uint32_t)((x.sect * 8 + x.deg) * 2 + x.last); }
    gn_chord(g, c, r, &x);
    if (t == TR_BASS) return (uint32_t)((x.sect * 16 + x.root) * 2 + x.last);
    return (uint32_t)(((x.sect * 4 + (x.bar & 3)) * 8 + x.deg) * 2 + x.last);
}
static void gn_wchords(Song *g, const GenCfg *c, int p, const GnCtx *x) {
    int d = x->deg, q = gn_tri[c->scale][d], root = 48 + (c->key + gn_scl[c->scale][d]) % 12, ch = x->sect == SEC_CHORUS, i;
    if (c->style == GS_BALLAD) q = q == CQ_MAJ ? (d == 4 && !c->scale ? CQ_7 : CQ_MAJ7) : q == CQ_MIN ? CQ_MIN7 : q;
    if (x->sect == SEC_OUTRO && x->last) { gn_put(g, TR_CHORDS, p, 0, root, CM_STRUM << 4 | q, 16, 11); return; }
    switch (c->style) {
    case GS_POP:
        if (!ch) { gn_put(g, TR_CHORDS, p, 0, root, CM_UPDOWN << 4 | q, 16, 11); break; }
        gn_put(g, TR_CHORDS, p, 0, root, CM_BLOCK << 4 | q, 2, 11); gn_put(g, TR_CHORDS, p, 2, root, CM_UPDOWN << 4 | q, 6, 11);
        gn_put(g, TR_CHORDS, p, 8, root, CM_BLOCK << 4 | q, 2, 11); gn_put(g, TR_CHORDS, p, 10, root, CM_UP << 4 | q, 6, 11);
        break;
    case GS_BALLAD: gn_put(g, TR_CHORDS, p, 0, root, (ch ? CM_BLOCK : CM_UP) << 4 | q, 16, 10); break;
    case GS_DANCE: for (i = 0; i < 16; i++) if ((ch ? 0x4949 : 0x4444) >> i & 1) gn_put(g, TR_CHORDS, p, i, root, CM_BLOCK << 4 | q, 2, 11); break;
    default: gn_put(g, TR_CHORDS, p, 0, root, (ch ? CM_UPDOWN32 : CM_UP32) << 4 | q, 16, 10); break;
    }
}
static void gn_wbass(Song *g, const GenCfg *c, int p, const GnCtx *x) {
    static const uint8_t po[3][8] = { { 0, 0, 12, 0, 0, 0, 12, 0 }, { 0, 0, 0, 12, 0, 0, 7, 12 }, { 0, 12, 0, 0, 0, 12, 0, 7 } };
    static const uint8_t ov[4][4] = { { 0, 12, 0, 12 }, { 0, 12, 7, 12 }, { 0, 7, 12, 7 }, { 0, 0, 12, 7 } };   /* dance offbeats, chip arpeggio */
    int n = 36 + x->root, sl = x->last && x->sect != SEC_OUTRO ? BF_SLIDE : 0, i, v = (int)gn_rnd(4);
    const uint8_t *o = ov[v], *q = po[v % 3];
    if (x->sect == SEC_OUTRO && x->last) { gn_put(g, TR_BASS, p, 0, n, 16, BF_ACCENT, 13); return; }
    switch (c->style) {
    case GS_POP:
        for (i = 0; i < 8; i++) {
            int a = q[i];
            if ((i == 3 || i == 5) && gn_rnd(10) < 3) a = 7;
            gn_put(g, TR_BASS, p, i * 2, n + a, 2, (i % 4 == 0 ? BF_ACCENT : 0) | (i == 7 ? sl : 0), 12);
        }
        break;
    case GS_BALLAD:
        gn_put(g, TR_BASS, p, 0, n, v & 1 ? 6 : 8, BF_ACCENT, 12);
        if (v & 1) gn_put(g, TR_BASS, p, 6, n + 7, 2, 0, 10);
        gn_put(g, TR_BASS, p, 8, n + (v & 2 ? 7 : 0), 8, sl, 11);
        break;
    case GS_DANCE: for (i = 0; i < 4; i++) gn_put(g, TR_BASS, p, 2 + 4 * i, n + o[i], 2, (i & 1 ? 0 : BF_ACCENT) | (i == 3 ? sl : 0), 12); break;
    default: for (i = 0; i < 16; i++) gn_put(g, TR_BASS, p, i, n + o[i & 3], 1, (i % 4 == 0 ? BF_ACCENT : 0) | (i == 15 ? sl : 0), 11); break;
    }
}
static void gn_wdrums(Song *g, const GenCfg *c, int p, const GnCtx *x) {
    int k = x->sect == SEC_CHORUS ? 1 : x->sect == SEC_INTRO || x->sect == SEC_OUTRO ? 2 : 0, i, s;
    uint32_t m[6];
    for (i = 0; i < 5; i++) m[i] = gn_dr[c->style][k][i];
    m[5] = c->style == GS_CHIP || gn_rnd(3) == 0 ? 1u << (2 + 4 * gn_rnd(4)) : 0;   /* a perc hit */
    if (gn_rnd(3) == 0) m[0] |= 1u << (gn_rnd(2) ? 10 : 7);                         /* a pushed kick */
    if (x->sect == SEC_OUTRO && x->last) { m[0] = 1; m[1] = m[2] = m[3] = m[5] = 0; m[4] = 1; }
    else if (x->last && k != 2) { m[1] |= 0xD000; m[3] &= 0x0FFF; m[4] &= 0x0FFF; }  /* fill into the next section */
    for (s = 0; s < 16; s++) {
        int v = 0, a = 0;
        for (i = 0; i < 6; i++) if (m[i] >> s & 1) v |= 1 << i;
        if (v & D_SNARE) a |= D_SNARE;
        if ((v & D_KICK) && s % 8 == 0) a |= D_KICK;
        if (v) gn_put(g, TR_DRUMS, p, s, v, a, 0, 0);
    }
}
static int gn_ct(int e, int cd) { int r = ((e - cd) % 7 + 14) % 7; return r == 0 || r == 2 || r == 4; }
static int gn_near(int e, int cd) {   /* nearest chord tone (scale degrees) */
    int k;
    for (k = 0; k < 4; k++) { if (gn_ct(e - k, cd)) return e - k; if (gn_ct(e + k, cd)) return e + k; }
    return e;
}
static void gn_wvocal(Song *g, const GenCfg *c, int p, const GnCtx *x, uint32_t var) {
    static const int8_t stp[8] = { -2, -1, -1, 0, 0, 1, 1, 2 };
    int ch = x->sect == SEC_CHORUS, lo = ch ? 2 : 0, hi = ch ? 9 : 7, t = c->key <= 5 ? 60 + c->key : 48 + c->key;
    int on[16], n = 0, i, d, s;
    uint32_t m;
    gn_seed(c->seed, 11, (uint32_t)(x->sect * 4 + (x->bar & 1)), var);   /* rhythm: bars 1/3 and 2/4 of a section share it */
    m = gn_vr[c->style][gn_rnd(4)];
    if (x->last) m = (m & 0x03FFu) | 1u;
    for (s = 0; s < 16; s++) if (m >> s & 1) on[n++] = s;
    gn_seed(c->seed, 13, (uint32_t)((x->sect * 4 + (x->bar & 3)) * 8 + x->deg), var);
    d = gn_near(ch ? 5 : 3, x->deg);
    for (i = 0; i < n; i++) {
        int len = (i + 1 < n ? on[i + 1] : 16) - on[i], note;
        const char *ly;
        if (i) d += stp[gn_rnd(8)];
        if (d < lo) d = 2 * lo - d;
        if (d > hi) d = 2 * hi - d;
        if (on[i] == 0 || on[i] == 8) d = gn_near(d, x->deg);
        if (x->last && i == n - 1) { int e = ((d - x->deg) % 7 + 14) % 7; d += e <= 3 ? -e : 7 - e; }   /* end on the root */
        if (!x->last && len > 6) len = 6;
        note = t + 12 * ((d + 70) / 7 - 10) + gn_scl[c->scale][(d + 70) % 7];
        if (len >= 4) ly = gn_lo[gn_rnd(GN_NLO)];
        else { uint32_t j = gn_rnd(GN_NLY); if (!i && j == GN_NLY - 1) j = 0; ly = gn_ly[j]; }
        gn_put(g, TR_VOCAL, p, on[i], note, len, (int)mora_find(ly), ch ? 13 : 12);
    }
}
static void gn_write(Song *g, const GenCfg *c, int t, int p, int r, uint32_t key, uint32_t var) {
    GnCtx x;
    gn_pos(g, r, &x);
    sg_clear_pat(g, t, p);
    if (t == TR_CHORDS) {
        x.deg = gn_pdeg(c, &x);
        if (var) { int d; gn_seed(c->seed, 17, (uint32_t)r, var); do d = gn_alt[c->scale][gn_rnd(6)]; while (d == x.deg); x.deg = d; }
        gn_wchords(g, c, p, &x);
        return;
    }
    gn_chord(g, c, r, &x);
    gn_seed(c->seed, (uint32_t)t + 1u, key, var);
    if (t == TR_VOCAL) gn_wvocal(g, c, p, &x, var);
    else if (t == TR_BASS) gn_wbass(g, c, p, &x);
    else gn_wdrums(g, c, p, &x);
}
static void gn_norm(const GenCfg *c, GenCfg *o) {
    *o = *c;
    if (o->style >= GS_N) o->style = GS_POP;
    o->key %= 12; o->scale &= 1; o->lock &= 15;
}
void gen_song(Song *g, const GenCfg *cfg) {
    static const uint8_t ar[6][2] = { { SEC_INTRO, 2 }, { SEC_VERSE, 4 }, { SEC_CHORUS, 4 }, { SEC_VERSE, 4 }, { SEC_CHORUS, 4 }, { SEC_OUTRO, 2 } };
    static const uint8_t ord[NTRK] = { TR_CHORDS, TR_BASS, TR_VOCAL, TR_DRUMS };   /* chords first: the others read them */
    GenCfg c;
    int i, j, t, r;
    gn_norm(cfg, &c);
    if (!c.lock || !g->nrows) {
        uint32_t sd = c.seed % 10000u;
        g->nrows = 0;
        for (i = 0; i < 6; i++) for (j = 0; j < ar[i][1]; j++) { sg_row_insert(g, g->nrows); sg_row_sect(g, g->nrows - 1, ar[i][0]); }
        for (i = 0; i < 3; i++) g->name[i] = gn_sab[c.style][i];
        g->name[3] = ' ';
        for (i = 7; i >= 4; i--) { g->name[i] = (char)('0' + sd % 10u); sd /= 10u; }
        g->bpm = gn_bpm[c.style]; g->swing = 0; g->loop = 1; g->transpose = 0;
    }
    for (i = 0; i < NTRK; i++) {
        uint32_t keys[NPAT];
        int nk = 0;
        t = ord[i];
        if (c.lock >> t & 1) continue;
        if (t != TR_VOCAL) g->trk[t].preset = gn_pre[c.style][t - 1];
        for (j = 0; j < NPAT; j++) sg_clear_pat(g, t, j);
        for (r = 0; r < g->nrows; r++) {
            int s = gn_sect(g, r);
            uint32_t k;
            if (t == TR_VOCAL && (s == SEC_INTRO || s == SEC_OUTRO)) { sg_row_set(g, r, t, ROW_OFF); continue; }
            k = gn_key(g, &c, t, r);
            for (j = 0; j < nk && keys[j] != k; j++) {}
            if (j == nk) {
                if (nk < NPAT) { keys[nk++] = k; gn_write(g, &c, t, j, r, k, 0); }
                else j = (int)(k % NPAT);      /* more than 16 different contexts: reuse a pattern */
            }
            sg_row_set(g, r, t, j);
        }
    }
}
int gen_phrase(Song *g, const GenCfg *cfg, int row, int trk, uint32_t var) {
    GenCfg c;
    uint32_t p;
    gn_norm(cfg, &c);
    if (row < 0 || row >= g->nrows || trk < 0 || trk >= NTRK || (c.lock >> trk & 1)) return 0;
    p = g->row[row].pat[trk];
    if (p >= NPAT) {
        int f = sg_find_free(g, trk);
        if (f < 0) return 0;
        sg_row_set(g, row, trk, f);
        p = (uint32_t)f;
    }
    gn_write(g, &c, trk, (int)p, row, gn_key(g, &c, trk, row), var);
    return 1;
}

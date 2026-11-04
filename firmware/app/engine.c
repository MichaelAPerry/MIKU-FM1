/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
static const uint8_t npre[NTRK] = { 1, CH_NPRESET, BS_NPRESET, DR_NPRESET };
/* languages: ANIMAL babbles random morae (ka..wo, ga..po) every 70-90 ms with small pitch jumps, like animal-game voices;
   SCAT sings da ba bi do pa ra ri de be on eighth notes; both keep the held note's envelope (legato articulation) */
static const uint8_t scat[9] = { 57, 60, 61, 59, 65, 39, 40, 58, 63 };   /* mora indices of da ba bi do pa ra ri de be */
static const int8_t ajit[8] = { 0, 0, 2, 3, 5, -2, 7, 4 };
static uint32_t rnd(Engine *e) { uint32_t x = e->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; e->rng = x; return x; }
uint32_t en_pre(uint32_t t, uint32_t p) { return p < npre[t] ? p : 0; }
void en_init(Engine *e, const Song *g) {
    uint32_t t;
    e->g = g;
    sq_init(&e->sq); fv_init(&e->fv); ch_init(&e->ch); bs_init(&e->bs); dr_init(&e->dr); vx_init();
    e->bt = 0; e->rng = 0x6D2B79F5u; e->vnote = 60; e->vvel = 110; e->vmora = 0;
    e->fv.vib_rate = 55; e->fv.vib_delay = 200;
    for (t = 0; t < NTRK; t++) e->pre[t] = 0;
    e->vh = 0; e->vl = e->vr = 0; e->up = 1;
}
uint32_t en_post(Engine *e, uint32_t kind, uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = e->qh;
    if (h - e->qt >= EN_QN) return 0;
    e->q[h % EN_QN] = kind << 24 | (a & 255u) << 16 | (b & 255u) << 8 | (c & 255u);
    e->qh = h + 1u;
    return 1;
}
uint32_t en_lock(Engine *e, uint32_t (*wait)(void)) {
    e->lock = 1;
    while (!e->ack && wait()) {}
    return e->ack;
}
void en_unlock(Engine *e, uint32_t (*wait)(void)) {
    e->lock = 0;
    while (e->ack && wait()) {}
}
typedef char en_cons_chk[((int)MC_NONE == (int)FC_NONE && (int)MC_K == (int)FC_K && (int)MC_NN == (int)FC_NN && (int)MC_P == (int)FC_P && (int)MC_Y == (int)FC_Y) ? 1 : -1];
static void sing(Engine *e, uint32_t note, uint32_t mora, uint32_t vel) {   /* consonant + vowel; moraic n is a hum (vowel ignored) */
    uint32_t v = mora_vowel(mora);
    uint32_t lang = e->g->vx.lang;
    if (v < 5u) e->vh = (uint8_t)v;
    fv_note_on_mora(&e->fv, (uint8_t)note, lang == VL_VOWEL ? FC_NONE : (uint8_t)mora_cons(mora), v < 5u ? (uint8_t)v : e->vh, (uint8_t)vel);
    e->mora = mora; e->vnote = (uint8_t)note; e->vvel = (uint8_t)vel;
    e->bt = lang == VL_ANIMAL ? 3087u + rnd(e) % 882u : lang == VL_SCAT ? 1323000u / (e->g->bpm ? e->g->bpm : 120u) : 0;
}
static void command(Engine *e, uint32_t w) {
    uint32_t a = w >> 16 & 255u, b = w >> 8 & 255u, c = w & 255u;
    switch (w >> 24) {
    case EN_PLAY: sq_play(&e->sq, e->g, a, (int)b); break;
    case EN_STOP: sq_stop(&e->sq); break;
    case EN_BPM: sq_set_bpm(&e->sq, a); break;
    case EN_ON:
        if (a == TR_VOCAL) sing(e, b, c, 110);
        else if (a == TR_CHORDS) ch_note_on(&e->ch, (uint8_t)b, 100);
        else if (a == TR_BASS) bs_note_on(&e->bs, (uint8_t)b, 100, 0, 0);
        else if (b < DR_N) dr_trig(&e->dr, (uint8_t)b, 110);
        break;
    case EN_OFF:
        if (a == TR_VOCAL) fv_note_off(&e->fv);
        else if (a == TR_CHORDS) ch_note_off(&e->ch, (uint8_t)b);
        else if (a == TR_BASS) bs_note_off(&e->bs);
        break;
    case EN_VOW: if (mora_vowel(a) < 5u) { e->vh = (uint8_t)mora_vowel(a); fv_set_vowel(&e->fv, e->vh); } break;
    case EN_PANIC: fv_note_off(&e->fv); ch_all_off(&e->ch); bs_note_off(&e->bs); break;
    }
}
static void event(Engine *e, const SqEvent *v) {
    switch (v->type) {
    case SQ_NOTE_ON:
        if (v->trk == TR_VOCAL) sing(e, v->note, v->mora, v->vel);
        else if (v->trk == TR_CHORDS) ch_note_on(&e->ch, v->note, v->vel);
        else bs_note_on(&e->bs, v->note, v->vel, (uint8_t)(v->aux & 1u), (uint8_t)(v->aux >> 1 & 1u));
        break;
    case SQ_NOTE_OFF:
        if (v->trk == TR_VOCAL) fv_note_off(&e->fv);
        else if (v->trk == TR_CHORDS) ch_note_off(&e->ch, v->note);
        else bs_note_off(&e->bs);
        break;
    case SQ_DRUM: dr_trig(&e->dr, v->note, v->vel); break;
    case SQ_STEP: e->pos = (e->pos & 0x10000u) | (uint32_t)v->note << 8 | v->aux; break;
    }
}
static void seg(Engine *e, int32_t *out, uint32_t at, uint32_t n) {
    int32_t *o = out + 2u * at;
    uint32_t i;
    const Voice *x = &e->g->vx;
    VxSet fx;
    if (!n) return;
    for (i = 0; i < n;) {                          /* the voice first, into silence: it is scaled by its own level/pan and +3.5 dB */
        uint32_t m = n - i;
        if (e->bt && e->fv.gated) {
            if (m > e->bt) m = e->bt;
            e->bt -= m;
        }
        fv_render(&e->fv, o + 2u * i, m);
        i += m;
        if (!e->bt && e->fv.gated && (x->lang == VL_ANIMAL || x->lang == VL_SCAT)) {   /* the next babbled syllable */
            uint32_t r = rnd(e), mo = x->lang == VL_ANIMAL ? (r % 3u ? 6u + r % 40u : 47u + r % 23u) : scat[r % 9u];
            int32_t nt = (int32_t)e->vnote + (x->lang == VL_ANIMAL ? ajit[(r >> 8) & 7u] : 0);
            uint32_t v = mora_vowel(mo);
            fv_note_on_mora(&e->fv, (uint8_t)(nt > 127 ? 127 : nt), (uint8_t)mora_cons(mo), v < 5u ? (uint8_t)v : e->vh, e->vvel);
            e->mora = mo;
            e->bt = x->lang == VL_ANIMAL ? 3087u + (r >> 16) % 882u : 1323000u / (e->g->bpm ? e->g->bpm : 120u);
        }
    }
    fx.drv = x->drv; fx.trem = x->trem; fx.cho = x->cho; fx.dly = x->dly; fx.rev = x->rev;
    vx_render(o, n, &fx, e->g->bpm);
    for (i = 0; i < n; i++) {
        o[2u * i] = ((o[2u * i] * e->vl) >> 15) * 3;
        o[2u * i + 1u] = ((o[2u * i + 1u] * e->vr) >> 15) * 3;
    }
    ch_render(&e->ch, o, n); bs_render(&e->bs, o, n); dr_render(&e->dr, o, n);
}
void en_render(Engine *e, int32_t *out, uint32_t n) {
    const Song *g = e->g;
    uint32_t i, k, ne = 0, pos = 0, l = e->lock, m;
    int32_t mg;
    e->ack = l;
    if (!l) {
        while (e->qt != e->qh) { uint32_t t = e->qt; command(e, e->q[t % EN_QN]); e->qt = t + 1u; }
        ne = sq_block(&e->sq, g, n, e->ev, SQ_EVMAX);
        e->pos = (uint32_t)(sq_playing(&e->sq) != 0) << 16 | (e->pos & 0xFFFFu);
    }
    for (k = 0; k < NTRK; k++) {                   /* the mixer state is plain bytes of the song: read every block, no request needed */
        uint32_t p = en_pre(k, g->trk[k].preset), lv = g->trk[k].level, pn = e->pan[k];
        if (k == TR_VOCAL) {
            const Trk *v = &g->trk[0];
            mx_gain((uint8_t)lv, (uint8_t)pn, &e->vl, &e->vr);
            e->fv.vib_depth = v->vib; e->fv.porta = (uint16_t)(v->porta * 5u);
            e->fv.breath = (uint8_t)(v->breath * 127u / 100u); e->fv.bright = (uint8_t)(v->bright * 127u / 100u);
            e->fv.attack = (uint16_t)(g->vx.atk * 4u); e->fv.release = (uint16_t)(g->vx.rel * 5u); e->fv.scoop = g->vx.scoop;
            e->fv.human = (uint8_t)(g->vx.human * 127u / 100u); e->fv.vib_rate = (uint16_t)(30u + g->vx.vrate / 2u);
            e->fv.vib_delay = (uint16_t)(g->vx.vdel * 10u); e->fv.chr = g->vx.chr;
            continue;
        }
        if (p != e->pre[k]) {
            e->pre[k] = (uint8_t)p;
            if (k == TR_CHORDS) ch_set_preset(&e->ch, (uint8_t)p);
            else if (k == TR_BASS) bs_set_preset(&e->bs, (uint8_t)p);
            else dr_set_preset(&e->dr, (uint8_t)p);
        }
        if (k == TR_CHORDS) ch_set_mix(&e->ch, (uint8_t)lv, (uint8_t)pn);
        else if (k == TR_BASS) bs_set_mix(&e->bs, (uint8_t)lv, (uint8_t)pn);
        else dr_set_mix(&e->dr, (uint8_t)lv, (uint8_t)pn);
    }
    for (i = 0; i < ne; i++) {
        if (e->ev[i].at > pos) { seg(e, out, pos, e->ev[i].at - pos); pos = e->ev[i].at; }
        event(e, &e->ev[i]);
    }
    seg(e, out, pos, n - pos);
    m = g->master > 127u ? 127u : g->master;
    mg = (int32_t)(m * m * 3072u / 10000u);        /* Q12, master 100 = 0.75 (-2.5 dB): the voice alone peaks near -3 dBFS, the full band needs the head room */
    for (i = 0; i < 2u * n; i++) {
        int32_t x = out[i];
        x = x > 131071 ? 131071 : x < -131071 ? -131071 : x;
        out[i] = (x * mg) >> 12;
    }
    mx_limit(out, n);
    e->mouth = fv_mouth(&e->fv); e->level = fv_level(&e->fv);
    e->act = (fv_level(&e->fv) > 40u) | (uint32_t)(ch_active(&e->ch) != 0) << 1 | (uint32_t)(bs_active(&e->bs) != 0) << 2 | (uint32_t)(dr_active(&e->dr) != 0) << 3;
}

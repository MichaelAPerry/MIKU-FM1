/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/app/engine.h"
static const uint8_t npre[NTRK] = { 1, CH_NPRESET, BS_NPRESET, DR_NPRESET };
/* languages: ANIMAL babbles random morae (ka..wo, ga..po) at an adjustable 140-220 ms default cadence with small pitch jumps, like animal-game voices;
   SCAT sings da ba bi do pa ra ri de be on eighth notes; both keep the held note's envelope (legato articulation) */
static const uint8_t scat[9] = { 57, 60, 61, 59, 65, 39, 40, 58, 63 };   /* mora indices of da ba bi do pa ra ri de be */
static const int8_t ajit[8] = { 0, 0, 2, 3, 5, -2, 7, 4 };
/* Session speech rate: higher is faster; 0/50/100 -> 180-260/140-220/100-180 ms.
   Sample counters keep timing independent of render block sizes, tempo and other languages. */
static uint32_t animal_wait(const Engine *e, uint32_t r) {
    return (180u - e->arate * 4u / 5u) * 441u / 10u + r % 3529u;
}
static uint32_t rnd(Engine *e) { uint32_t x = e->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; e->rng = x; return x; }
/* Harmony: a second voice (fv2) sings every vocal note at an interval, with its own character and a small detune, mixed in
   before the effects. On-beat: the band is delayed by D (up to the longest consonant, 66 ms x consonant length) and every
   sequenced vocal note starts D minus its own consonant time later, so its vowel lands where the band's beat now is; a
   delayed note-off ends only the note it belongs to. Both are off (and exactly bypassed) at 0. */
#ifndef VX_POOL
#define VX_POOL
#endif
#define BDN 8192u
static int32_t bdl[2u * BDN] VX_POOL, vtmp[2u * 1024u] VX_POOL, btmp[2u * 1024u] VX_POOL;
static const int8_t hiv[7] = { 0, -12, 3, 4, 5, 7, 12 };
static int32_t hint_st(const Voice *x) { return hiv[x->hint * 7u / 101u]; }
static uint32_t csc(const Voice *x) { return 25u + x->clen * 3u / 2u; }                  /* consonant length, % */
static uint32_t obd(const Voice *x) { return x->obeat * 2911u / 100u * csc(x) / 100u; }  /* band delay, frames */
static void hnote(Engine *e, int32_t note, uint32_t cons, uint32_t vw, uint32_t vel) {
    int32_t iv = hint_st(&e->g->vx);
    if (!iv) return;
    note += iv;
    fv_note_on_mora(&e->fv2, (uint8_t)(note < 1 ? 1 : note > 127 ? 127 : note), (uint8_t)cons, (uint8_t)vw, (uint8_t)vel);
}
static void voff(Engine *e) { fv_note_off(&e->fv); fv_note_off(&e->fv2); }
uint32_t en_pre(uint32_t t, uint32_t p) { return p < npre[t] ? p : 0; }
void en_init(Engine *e, const Song *g) {
    uint32_t t;
    e->g = g;
    sq_init(&e->sq); fv_init(&e->fv); fv_init(&e->fv2); ch_init(&e->ch); bs_init(&e->bs); dr_init(&e->dr); vx_init();
    e->fv2.rng = 0x2545F491u;                       /* the harmony voice drifts on its own */
    e->clk = e->bw = 0; e->vqn = 0; e->bdon = 0; e->vid = e->vcur = 0;
    e->arate = 50; e->bt = 0; e->rng = 0x6D2B79F5u; e->vnote = 60; e->vvel = 110; e->vmora = 0;
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
    uint32_t lang = e->g->vx.lang, cons = lang == VL_VOWEL ? FC_NONE : mora_cons(mora);
    if (v < 5u) e->vh = (uint8_t)v;
    fv_note_on_mora(&e->fv, (uint8_t)note, (uint8_t)cons, v < 5u ? (uint8_t)v : e->vh, (uint8_t)vel);
    hnote(e, (int32_t)note, cons, v < 5u ? v : e->vh, vel);
    e->mora = mora; e->vnote = (uint8_t)note; e->vvel = (uint8_t)vel;
    e->bt = lang == VL_ANIMAL ? animal_wait(e, rnd(e)) : lang == VL_SCAT ? 1323000u / (e->g->bpm ? e->g->bpm : 120u) : 0;
}
static void command(Engine *e, uint32_t w) {
    uint32_t a = w >> 16 & 255u, b = w >> 8 & 255u, c = w & 255u;
    switch (w >> 24) {
    case EN_PLAY: sq_play(&e->sq, e->g, a, (int)b); break;
    case EN_STOP: sq_stop(&e->sq); break;
    case EN_BPM: sq_set_bpm(&e->sq, a); break;
    case EN_RATE: e->arate = (uint8_t)(a > 100u ? 100u : a); break;   /* next interval only; never retrigger a held note */
    case EN_ON:
        if (a == TR_VOCAL) { sing(e, b, c, 110); e->vcur = ++e->vid; }
        else if (a == TR_CHORDS) ch_note_on(&e->ch, (uint8_t)b, 100);
        else if (a == TR_BASS) bs_note_on(&e->bs, (uint8_t)b, 100, 0, 0);
        else if (b < DR_N) dr_trig(&e->dr, (uint8_t)b, 110);
        break;
    case EN_OFF:
        if (a == TR_VOCAL) voff(e);
        else if (a == TR_CHORDS) ch_note_off(&e->ch, (uint8_t)b);
        else if (a == TR_BASS) bs_note_off(&e->bs);
        break;
    case EN_VOW: if (mora_vowel(a) < 5u) { e->vh = (uint8_t)mora_vowel(a); fv_set_vowel(&e->fv, e->vh); } break;
    case EN_PANIC: voff(e); e->vqn = 0; ch_all_off(&e->ch); bs_note_off(&e->bs); break;
    }
}
static void vq_push(Engine *e, uint32_t on, uint32_t delay, const SqEvent *v, uint16_t id) {
    EnVq *q = &e->vq[e->vqn++];
    q->due = e->clk + delay; q->on = (uint8_t)on; q->note = v->note; q->mora = v->mora; q->vel = v->vel; q->id = id;
}
static void vq_apply(Engine *e, const EnVq *q) {
    if (q->on) { sing(e, q->note, q->mora, q->vel); e->vcur = q->id; }
    else if (q->id == e->vcur) voff(e);
}
static void vocal_event(Engine *e, const SqEvent *v, uint32_t on) {   /* a sequenced vocal note: now, or queued (on-beat) */
    const Voice *x = &e->g->vx;
    uint32_t d = obd(x);
    if ((!d && !e->vqn) || e->vqn >= EN_VQN) {
        if (on) { sing(e, v->note, v->mora, v->vel); e->vcur = ++e->vid; }
        else voff(e);
        return;
    }
    if (on) {
        uint32_t c = x->lang == VL_VOWEL ? 0u : fv_cons_ms((uint8_t)mora_cons(v->mora)) * 441u / 10u * csc(x) / 100u * x->obeat / 100u;
        vq_push(e, 1, d > c ? d - c : 0, v, ++e->vid);
    } else vq_push(e, 0, d, v, e->vid);
}
static void vq_run(Engine *e) {                    /* apply every queued event that is due, earliest first */
    for (;;) {
        uint32_t i, b = EN_VQN;
        for (i = 0; i < e->vqn; i++)
            if ((int32_t)(e->vq[i].due - e->clk) <= 0 && (b == EN_VQN || (int32_t)(e->vq[i].due - e->vq[b].due) < 0 ||
                (e->vq[i].due == e->vq[b].due && !e->vq[i].on && e->vq[b].on))) b = i;
        if (b == EN_VQN) return;
        { EnVq q = e->vq[b]; e->vq[b] = e->vq[--e->vqn]; vq_apply(e, &q); }
    }
}
static uint32_t vq_wait(const Engine *e) {         /* frames to the next queued event */
    uint32_t i, w = 0xFFFFFFFFu;
    for (i = 0; i < e->vqn; i++) { int32_t d = (int32_t)(e->vq[i].due - e->clk); if (d > 0 && (uint32_t)d < w) w = (uint32_t)d; }
    return w;
}
static void event(Engine *e, const SqEvent *v) {
    switch (v->type) {
    case SQ_NOTE_ON:
        if (v->trk == TR_VOCAL) vocal_event(e, v, 1);
        else if (v->trk == TR_CHORDS) ch_note_on(&e->ch, v->note, v->vel);
        else bs_note_on(&e->bs, v->note, v->vel, (uint8_t)(v->aux & 1u), (uint8_t)(v->aux >> 1 & 1u));
        break;
    case SQ_NOTE_OFF:
        if (v->trk == TR_VOCAL) vocal_event(e, v, 0);
        else if (v->trk == TR_CHORDS) ch_note_off(&e->ch, v->note);
        else bs_note_off(&e->bs);
        break;
    case SQ_DRUM: dr_trig(&e->dr, v->note, v->vel); break;
    case SQ_STEP: e->pos = (e->pos & 0x10000u) | (uint32_t)v->note << 8 | v->aux; break;
    }
}
static void seg(Engine *e, int32_t *out, uint32_t at, uint32_t n) {
    int32_t *o = out + 2u * at;
    uint32_t i, j, bd;
    const Voice *x = &e->g->vx;
    int32_t hm = x->hmix * 229;
    VxSet fx;
    if (!n) return;
    for (i = 0; i < n;) {                          /* the voice first, into silence: it is scaled by its own level/pan and +3.5 dB */
        uint32_t m = n - i;
        vq_run(e);
        if (e->vqn) { uint32_t w = vq_wait(e); if (m > w) m = w; }
        if (e->bt && e->fv.gated) {
            if (m > e->bt) m = e->bt;
            e->bt -= m;
        }
        fv_render(&e->fv, o + 2u * i, m);
        if (e->fv2.gated || e->fv2.env || e->fv2.gcur || e->fv2.nl) {   /* the harmony voice, mixed in */
            for (j = 0; j < 2u * m; j++) vtmp[j] = 0;
            fv_render(&e->fv2, vtmp, m);
            for (j = 0; j < 2u * m; j++) o[2u * i + j] += (vtmp[j] * hm) >> 15;
        }
        e->clk += m;
        i += m;
        if (!e->bt && e->fv.gated && (x->lang == VL_ANIMAL || x->lang == VL_SCAT)) {   /* the next babbled syllable */
            uint32_t r = rnd(e), mo = x->lang == VL_ANIMAL ? (r % 3u ? 6u + r % 40u : 47u + r % 23u) : scat[r % 9u];
            int32_t nt = (int32_t)e->vnote + (x->lang == VL_ANIMAL ? ajit[(r >> 8) & 7u] : 0);
            uint32_t v = mora_vowel(mo);
            fv_note_on_mora(&e->fv, (uint8_t)(nt > 127 ? 127 : nt), (uint8_t)mora_cons(mo), v < 5u ? (uint8_t)v : e->vh, e->vvel);
            hnote(e, nt > 127 ? 127 : nt, mora_cons(mo), v < 5u ? v : e->vh, e->vvel);
            e->mora = mo;
            e->bt = x->lang == VL_ANIMAL ? animal_wait(e, r >> 16) : 1323000u / (e->g->bpm ? e->g->bpm : 120u);
        }
    }
    fx.drv = x->drv; fx.trem = x->trem; fx.cho = x->cho; fx.dly = x->dly; fx.rev = x->rev;
    fx.rsize = x->rsize; fx.dtime = x->dtime; fx.dfb = x->dfb; fx.crate = x->crate;
    vx_render(o, n, &fx, e->g->bpm);
    for (i = 0; i < n; i++) {
        o[2u * i] = ((o[2u * i] * e->vl) >> 15) * 3;
        o[2u * i + 1u] = ((o[2u * i + 1u] * e->vr) >> 15) * 3;
    }
    bd = obd(x);
    if (!bd) { e->bdon = 0; ch_render(&e->ch, o, n); bs_render(&e->bs, o, n); dr_render(&e->dr, o, n); return; }
    if (!e->bdon) { for (j = 0; j < 2u * BDN; j++) bdl[j] = 0; e->bdon = 1; }
    for (j = 0; j < 2u * n; j++) btmp[j] = 0;
    ch_render(&e->ch, btmp, n); bs_render(&e->bs, btmp, n); dr_render(&e->dr, btmp, n);
    for (j = 0; j < n; j++) {                      /* on-beat: the band, D frames late */
        uint32_t r = (e->bw - bd) & (BDN - 1u);
        bdl[2u * e->bw] = btmp[2u * j]; bdl[2u * e->bw + 1u] = btmp[2u * j + 1u];
        o[2u * j] += bdl[2u * r]; o[2u * j + 1u] += bdl[2u * r + 1u];
        e->bw = (e->bw + 1u) & (BDN - 1u);
    }
}
void en_render(Engine *e, int32_t *out, uint32_t n) {
    const Song *g = e->g;
    uint32_t i, k, ne = 0, pos = 0, l = e->lock, m;
    int32_t mg;
    e->ack = l;
    for (k = 0; k < NTRK; k++) {                   /* the mixer and voice settings are plain bytes of the song: read every block, before the commands (a new note starts with them) */
        uint32_t p = en_pre(k, g->trk[k].preset), lv = g->trk[k].level, pn = e->pan[k];
        if (k == TR_VOCAL) {
            const Trk *v = &g->trk[0];
            mx_gain((uint8_t)lv, (uint8_t)pn, &e->vl, &e->vr);
            e->fv.vib_depth = v->vib; e->fv.porta = (uint16_t)(v->porta * 5u);
            e->fv.breath = (uint8_t)(v->breath * 127u / 100u); e->fv.bright = (uint8_t)(v->bright * 127u / 100u);
            e->fv.attack = (uint16_t)(g->vx.atk * g->vx.atk / 20u); e->fv.release = (uint16_t)(g->vx.rel * g->vx.rel / 20u); e->fv.scoop = g->vx.scoop;   /* times: squared knobs, 0..500 ms */
            e->fv.human = (uint8_t)(g->vx.human * 127u / 100u); e->fv.vib_rate = (uint16_t)(30u + g->vx.vrate / 2u);
            e->fv.vib_delay = (uint16_t)(g->vx.vdel * g->vx.vdel / 10u);
            {   /* character + fine formant shift (50 = none); source; articulation (50 = 100 %) */
                int32_t ps = (int32_t)(g->vx.chr % VC_N) * 100 + ((int32_t)g->vx.fmt - 50) * 2;
                ps = ps < 0 ? 0 : ps > (VC_N - 1) * 100 ? (VC_N - 1) * 100 : ps;
                e->fv.chr = (uint8_t)(ps / 100); e->fv.chf = (uint8_t)(ps % 100);
            }
            e->fv.tension = g->vx.tens; e->fv.fry = g->vx.fry; e->fv.whisper = g->vx.whsp;
            e->fv.cscale = (uint8_t)csc(&g->vx); e->fv.vscale = (uint8_t)(25u + g->vx.vspd * 3u / 2u); e->fv.clevel = (uint16_t)(g->vx.clev * 2u);
            {   /* the harmony voice: the same settings, its own character, detuned up to 30 cents */
                FvVoice *h = &e->fv2;
                h->vib_depth = e->fv.vib_depth; h->vib_rate = e->fv.vib_rate; h->vib_delay = e->fv.vib_delay; h->porta = e->fv.porta;
                h->attack = e->fv.attack; h->release = e->fv.release; h->breath = e->fv.breath; h->bright = e->fv.bright;
                h->human = e->fv.human; h->scoop = e->fv.scoop; h->tension = e->fv.tension; h->fry = e->fv.fry; h->whisper = e->fv.whisper;
                h->cscale = e->fv.cscale; h->vscale = e->fv.vscale; h->clevel = e->fv.clevel;
                h->chr = (uint8_t)(g->vx.hchr % VC_N); h->chf = 0; h->tune = (int16_t)(g->vx.hdet * 1024u / 1000u);
                if (!hint_st(&g->vx) && h->gated) fv_note_off(h);
            }
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
    if (!l) {
        while (e->qt != e->qh) { uint32_t t = e->qt; command(e, e->q[t % EN_QN]); e->qt = t + 1u; }
        ne = sq_block(&e->sq, g, n, e->ev, SQ_EVMAX);
        e->pos = (uint32_t)(sq_playing(&e->sq) != 0) << 16 | (e->pos & 0xFFFFu);
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

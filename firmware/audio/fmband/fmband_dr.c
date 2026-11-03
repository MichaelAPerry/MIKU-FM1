/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/audio/fmband/fmband.h"
#include "firmware/audio/fmband/fmband_env.h"
/* Synthesized kit, all integer. Envelopes (Q24, exponential) and sweeps run per 16-frame block, amplitudes are ramped per
   sample. One shared sample-and-hold noise source per block.
   kick: sine/tri with exponential pitch sweep + noise click (2 mul/sample); snare: tone + band-passed noise (4);
   clap: 3 noise bursts + tail through a band-pass (3); hats: 6 square cluster (or noise) high-passed, closed/open envelopes,
   closed chokes open (3, shared); perc: FM blip (3). Worst case, everything sounding: 15 mul/sample + 2 for pan. */
#define HZ10(x) ((uint32_t)(x) * 9739u)          /* deci-Hz -> phase increment (2^32 / 44100 = 97391.55 per Hz) */
static const uint32_t hinc[6] = { HZ10(2053), HZ10(3044), HZ10(3696), HZ10(5227), HZ10(5400), HZ10(8000) };   /* 808 hat cluster */
static const DrPreset pre[DR_NPRESET] = {
    { /* 808-ish */
      .k_f = HZ10(480), .k_sw = HZ10(1300), .s_f = HZ10(1900), .p_f = HZ10(2000), .p_sw = HZ10(1200),
      .k_lv = 27000, .s_lv = 24000, .s_tl = 170, .s_nl = 230, .s_khp = 7412, .s_klp = 16696, .c_lv = 24000, .c_klp = 16696, .c_khp = 3530,
      .h_lv = 32000, .h_khp = 20681, .p_lv = 22000, .p_ratio = 360, .p_idx = 400,
      .k_rsw = 31, .k_ra = 42, .k_cl = 60, .k_rc = 3, .k_wv = 0, .s_rt = 31, .s_rn = 37, .s_wv = 0, .hold = 1,
      .c_rb = 12, .c_rt = 37, .c_sp = 27, .h_rc = 26, .h_ro = 41, .h_noise = 0, .p_rsw = 26, .p_ra = 38, .p_ri = 30, .p_wv = 0 },
    { /* chip / 8-bit: triangle kick, held noise, square blips */
      .k_f = HZ10(550), .k_sw = HZ10(3300), .s_f = HZ10(2200), .p_f = HZ10(7000), .p_sw = HZ10(5000),
      .k_lv = 24000, .s_lv = 22000, .s_tl = 90, .s_nl = 240, .s_khp = 4351, .s_klp = 23678, .c_lv = 22000, .c_klp = 16696, .c_khp = 7412,
      .h_lv = 30000, .h_khp = 18830, .p_lv = 18000, .p_ratio = 256, .p_idx = 0,
      .k_rsw = 30, .k_ra = 34, .k_cl = 0, .k_rc = 3, .k_wv = 1, .s_rt = 26, .s_rn = 33, .s_wv = 2, .hold = 3,
      .c_rb = 10, .c_rt = 31, .c_sp = 20, .h_rc = 22, .h_ro = 35, .h_noise = 1, .p_rsw = 22, .p_ra = 30, .p_ri = 20, .p_wv = 2 },
};
static void cpy(void *d, const void *s, uint32_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; }
static void zero(void *p, uint32_t n) { for (uint8_t *b = (uint8_t *)p, *e = b + n; b < e; b++) *b = 0; }
void dr_init(DrKit *d) { zero(d, sizeof *d); d->rng = 0x2545F491u; dr_set_preset(d, 0); dr_set_mix(d, 100, 64); }
void dr_set_preset(DrKit *d, uint8_t n) {
    d->preset = n >= DR_NPRESET ? 0 : n; cpy(&d->p, &pre[d->preset], sizeof d->p);
    d->pikb = (int32_t)(((uint32_t)d->p.p_idx * 5214u) >> 10);
}
void dr_set_mix(DrKit *d, uint8_t level, uint8_t pan) { mx_gain(level, pan, &d->gl, &d->gr); }
uint8_t dr_active(const DrKit *d) {
    return (uint8_t)((d->kae || d->kce) + (d->ste || d->sne) + (d->cne || d->cbl) + (d->hae != 0) + (d->hao != 0) + (d->pae != 0));
}
void dr_trig(DrKit *d, uint8_t inst, uint8_t vel) {
    const DrPreset *p = &d->p;
    if (vel > 127) vel = 127;
    switch (inst) {
    case DR_KICK: d->kvg = fb_vg(p->k_lv, vel); d->kae = d->kse = d->kce = FB_FULL; d->kph = 0; d->kinc = p->k_f + p->k_sw;
        d->ka = d->kvg; d->kda = 0; d->kca = (d->kvg * p->k_cl) >> 8; d->kdca = 0; break;
    case DR_SNARE: d->svg = fb_vg(p->s_lv, vel); d->ste = d->sne = FB_FULL; d->sph = 0;
        d->ta = (d->svg * p->s_tl) >> 8; d->na = (d->svg * p->s_nl) >> 8; d->dta = d->dna = 0; break;
    case DR_CLAP: d->cvg = fb_vg(p->c_lv, vel); d->cne = FB_FULL; d->cbl = 3; d->cnb = p->c_sp; d->ca = d->cvg; d->dca = 0; break;
    case DR_CHH: d->hvg = fb_vg(p->h_lv, vel); d->hae = FB_FULL; d->ha = d->hvg; d->dha = 0; if (d->hao) d->choke = 1; break;
    case DR_OHH: d->ovg = fb_vg(p->h_lv, vel); d->hao = FB_FULL; d->oa = d->ovg; d->doa = 0; d->choke = 0; break;
    case DR_PERC: d->pvg = fb_vg(p->p_lv, vel); d->pae = d->pse = d->pie = FB_FULL; d->pph = d->ppm = 0; d->pinc = p->p_f + p->p_sw;
        d->pa = d->pvg; d->dpa = 0; d->pik = d->pikb > 65535 ? 65535 : d->pikb; d->pdik = 0; break;
    default: break;
    }
}
static int32_t osc(uint32_t ph, int wv) {
    if (wv == 0) return fmb_sin[ph >> 22];
    if (wv == 1) { int32_t u = (int32_t)(ph >> 16); return (u < 32768 ? u : 65535 - u) * 2 - 32767; }
    return ph >> 31 ? -24000 : 24000;
}
static int32_t tgt(uint32_t e, int32_t vg) { return (int32_t)(((e >> 9) * (uint32_t)vg) >> 15); }
static void block(DrKit *d) {
    const DrPreset *p = &d->p;
    if (d->kae || d->kce) {
        d->kse = fb_ap(d->kse, 0, fmb_kdec[p->k_rsw]); d->kae = fb_ap(d->kae, 0, fmb_kdec[p->k_ra]); d->kce = fb_ap(d->kce, 0, fmb_kdec[p->k_rc]);
        d->kinc = p->k_f + (p->k_sw >> 8) * (d->kse >> 16);
        d->kda = (tgt(d->kae, d->kvg) - d->ka) >> 4; d->kdca = ((tgt(d->kce, d->kvg) * p->k_cl >> 8) - d->kca) >> 4;
    }
    if (d->ste || d->sne) {
        d->ste = fb_ap(d->ste, 0, fmb_kdec[p->s_rt]); d->sne = fb_ap(d->sne, 0, fmb_kdec[p->s_rn]);
        d->dta = ((tgt(d->ste, d->svg) * p->s_tl >> 8) - d->ta) >> 4; d->dna = ((tgt(d->sne, d->svg) * p->s_nl >> 8) - d->na) >> 4;
    }
    if (d->cne || d->cbl) {
        d->cne = fb_ap(d->cne, 0, fmb_kdec[d->cbl ? p->c_rb : p->c_rt]);
        if (d->cbl && !--d->cnb) { d->cne = FB_FULL; d->cnb = p->c_sp; d->cbl--; }
        d->dca = (tgt(d->cne, d->cvg) - d->ca) >> 4;
    }
    if (d->hae || d->hao) {
        d->hae = fb_ap(d->hae, 0, fmb_kdec[p->h_rc]); d->hao = fb_ap(d->hao, 0, fmb_kdec[d->choke ? 2 : p->h_ro]);
        if (!d->hao) d->choke = 0;
        d->dha = (tgt(d->hae, d->hvg) - d->ha) >> 4; d->doa = (tgt(d->hao, d->ovg) - d->oa) >> 4;
    }
    if (d->pae) {
        d->pse = fb_ap(d->pse, 0, fmb_kdec[p->p_rsw]); d->pae = fb_ap(d->pae, 0, fmb_kdec[p->p_ra]); d->pie = fb_ap(d->pie, 0, fmb_kdec[p->p_ri]);
        d->pinc = p->p_f + (p->p_sw >> 8) * (d->pse >> 16);
        d->dpa = (tgt(d->pae, d->pvg) - d->pa) >> 4;
        d->pdik = ((int32_t)(((uint32_t)(d->pikb > 65535 ? 65535 : d->pikb) * (d->pie >> 8)) >> 16) - d->pik) >> 4;
    }
}
void dr_render(DrKit *d, int32_t *out, uint32_t n) {
    const DrPreset *p = &d->p;
    while (n) {
        if (!d->left) {
            if (!dr_active(d)) return;
            block(d); d->left = FMB_BLK;
        }
        uint32_t m = n < d->left ? n : d->left;
        d->left = (uint8_t)(d->left - m); n -= m;
        int32_t mb[FMB_BLK], nz[FMB_BLK];
        for (uint32_t i = 0; i < m; i++) {
            if (!d->hc) { d->rng = fb_xs(d->rng); d->hn = (int16_t)(d->rng >> 16); }
            if (++d->hc >= (p->hold ? p->hold : 1)) d->hc = 0;
            nz[i] = d->hn; mb[i] = 0;
        }
        if (d->kae || d->kce) {
            uint32_t ph = d->kph; int32_t a = d->ka, c = d->kca;
            for (uint32_t i = 0; i < m; i++) {
                ph += d->kinc; a += d->kda; c += d->kdca;
                mb[i] += ((osc(ph, p->k_wv) * a) >> 15) + ((nz[i] * c) >> 15);
            }
            d->kph = ph; d->ka = a; d->kca = c;
        }
        if (d->ste || d->sne) {
            uint32_t ph = d->sph; int32_t a = d->ta, b = d->na, lp = d->snlp, l1 = d->sn1;
            for (uint32_t i = 0; i < m; i++) {
                ph += p->s_f; a += d->dta; b += d->dna;
                l1 += ((nz[i] - l1) * (int32_t)p->s_klp) >> 15; lp += ((l1 - lp) * (int32_t)p->s_khp) >> 15;
                mb[i] += ((osc(ph, p->s_wv) * a) >> 15) + (((l1 - lp) * b) >> 15);
            }
            d->sph = ph; d->ta = a; d->na = b; d->snlp = lp; d->sn1 = l1;
        }
        if (d->cne || d->cbl) {
            int32_t a = d->ca, l1 = d->cl1, l2 = d->cl2;
            for (uint32_t i = 0; i < m; i++) {
                a += d->dca;
                l1 += ((nz[i] - l1) * (int32_t)p->c_klp) >> 15; l2 += ((l1 - l2) * (int32_t)p->c_khp) >> 15;
                mb[i] += ((l1 - l2) * a) >> 14;
            }
            d->ca = a; d->cl1 = l1; d->cl2 = l2;
        }
        if (d->hae || d->hao) {
            int32_t a = d->ha, b = d->oa, lp = d->hlp;
            for (uint32_t i = 0; i < m; i++) {
                a += d->dha; b += d->doa;
                int32_t s = 0;
                if (p->h_noise) s = nz[i];
                else for (int q = 0; q < 6; q++) { d->hph[q] += hinc[q]; s += d->hph[q] >> 31 ? -5000 : 5000; }
                lp += ((s - lp) * (int32_t)p->h_khp) >> 15;
                int32_t h = s - lp;
                mb[i] += ((h * a) >> 14) + ((h * b) >> 14);
            }
            d->ha = a; d->oa = b; d->hlp = lp;
        }
        if (d->pae) {
            uint32_t ph = d->pph, pm = d->ppm; int32_t a = d->pa, ik = d->pik;
            for (uint32_t i = 0; i < m; i++) {
                ph += d->pinc; pm += (d->pinc >> 8) * p->p_ratio; a += d->dpa; ik += d->pdik;
                mb[i] += (osc(ph + ((uint32_t)(fmb_sin[pm >> 22] * ik) << 4), p->p_wv) * a) >> 15;
            }
            d->pph = ph; d->ppm = pm; d->pa = a; d->pik = ik;
        }
        for (uint32_t i = 0; i < m; i++, out += 2) {
            int32_t x = fb_clamp(mb[i], -32767, 32767);
            out[0] += (x * d->gl) >> 15; out[1] += (x * d->gr) >> 15;
        }
    }
}

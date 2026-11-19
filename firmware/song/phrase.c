/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/phrase.h"
#include "firmware/song/english.h"
#include "firmware/song/lyrics.h"
#include "firmware/song/mora.h"

enum { PH_CRC_AT = 1020, PH_SYL_AT = 160 };
_Static_assert(sizeof(PhSyl) == 16, "phrase syllable layout");

static void ph_copy(void *dst, const void *src, uint32_t n)
{
    volatile uint8_t *d = dst;
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

/* Keep loops explicit on the device compiler, which can emit libc for aggregates. */
static void ph_clear(void *dst, uint32_t n)
{
    volatile uint8_t *d = dst;
    for (uint32_t i = 0; i < n; i++) d[i] = 0;
}

static int ph_zero(const uint8_t *s, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) if (s[i]) return 0;
    return 1;
}

static int ph_name_ok(const char *s)
{
    for (uint32_t i = 0; i < 12; i++)
        if (!(s[i] == ' ' || (s[i] >= 'A' && s[i] <= 'Z') ||
              (s[i] >= '0' && s[i] <= '9'))) return 0;
    return 1;
}

static int ph_printable(char c)
{
    return c >= 32 && c <= 126;
}

static int ph_padded(const char *s, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (!s[i]) return ph_zero((const uint8_t *)s + i, n - i);
        if (!ph_printable(s[i])) return 0;
    }
    return 1;
}

static int ph_header_ok(uint32_t lang, uint32_t bpm, uint32_t n, uint32_t deflen, uint32_t flags)
{
    return lang <= PH_JP && bpm >= 40 && bpm <= 240 && n <= PH_SYL &&
           deflen >= 1 && deflen <= 16 && !(flags & ~PH_MELODY);
}

static int ph_syl_ok(const uint8_t *s, uint32_t lang)
{
    if (s[14] > 127 || s[13] || !ph_padded((const char *)s + 8, 6)) return 0;
    if (lang == PH_JP) return s[0] < MORA_N && ph_zero(s + 1, 7);
    for (uint32_t i = 0; i < 8; i++) if (s[i] > FV_EP_AX) return 0;
    return 1;
}

int ph_encode(const Phrase *p, uint8_t rec[PH_REC])
{
    uint32_t crc;
    if (!p || !rec || !ph_header_ok(p->lang, p->bpm, p->n, p->deflen, p->flags) ||
        !ph_name_ok(p->name) || !ph_padded(p->text, PH_TEXT)) return -1;
    for (uint32_t i = 0; i < p->n; i++)
        if (!ph_syl_ok((const uint8_t *)&p->s[i], p->lang)) return -1;
    ph_clear(rec, PH_CRC_AT);
    ph_copy(rec, "MPH1", 4);
    rec[4] = 1;
    rec[5] = p->lang;
    rec[6] = p->bpm;
    rec[7] = p->n;
    rec[8] = p->deflen;
    rec[9] = p->flags;
    ph_copy(rec + 12, p->name, 12);
    ph_copy(rec + 32, p->text, PH_TEXT);
    ph_copy(rec + PH_SYL_AT, p->s, 16u * p->n);
    crc = sg_crc32(rec, PH_CRC_AT);
    for (uint32_t i = 0; i < 4; i++) rec[PH_CRC_AT + i] = (uint8_t)(crc >> (8u * i));
    return 0;
}

int ph_decode(const uint8_t rec[PH_REC], Phrase *p)
{
    uint32_t crc = 0;
    if (!rec || rec[0] != 'M' || rec[1] != 'P' || rec[2] != 'H' || rec[3] != '1' ||
        rec[4] != 1 || !ph_header_ok(rec[5], rec[6], rec[7], rec[8], rec[9])) return -1;
    for (uint32_t i = 0; i < 4; i++) crc |= (uint32_t)rec[PH_CRC_AT + i] << (8u * i);
    if (crc != sg_crc32(rec, PH_CRC_AT) || !ph_zero(rec + 10, 2) ||
        !ph_zero(rec + 24, 8) || !ph_name_ok((const char *)rec + 12) ||
        !ph_padded((const char *)rec + 32, PH_TEXT) ||
        !ph_zero(rec + PH_SYL_AT + 16u * rec[7], PH_CRC_AT - PH_SYL_AT - 16u * rec[7]))
        return -1;
    for (uint32_t i = 0; i < rec[7]; i++)
        if (!ph_syl_ok(rec + PH_SYL_AT + i * 16u, rec[5])) return -1;
    if (p) {
        p->lang = rec[5];
        p->bpm = rec[6];
        p->n = rec[7];
        p->deflen = rec[8];
        p->flags = rec[9];
        ph_copy(p->name, rec + 12, 12);
        ph_copy(p->text, rec + 32, PH_TEXT);
        ph_copy(p->s, rec + PH_SYL_AT, sizeof p->s);
    }
    return 0;
}

static const char *ph_lookup(const char *word, void *user)
{
    static const struct { char word[11]; const char *arpa; } ph_words[] = {
        {"miku", "M IY1 K UW0"},
        {"hatsune", "HH AA0 T S UW1 N EH0"},
        {"vocaloid", "V OW1 K AH0 L OY2 D"},
        {"kawaii", "K AA0 W AA1 IY0"},
        {"arigato", "AA0 R IY0 G AA1 T OW0"},
        {"konnichiwa", "K OW0 N N IY1 CH IY0 W AA0"},
        {"sayonara", "S AA0 Y OW0 N AA1 R AA0"},
        {"senpai", "S EH1 N P AY0"}
    };
    (void)user;
    for (uint32_t i = 0; i < sizeof ph_words / sizeof ph_words[0]; i++) {
        uint32_t j = 0;
        while (word[j] && word[j] == ph_words[i].word[j]) j++;
        if (!word[j] && !ph_words[i].word[j]) return ph_words[i].arpa;
    }
    return 0;
}

static void ph_spell(PhSyl *s, const char *word)
{
    uint32_t i = 0;
    while (i < 5 && word[i]) { s->spell[i] = word[i]; i++; }
}

static void ph_emit_en(void *user, int i, const EnSylOut *out)
{
    Phrase *p = user;
    PhSyl s;
    ph_clear(&s, sizeof s);
    ph_copy(s.ph, out->s.on, 3);
    s.ph[3] = out->s.v == EP_AH && !out->stress ? FV_EP_AX : out->s.v;
    ph_copy(s.ph + 4, out->s.co, 4);
    ph_spell(&s, out->text);
    if (out->flags & EN_WORD_END) s.len = PH_WORD_END;
    ph_copy(&p->s[i], &s, sizeof s);
}

static int ph_compile_en(const char *text, Phrase *p, int *err_at)
{
    int n, rc = en_text_ex_emit(text, EN_SUNG, ph_lookup, 0, 0, 0, PH_SYL, &n, err_at);
    if (rc) return rc;
    rc = en_text_ex_emit(text, EN_SUNG, ph_lookup, 0, ph_emit_en, p, PH_SYL, &n, err_at);
    return rc ? rc : n;
}

/* Locate a token in already validated printable romaji. */
static int ph_jp_at(const char *text, uint32_t index)
{
    int at = 0;
    while (text[at] == ' ') at++;
    while (index-- && text[at]) {
        while (text[at] && text[at] != ' ' && text[at] != '-') at++;
        if (text[at] == '-') at++;
        while (text[at] == ' ') at++;
    }
    return at;
}

static int ph_compile_jp(const char *text, Phrase *p, uint32_t deflen, int *err_at)
{
    LyUnit units[PH_SYL];
    uint32_t n, at;
    int rc = ly_compile(text, (uint16_t)deflen, units, PH_SYL, &n, &at);
    if (rc) {
        if (err_at) *err_at = rc == LY_CAPACITY ? ph_jp_at(text, PH_SYL) : (int)at;
        return rc;
    }
    for (uint32_t i = 0; i < n; i++) {
        if (units[i].ticks > 127) {
            if (err_at) *err_at = ph_jp_at(text, i);
            return LY_DURATION;
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        const LyUnit *u = &units[i];
        PhSyl s;
        ph_clear(&s, sizeof s);
        s.ph[0] = u->mora;
        if (u->flags & LY_REST) s.spell[0] = u->flags & LY_PHRASE_END ? '|' : '/';
        else if (u->flags & LY_SUSTAIN) s.spell[0] = '_';
        else {
            ph_spell(&s, mora_str(u->mora));
            if (u->flags & LY_SEPARATE) {
                uint32_t j = 0;
                while (s.spell[j]) j++;
                s.spell[j] = '!';
            }
        }
        s.len = (uint8_t)(u->ticks | (u->flags & LY_WORD_END ? PH_WORD_END : 0));
        ph_copy(&p->s[i], &s, sizeof s);
    }
    return (int)n;
}

int ph_compile(const char *text, uint32_t lang, Phrase *p, int *err_at)
{
    uint32_t len = 0, deflen;
    int n;
    if (err_at) *err_at = 0;
    if (!p || !text || lang > PH_JP) return -1;
    while (text[len]) {
        if (len >= PH_TEXT || !ph_printable(text[len])) {
            if (err_at) *err_at = (int)len;
            return -1;
        }
        len++;
    }
    deflen = p->deflen >= 1 && p->deflen <= 16 ? p->deflen : 1u;
    n = lang == PH_EN ? ph_compile_en(text, p, err_at) : ph_compile_jp(text, p, deflen, err_at);
    if (n < 0) return n;
    ph_copy(p->text, text, len);
    ph_clear(p->text + len, PH_TEXT - len);
    ph_clear(p->s + n, (PH_SYL - (uint32_t)n) * sizeof p->s[0]);
    p->lang = (uint8_t)lang;
    p->n = (uint8_t)n;
    p->deflen = (uint8_t)deflen;
    p->flags = 0;
    if (err_at) *err_at = -1;
    return 0;
}

void ph_fvsyl(const PhSyl *s, FvSyl *out)
{
    ph_copy(out->on, s->ph, 3);
    out->v = s->ph[3];
    ph_copy(out->co, s->ph + 4, 4);
}

static int ph_same_syl(const PhSyl *s, const uint8_t *ph, const char *spell)
{
    for (uint32_t i = 0; i < 8; i++) if (s->ph[i] != ph[i]) return 0;
    for (uint32_t i = 0; i < 6; i++) if (s->spell[i] != spell[i]) return 0;
    return 1;
}

static int ph_table_fits(const Song *g, const Phrase *p)
{
    uint32_t needed = 0;
    for (uint32_t i = 0; i < p->n; i++) {
        const PhSyl *s = &p->s[i];
        uint32_t j = 0;
        while (j < i && !ph_same_syl(s, p->s[j].ph, p->s[j].spell)) j++;
        if (j < i) continue;
        for (j = 0; j < g->nsyl; j++) {
            const SgSyl *old = &g->syl[j];
            if (!old->text[6] && !old->text[7] && ph_same_syl(s, old->ph, old->text)) break;
        }
        if (j == g->nsyl && ++needed + g->nsyl > SG_NSYL) return 0;
    }
    return 1;
}

static int ph_separate(const PhSyl *s)
{
    for (uint32_t i = 0; i < 5 && s->spell[i]; i++) if (s->spell[i] == '!') return 1;
    return 0;
}

int ph_place(Song *g, uint32_t first_pat, uint32_t npat, const Phrase *p,
             uint8_t fallback_note, uint32_t *used)
{
    Step steps[PH_SYL];
    uint32_t offsets[PH_SYL], at = 0, count;
    if (!g || !p || first_pat >= NPAT || npat > NPAT - first_pat || !fallback_note ||
        fallback_note > 127 || g->nsyl > SG_NSYL ||
        !ph_header_ok(p->lang, p->bpm, p->n, p->deflen, p->flags)) return -1;
    for (uint32_t i = 0; i < p->n; i++) {
        const PhSyl *s = &p->s[i];
        if (!ph_syl_ok((const uint8_t *)s, p->lang)) return -1;
    }
    for (uint32_t i = 0; i < p->n; i++) {
        const PhSyl *s = &p->s[i];
        uint32_t len = s->len & 127u;
        uint32_t flags = s->len & PH_WORD_END ? VF_WORD_END : 0;
        uint32_t note = 0;
        int jp = p->lang == PH_JP, rest = jp && !s->ph[0];
        if (!len) len = p->deflen;
        if (len > 64 || at + len > npat * NSTEP) return -1;
        if (i + 1u == p->n || (rest && s->spell[0] == '|')) flags |= VF_PHRASE_END;
        if (rest) flags |= VF_WORD_END;
        else {
            note = p->flags & PH_MELODY ? s->note : 0;
            if (!note) note = g->pat[TR_VOCAL][first_pat + at / NSTEP][at % NSTEP].v[SV_NOTE];
            if (!note) note = fallback_note;
            if (note > 127) return -1;
            flags |= 12;
            if (jp && s->spell[0] == '_' && i && (steps[i-1].v[SV_FLAG] & VF_LEGATO) &&
                mora_vowel(steps[i-1].v[SV_MORA]) == mora_vowel(s->ph[0]) &&
                mora_vowel(s->ph[0]) != MV_N) flags |= VF_SUSTAIN;
            if (jp && i + 1u < p->n && p->s[i+1].ph[0] &&
                !ph_separate(&p->s[i+1])) flags |= VF_LEGATO;
        }
        steps[i] = (Step){{(uint8_t)note, (uint8_t)len, jp ? s->ph[0] : 0, (uint8_t)flags}};
        offsets[i] = at;
        at += len;
    }
    if (p->lang == PH_EN && !ph_table_fits(g, p)) return -1;
    count = (at + NSTEP - 1u) / NSTEP;
    for (uint32_t i = 0; i < count; i++) sg_clear_pat(g, TR_VOCAL, (int)(first_pat + i));
    for (uint32_t i = 0; i < p->n; i++) {
        if (p->lang == PH_EN)
            steps[i].v[SV_MORA] = (uint8_t)sg_syl_add(g, p->s[i].ph, p->s[i].spell);
        sg_set_step(g, TR_VOCAL, (int)(first_pat + offsets[i] / NSTEP),
                    (int)(offsets[i] % NSTEP), &steps[i]);
    }
    if (used) *used = count;
    return 0;
}

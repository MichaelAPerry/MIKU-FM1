/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jameson Hodge (CHIMERA, https://github.com/jamesondh/chimera-vocal-synth)
 * C port for the M-VAVE FM-1: Copyright (c) 2026 Shan-Sherif.
 * Adapted for Miku-FM: integer-only English text-to-phoneme and syllabification. */
#include "english.h"
#include "english_dict.h"

static const char ph_names[][3] = {
    "", "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH", "IY", "OW", "OY", "UH", "UW", "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K", "L", "M", "N", "NG", "P", "R", "S", "SH", "T", "TH", "V", "W", "Y", "Z", "ZH"
};

#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define WORD_MAX 47
#define WORD_PHONES (WORD_MAX * 2 + 1)
_Static_assert(sizeof(EnSyl) == 8, "song syllable format");

static int length(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int contains(const char *set, int c)
{
    while (*set)
        if (*set++ == c)
            return 1;
    return 0;
}

static int compare(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

const char *en_ph_name(uint8_t ph)
{
    return ph < EP_COUNT ? ph_names[ph] : ph_names[0];
}

int en_arpabet(const char *arpa, uint8_t *ph, uint8_t *stress, int cap)
{
    int n = 0;
    if (!arpa || cap < 0 || (cap && (!ph || !stress)))
        return EN_ERR_INPUT;
    while (*arpa) {
        const char *s = arpa;
        int len = 0, st = 0;
        while (*arpa && *arpa != ' ' && *arpa != '\t') {
            arpa++;
            len++;
        }
        if (*arpa)
            arpa++;
        if (len && s[len - 1] >= '0' && s[len - 1] <= '2')
            st = s[--len] - '0';
        if (len < 1 || len > 2)
            continue;
        for (int p = 1; p < EP_COUNT; p++) {
            if (s[0] != ph_names[p][0] || (len == 2 ? s[1] : 0) != ph_names[p][1])
                continue;
            if (n == cap)
                return EN_ERR_CAP;
            ph[n] = (uint8_t)p;
            stress[n++] = (uint8_t)(EP_IS_VOWEL(p) ? st : 0);
            break;
        }
    }
    return n;
}

typedef struct {
    char pre[2], grapheme[5], post[3];
    uint8_t phones[3];
} LtsRule;

/* Device additions take priority. Within each table, preserve CHIMERA's rule order. */
static const LtsRule device_rules[] = {
    {"", "all", "", {EP_AO, EP_L}},
    {"w", "or", "C", {EP_ER}},
    {"", "oo", "k", {EP_UH}},
    {"", "mb", "#", {EP_M}},
    {"", "nk", "", {EP_NG, EP_K}},
    {"", "ng", "s#", {EP_NG}},
    {"", "ow", "s#", {EP_OW}},
    {"t", "ed", "#", {EP_IH, EP_D}},
    {"d", "ed", "#", {EP_IH, EP_D}},
    {"p", "ed", "#", {EP_T}},
    {"k", "ed", "#", {EP_T}},
    {"s", "ed", "#", {EP_T}},
    {"h", "ed", "#", {EP_T}},
    {"f", "ed", "#", {EP_T}},
    {"b", "s", "#", {EP_Z}},
    {"d", "s", "#", {EP_Z}},
    {"g", "s", "#", {EP_Z}},
    {"l", "s", "#", {EP_Z}},
    {"m", "s", "#", {EP_Z}},
    {"n", "s", "#", {EP_Z}},
    {"r", "s", "#", {EP_Z}},
    {"v", "s", "#", {EP_Z}},
    {"w", "s", "#", {EP_Z}},
    {"", "ll", "", {EP_L}},
    {"", "tt", "", {EP_T}},
    {"", "ss", "", {EP_S}},
    {"", "pp", "", {EP_P}},
    {"", "ff", "", {EP_F}},
    {"", "mm", "", {EP_M}},
    {"", "nn", "", {EP_N}},
    {"", "rr", "", {EP_R}},
    {"", "dd", "", {EP_D}},
    {"", "bb", "", {EP_B}},
    {"", "gg", "", {EP_G}},
    {"", "zz", "", {EP_Z}},
    {"", "cc", "e", {EP_K, EP_S}},
    {"", "cc", "i", {EP_K, EP_S}},
    {"", "cc", "", {EP_K}},
};

static const LtsRule rules[] = {
    {"", "tion", "", {EP_SH, EP_AH, EP_N}},
    {"", "sion", "", {EP_ZH, EP_AH, EP_N}},
    {"", "ture", "", {EP_CH, EP_ER}},
    {"", "ough", "#", {EP_OW}},
    {"", "augh", "", {EP_AO}},
    {"", "igh", "", {EP_AY}},
    {"", "eigh", "", {EP_EY}},
    {"", "tch", "", {EP_CH}},
    {"", "dge", "", {EP_JH}},
    {"", "sch", "", {EP_S, EP_K}},
    {"", "ch", "", {EP_CH}},
    {"", "sh", "", {EP_SH}},
    {"", "th", "", {EP_TH}},
    {"", "ph", "", {EP_F}},
    {"", "wh", "", {EP_W}},
    {"", "ck", "", {EP_K}},
    {"", "ng", "#", {EP_NG}},
    {"", "ng", "", {EP_NG, EP_G}},
    {"", "qu", "", {EP_K, EP_W}},
    {"#", "kn", "", {EP_N}},
    {"#", "wr", "", {EP_R}},
    {"", "gh", "", {0}},
    {"", "ee", "", {EP_IY}},
    {"", "ea", "", {EP_IY}},
    {"", "oo", "", {EP_UW}},
    {"", "ou", "", {EP_AW}},
    {"", "ow", "#", {EP_OW}},
    {"", "ow", "", {EP_AW}},
    {"", "oi", "", {EP_OY}},
    {"", "oy", "", {EP_OY}},
    {"", "ai", "", {EP_EY}},
    {"", "ay", "", {EP_EY}},
    {"", "au", "", {EP_AO}},
    {"", "aw", "", {EP_AO}},
    {"", "ie", "#", {EP_AY}},
    {"", "ie", "", {EP_IY}},
    {"", "ei", "", {EP_EY}},
    {"", "ey", "#", {EP_IY}},
    {"", "oa", "", {EP_OW}},
    {"", "ue", "#", {EP_UW}},
    {"", "er", "#", {EP_ER}},
    {"", "er", "C", {EP_ER}},
    {"", "ir", "", {EP_ER}},
    {"", "ur", "", {EP_ER}},
    {"", "or", "", {EP_AO, EP_R}},
    {"", "ar", "", {EP_AA, EP_R}},
    {"", "le", "#", {EP_AH, EP_L}},
    {"", "ed", "#", {EP_D}},
    {"", "es", "#", {EP_Z}},
    {"C", "s", "#", {EP_S}},
    {"V", "s", "#", {EP_Z}},
    {"", "y", "#", {EP_IY}},
    {"#", "y", "", {EP_Y}},
    {"C", "y", "", {EP_AY}},
    {"", "e", "#", {0}},
    {"", "a", "CV", {EP_EY}},
    {"", "a", "", {EP_AE}},
    {"", "e", "", {EP_EH}},
    {"", "i", "CV", {EP_AY}},
    {"", "i", "", {EP_IH}},
    {"", "o", "CV", {EP_OW}},
    {"", "o", "", {EP_AA}},
    {"", "u", "CV", {EP_UW}},
    {"", "u", "", {EP_AH}},
    {"", "y", "", {EP_IH}},
    {"", "c", "e", {EP_S}},
    {"", "c", "i", {EP_S}},
    {"", "c", "y", {EP_S}},
    {"", "c", "", {EP_K}},
    {"", "g", "e", {EP_JH}},
    {"", "g", "i", {EP_JH}},
    {"", "g", "", {EP_G}},
    {"", "x", "", {EP_K, EP_S}},
    {"", "j", "", {EP_JH}},
    {"", "q", "", {EP_K}},
    {"", "b", "", {EP_B}},
    {"", "d", "", {EP_D}},
    {"", "f", "", {EP_F}},
    {"", "h", "", {EP_HH}},
    {"", "k", "", {EP_K}},
    {"", "l", "", {EP_L}},
    {"", "m", "", {EP_M}},
    {"", "n", "", {EP_N}},
    {"", "p", "", {EP_P}},
    {"", "r", "", {EP_R}},
    {"", "s", "", {EP_S}},
    {"", "t", "", {EP_T}},
    {"", "v", "", {EP_V}},
    {"", "w", "", {EP_W}},
    {"", "z", "", {EP_Z}},
};

static int letter_vowel(int c)
{
    return contains("aeiouy", lower(c));
}

static int match_context(const char *w, int len, int pos, const char *ctx, int dir)
{
    int n = length(ctx);
    for (int k = 0; k < n; k++, pos += dir) {
        int c = dir > 0 ? ctx[k] : ctx[n - k - 1];
        int ch = pos >= 0 && pos < len ? (unsigned char)w[pos] : 0;
        if (c == '#' ? ch != 0 : c == 'V' ? !letter_vowel(ch) :
            c == 'C' ? !ch || letter_vowel(ch) : ch != c)
            return 0;
    }
    return 1;
}

static const LtsRule *match_rule(const LtsRule *rules, int count, const char *w,
                                 int len, int i, int allow_ed, int *width)
{
    for (int r = 0; r < count; r++) {
        const LtsRule *rule = &rules[r];
        int k = 0;
        while (rule->grapheme[k] && w[i + k] == rule->grapheme[k])
            k++;
        if (rule->grapheme[k] || (!allow_ed && !compare(rule->grapheme, "ed")))
            continue;
        if (match_context(w, len, i - 1, rule->pre, -1) &&
            match_context(w, len, i + k, rule->post, 1)) {
            *width = k;
            return rule;
        }
    }
    return 0;
}

static int letter_to_sound(const char *w, uint8_t *ph, uint8_t *stress, int cap)
{
    int len = length(w), n = 0, stressed = 0, vowel_before = 0;
    for (int i = 0; i < len;) {
        int width = 1;
        const LtsRule *rule = match_rule(device_rules, COUNT(device_rules), w, len, i, vowel_before, &width);
        if (!rule)
            rule = match_rule(rules, COUNT(rules), w, len, i, 1, &width);
        if (rule) {
            for (int j = 0; j < COUNT(rule->phones) && rule->phones[j]; j++) {
                int p = rule->phones[j];
                if (n == cap)
                    return EN_ERR_CAP;
                ph[n] = (uint8_t)p;
                stress[n++] = (uint8_t)(EP_IS_VOWEL(p) && !stressed);
                stressed |= EP_IS_VOWEL(p);
            }
        }
        for (int j = 0; j < width; j++)
            vowel_before |= letter_vowel(w[i + j]);
        i += width;
    }
    if (n >= 2 && len >= 2 && w[len - 1] == 's' && ph[n - 1] == EP_Z) {
        int prev = ph[n - 2];
        switch (prev) {
        case EP_P: case EP_T: case EP_K: case EP_F: case EP_TH:
            ph[n - 1] = EP_S;
            break;
        case EP_S: case EP_Z: case EP_SH: case EP_ZH: case EP_CH: case EP_JH:
            if (len >= 3 && w[len - 2] == 'e') {
                if (n == cap)
                    return EN_ERR_CAP;
                ph[n - 1] = EP_IH;
                stress[n - 1] = 0;
                ph[n] = EP_Z;
                stress[n++] = 0;
            }
            break;
        }
    }
    return n;
}

static const uint8_t *builtin(const char *word)
{
    int lo = 0, hi = COUNT(dict_offsets);
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        const uint8_t *entry = dict_data + dict_offsets[mid];
        int cmp = compare(word, (const char *)entry);
        if (!cmp)
            return entry + length(word) + 1;
        if (cmp < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return 0;
}

static const char *homograph(const char *word)
{
    static const char words[][6] = {"wind", "read", "live", "lead", "close", "wound"};
    static const char phones[][11] = {"W IH1 N D", "R IY1 D", "L IH1 V", "L IY1 D", "K L OW1 Z", "W UW1 N D"};
    for (int i = 0; i < COUNT(words); i++)
        if (!compare(word, words[i]))
            return phones[i];
    return 0;
}

static int word_in(const char *word, const char *list)
{
    for (int len = length(word); *list;) {
        int i = 0;
        while (word[i] && word[i] == list[i])
            i++;
        if (i == len && list[i] == ' ')
            return 1;
        while (*list++ != ' ')
            ;
    }
    return 0;
}

static void weak_form(const char *word, uint8_t *ph, uint8_t *stress, int n)
{
    static const char function_words[] =
        "a an the of to in on at by for from with as and or but nor so if is am are was were be "
        "been do does did has have had will would can could should shall may might must it its i you "
        "he she we they me him her us them my your his our their this that than then there not no ";
    int nv = 0;
    if (!word_in(word, function_words))
        return;
    for (int i = 0; i < n; i++) {
        nv += EP_IS_VOWEL(ph[i]);
        stress[i] = 0;
    }
    if (nv == 1 && !word_in(word, "you do to who not no ")) {
        for (int i = 0; i < n; i++) {
            switch (ph[i]) {
            case EP_AA: case EP_AE: case EP_AO: case EP_EH: case EP_UH: case EP_UW:
                ph[i] = EP_AH;
                break;
            }
        }
    }
}

/* Returns EN_NO_ENTRY when the compound splitter or the rules should take over. */
enum { EN_NO_ENTRY = -3 };

static int dictionary_word(const char *word, int reduce, en_lookup_fn lookup, void *user,
                           uint8_t *ph, uint8_t *stress, int cap)
{
    const char *arpa = lookup ? lookup(word, user) : 0;
    const uint8_t *entry;
    int n = 0;
    if (!arpa)
        arpa = homograph(word);
    if (arpa) {
        if (!*arpa)
            return EN_NO_ENTRY;
        n = en_arpabet(arpa, ph, stress, cap);
    } else if ((entry = builtin(word))) {
        while (*entry) {
            if (n == cap)
                return EN_ERR_CAP;
            ph[n] = *entry & 63;
            stress[n++] = *entry++ >> 6;
        }
    } else {
        return EN_NO_ENTRY;
    }
    if (n > 0 && reduce)
        weak_form(word, ph, stress, n);
    return n;
}

static void strip_tail(char *word, int len)
{
    while (len && word[len - 1] == '\'')
        len--;
    word[len] = 0;
}

int en_word(const char *word, int reduce, en_lookup_fn lookup, void *user,
            uint8_t *ph, uint8_t *stress, int cap)
{
    char clean[WORD_MAX + 1];
    int len, n;
    if (!word || cap < 0 || (cap && (!ph || !stress)))
        return EN_ERR_INPUT;
    /* Use real addresses for the compound splitter even with a NULL, zero-capacity buffer. */
    uint8_t unused;
    if (!ph)
        ph = &unused;
    if (!stress)
        stress = &unused;
    while (*word == '\'')
        word++;
    len = length(word);
    while (len && word[len - 1] == '\'')
        len--;
    if (len > WORD_MAX)
        len = WORD_MAX;
    for (int i = 0; i < len; i++)
        clean[i] = (char)lower(word[i]);
    clean[len] = 0;
    n = dictionary_word(clean, reduce, lookup, user, ph, stress, cap);
    if (n != EN_NO_ENTRY)
        return n;
    if (!contains(clean, '-'))
        return letter_to_sound(clean, ph, stress, cap);

    n = 0;
    for (char *part = clean; part;) {
        char *end = part;
        int count;
        while (*end && *end != '-')
            end++;
        char *next = *end ? end + 1 : 0;
        strip_tail(part, (int)(end - part));
        while (*part == '\'')
            part++;
        count = dictionary_word(part, reduce, lookup, user, ph + n, stress + n, cap - n);
        if (count == EN_NO_ENTRY)
            count = letter_to_sound(part, ph + n, stress + n, cap - n);
        if (count < 0)
            return count;
        n += count;
        part = next;
    }
    return n;
}

#define PAIR(a, b) ((a) * EP_COUNT + (b))
#define TRIPLE(a, b, c) (PAIR(a, b) * EP_COUNT + (c))

static int legal_onset(const uint8_t *p, int n)
{
    static const uint16_t pairs[] = {
        PAIR(EP_P, EP_L), PAIR(EP_B, EP_L), PAIR(EP_K, EP_L), PAIR(EP_G, EP_L),
        PAIR(EP_F, EP_L), PAIR(EP_S, EP_L),
        PAIR(EP_P, EP_R), PAIR(EP_B, EP_R), PAIR(EP_T, EP_R), PAIR(EP_D, EP_R),
        PAIR(EP_K, EP_R), PAIR(EP_G, EP_R), PAIR(EP_F, EP_R), PAIR(EP_TH, EP_R),
        PAIR(EP_SH, EP_R), PAIR(EP_S, EP_P), PAIR(EP_S, EP_T), PAIR(EP_S, EP_K),
        PAIR(EP_S, EP_M), PAIR(EP_S, EP_N), PAIR(EP_S, EP_W), PAIR(EP_S, EP_F),
        PAIR(EP_T, EP_W), PAIR(EP_K, EP_W), PAIR(EP_D, EP_W), PAIR(EP_G, EP_W),
        PAIR(EP_TH, EP_W), PAIR(EP_P, EP_Y), PAIR(EP_B, EP_Y), PAIR(EP_K, EP_Y),
        PAIR(EP_G, EP_Y), PAIR(EP_M, EP_Y), PAIR(EP_N, EP_Y), PAIR(EP_F, EP_Y),
        PAIR(EP_V, EP_Y), PAIR(EP_HH, EP_Y), PAIR(EP_L, EP_Y)
    };
    static const uint16_t triples[] = {
        TRIPLE(EP_S, EP_P, EP_L), TRIPLE(EP_S, EP_P, EP_R), TRIPLE(EP_S, EP_T, EP_R),
        TRIPLE(EP_S, EP_K, EP_R), TRIPLE(EP_S, EP_K, EP_W), TRIPLE(EP_S, EP_K, EP_Y),
        TRIPLE(EP_S, EP_P, EP_Y)
    };
    if (n == 1)
        return p[0] != EP_NG;
    if (n == 2) {
        for (int i = 0; i < COUNT(pairs); i++)
            if (pairs[i] == PAIR(p[0], p[1]))
                return 1;
    }
    if (n == 3) {
        for (int i = 0; i < COUNT(triples); i++)
            if (triples[i] == TRIPLE(p[0], p[1], p[2]))
                return 1;
    }
    return 0;
}

static void spelling_chunk(char dst[8], const char *start, const char *end)
{
    int n = 0;
    while (start < end && n < 7)
        dst[n++] = *start++;
    dst[n] = 0;
}

/* CHIMERA's vowel-group spelling heuristic, keeping the final groups together
 * if there are more written vowel groups than phonetic syllables. */
typedef struct {
    const char *start, *end, *vowel;
    int groups;
} EnSpelling;

static void en_spelling_init(EnSpelling *sp, const char *word, int n)
{
    const char *end = word + length(word);
    const char *p = word;
    int groups = 0;
    sp->start = word;
    sp->end = end;
    sp->vowel = word;
    sp->groups = 0;
    if (n == 1) return;
    while (*p) {
        if (!letter_vowel(*p++))
            continue;
        groups++;
        while (letter_vowel(*p))
            p++;
    }
    if (groups > 1 && end - word >= 2 && lower(end[-1]) == 'e' && !letter_vowel(end[-2])) {
        int consonant_le = end - word >= 3 && lower(end[-2]) == 'l' &&
            !letter_vowel(end[-3]) && lower(end[-3]) != 'l';
        groups -= !consonant_le;
    }
    sp->groups = groups;
    if (groups >= n)
        while (!letter_vowel(*sp->vowel))
            sp->vowel++;
}

static void en_spelling_next(EnSpelling *sp, char text[8], int i, int n)
{
    if (n == 1 || (sp->groups < n && !i) || (sp->groups >= n && i == n - 1)) {
        spelling_chunk(text, sp->start, sp->end);
    } else if (sp->groups < n) {
        text[0] = '~';
    } else {
        const char *p = sp->vowel;
        while (letter_vowel(*p))
            p++;
        const char *gap = p;
        while (!letter_vowel(*p))
            p++;
        int width = (int)(p - gap);
        const char *cut = gap + width / 2;
        if (width >= 2) {
            int a = lower(p[-2]), b = lower(p[-1]);
            if ((b == 'h' && contains("tscpwg", a)) || (a == 'c' && b == 'k'))
                cut = p - 2;
        }
        spelling_chunk(text, sp->start, cut);
        sp->start = cut;
        sp->vowel = p;
    }
}

/* The device compiler lowers an aggregate assignment here to libc memcpy. */
static void en_syl_copy(EnSylOut *out, const EnSylOut *s)
{
    volatile uint8_t *d = (volatile uint8_t *)out;
    const uint8_t *src = (const uint8_t *)s;
    for (unsigned i = 0; i < sizeof *s; i++) d[i] = src[i];
}

static int en_syllabify_emit(const uint8_t *ph, const uint8_t *stress, int n,
                             const char *spelling, EnSylOut *out, int cap,
                             en_emit_fn emit, void *user, int base)
{
    EnSpelling split;
    int count = 0, v = 0, start = 0;
    if (!spelling || n < 0 || cap < 0 || (n && !ph))
        return EN_ERR_INPUT;
    for (int i = 0; i < n; i++) {
        if (!ph[i] || ph[i] >= EP_COUNT)
            return EN_ERR_INPUT;
        count += EP_IS_VOWEL(ph[i]);
    }
    if (!n)
        return 0;
    if (!cap || count > cap)
        return EN_ERR_CAP;
    if (!out && !emit) return count ? count : 1;
    if (!count) {
        /* With no vowel, use both fields in reading order. */
        EnSylOut syl = {{{0}, 0, {0}}, {0}, 0, 0};
        int i = 0;
        for (; i < n && i < COUNT(syl.s.on); i++)
            syl.s.on[i] = ph[i];
        for (int j = 0; i < n && j < COUNT(syl.s.co); j++, i++)
            syl.s.co[j] = ph[i];
        syl.flags = EN_WORD_END | (i < n ? EN_TRUNCATED : 0);
        spelling_chunk(syl.text, spelling, spelling + length(spelling));
        if (out) en_syl_copy(&out[0], &syl);
        else emit(user, base, &syl);
        return 1;
    }
    en_spelling_init(&split, spelling, count);
    while (!EP_IS_VOWEL(ph[v]))
        v++;
    for (int i = 0; i < count; i++) {
        EnSylOut syl = {{{0}, 0, {0}}, {0}, 0, 0};
        int next = n, end = n;
        int onset = v - start;
        if (onset > COUNT(syl.s.on)) {
            start = v - COUNT(syl.s.on);
            onset = COUNT(syl.s.on);
            syl.flags |= EN_TRUNCATED;
        }
        for (int j = 0; j < onset; j++)
            syl.s.on[j] = ph[start + j];
        syl.s.v = ph[v];
        syl.stress = stress ? stress[v] : 0;
        next = v + 1;
        while (next < n && !EP_IS_VOWEL(ph[next]))
            next++;
        end = next;
        if (next < n) {
            /* Longest legal suffix of the cluster goes into the next onset,
             * including any coda overflow it can legally accept. */
            int width = next - v - 1;
            if (width > COUNT(syl.s.on))
                width = COUNT(syl.s.on);
            for (; width; width--)
                if (legal_onset(ph + next - width, width)) {
                    end -= width;
                    break;
                }
        }
        for (int j = v + 1; j < end; j++) {
            if (j - v > COUNT(syl.s.co)) {
                syl.flags |= EN_TRUNCATED;
                break;
            }
            syl.s.co[j - v - 1] = ph[j];
        }
        en_spelling_next(&split, syl.text, i, count);
        if (i == count - 1) syl.flags |= EN_WORD_END;
        if (out) en_syl_copy(&out[i], &syl);
        else emit(user, base + i, &syl);
        start = end;
        v = next;
    }
    return count;
}

int en_syllabify(const uint8_t *ph, const uint8_t *stress, int n, const char *spelling,
                EnSylOut *out, int cap)
{
    if (cap && !out) return EN_ERR_INPUT;
    return en_syllabify_emit(ph, stress, n, spelling, out, cap, 0, 0, 0);
}

typedef struct {
    EnSylOut *out;
    int cap, n;
    en_lookup_fn lookup;
    void *user;
    int reduce;                 /* weak forms of function words (speech); 0 when sung */
    en_emit_fn emit;
    void *emit_user;
} TextOutput;

static int emit_word(TextOutput *o, const char *word)
{
    uint8_t ph[WORD_PHONES], stress[WORD_PHONES];
    int n = en_word(word, o->reduce, o->lookup, o->user, ph, stress, WORD_PHONES);
    if (n <= 0)
        return n;
    n = en_syllabify_emit(ph, stress, n, word, o->out ? o->out + o->n : 0,
                          o->cap - o->n, o->emit, o->emit_user, o->n);
    if (n < 0)
        return n;
    o->n += n;
    return 0;
}

static const char ones[][10] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
    "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
};
static const char tens[][8] = {
    "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"
};

static int emit_under_thousand(TextOutput *o, unsigned value)
{
    int result;
    if (value >= 100) {
        result = emit_word(o, ones[value / 100]);
        if (result < 0)
            return result;
        result = emit_word(o, "hundred");
        if (result < 0)
            return result;
        value %= 100;
        if (!value)
            return 0;
    }
    if (value >= 20) {
        result = emit_word(o, tens[value / 10 - 2]);
        if (result < 0)
            return result;
        value %= 10;
        if (!value)
            return 0;
    }
    return emit_word(o, ones[value]);
}

static int digit(int c)
{
    return c >= '0' && c <= '9';
}

static int emit_number(TextOutput *o, const char *start, int len)
{
    unsigned value = 0;
    int result;
    if (len > 6) {
        for (int i = 0; i < len; i++) {
            result = emit_word(o, ones[start[i] - '0']);
            if (result < 0)
                return result;
        }
        return 0;
    }
    for (int i = 0; i < len; i++)
        value = value * 10 + (unsigned)(start[i] - '0');
    if (value >= 1000) {
        result = emit_under_thousand(o, value / 1000);
        if (result < 0)
            return result;
        result = emit_word(o, "thousand");
        if (result < 0)
            return result;
        value %= 1000;
        if (!value)
            return 0;
    }
    return emit_under_thousand(o, value);
}

static int word_byte(int c)
{
    int lc = lower(c);
    return (lc >= 'a' && lc <= 'z') || c == '\'' || c == '-';
}

int en_text(const char *text, en_lookup_fn lookup, void *user,
            EnSylOut *out, int cap, int *n, int *err_at)
{
    return en_text_ex(text, EN_SPEECH, lookup, user, out, cap, n, err_at);
}

static int en_text_walk(const char *text, TextOutput *o, int *n, int *err_at)
{
    int i = 0, start = 0, result = 0;
    if (err_at)
        *err_at = -1;
    if (n)
        *n = 0;
    if (!text || !n || o->cap < 0)
        return EN_ERR_INPUT;
    while (text[i]) {
        start = i;
        if (digit(text[i])) {
            do {
                i++;
            } while (digit(text[i]));
            result = emit_number(o, text + start, i - start);
        } else if (word_byte(text[i])) {
            char word[WORD_MAX + 1];
            int len = 0;
            do {
                if (len < WORD_MAX)
                    word[len++] = text[i];
                i++;
            } while (word_byte(text[i]));
            word[len] = 0;
            result = emit_word(o, word);
        } else {
            if (o->out && o->n && contains(".!?,;:", text[i]))
                o->out[o->n - 1].flags |= EN_PHRASE_END;
            i++;
        }
        if (result < 0)
            break;
    }
    *n = o->n;
    if (result < 0) {
        if (err_at)
            *err_at = start;
    } else if (o->out && o->n) {
        o->out[o->n - 1].flags |= EN_PHRASE_END;
    }
    return result;
}

int en_text_ex(const char *text, int mode, en_lookup_fn lookup, void *user,
               EnSylOut *out, int cap, int *n, int *err_at)
{
    TextOutput o = {out, (cap && !out) ? -1 : cap, 0, lookup, user, mode == EN_SPEECH, 0, 0};
    return en_text_walk(text, &o, n, err_at);
}

int en_text_ex_emit(const char *text, int mode, en_lookup_fn lookup, void *user,
                    en_emit_fn emit, void *emit_user, int cap, int *n, int *err_at)
{
    TextOutput o = {0, cap, 0, lookup, user, mode == EN_SPEECH, emit, emit_user};
    return en_text_walk(text, &o, n, err_at);
}

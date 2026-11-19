/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/english.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)
#define COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static void token(const char **expected, const char *name, int stress)
{
    char field[8];
    int len = stress >= 0 ? snprintf(field, sizeof field, "%s%d", name, stress)
                          : snprintf(field, sizeof field, "%s", name);
    if (**expected == ' ')
        ++*expected;
    if (strncmp(*expected, field, (size_t)len) ||
        ((*expected)[len] && (*expected)[len] != ' ')) {
        fprintf(stderr, "Expected remainder [%s], produced [%s]\n", *expected, field);
        exit(1);
    }
    *expected += len;
}

static void word_golden(const char *word, const char *expected)
{
    uint8_t ph[256], stress[256];
    int n = en_word(word, 0, NULL, NULL, ph, stress, COUNT(ph));
    CHECK(n >= 0);
    for (int i = 0; i < n; i++) {
        token(&expected, en_ph_name(ph[i]), EP_IS_VOWEL(ph[i]) ? stress[i] : -1);
        if (!EP_IS_VOWEL(ph[i]))
            CHECK(stress[i] == 0);
    }
    CHECK(!*expected);
}

/* Intentional differences from the two reference paths:
 * - The 39-device-code inventory folds schwa/r-colored schwa and dark L into
 *   AH/ER/L. Original T/D survive flapping. For the running-speech comparison
 *   only, compute DX where the reference would flap; check T and D separately.
 * - Caller lookup wins over homographs. Long clusters signal truncation
 *   (3 onset, 4 coda); vowel-less input keeps its phones with no nucleus.
 * - Running speech uses number expansion, weak forms and compound word
 *   boundaries. CHIMERA's separate lyric parser uses strong forms, splits
 *   explicit hyphens and supports standalone melisma markers.
 * - Spelling is limited to seven bytes per syllable; the digit expansion is
 *   streamed, so input can run past CHIMERA's 511-byte expanded text ceiling.
 * - Capacity failures are explicit and never write beyond the buffers.
 */
static void text_golden(const char *text, const char *expected)
{
    EnSylOut out[256];
    uint8_t ph[512], stress[512], flags[512];
    int n, err, len = 0;
    CHECK(en_text(text, NULL, NULL, out, COUNT(out), &n, &err) == 0 && err == -1);
    for (int i = 0; i < n; i++) {
        uint8_t phones[] = {out[i].s.on[0], out[i].s.on[1], out[i].s.on[2], out[i].s.v,
                            out[i].s.co[0], out[i].s.co[1], out[i].s.co[2], out[i].s.co[3]};
        CHECK(!(out[i].flags & EN_TRUNCATED));
        for (int j = 0; j < COUNT(phones); j++) {
            if (!phones[j])
                continue;
            CHECK(len < COUNT(ph));
            ph[len] = phones[j];
            stress[len] = j == 3 ? out[i].stress : 0;
            flags[len++] = 0;
        }
        CHECK(len > 0);
        flags[len - 1] = out[i].flags;
    }
    for (int i = 0; i < len; i++) {
        int p = ph[i];
        int flap = (p == EP_T || p == EP_D) && i > 0 && i + 1 < len &&
            (EP_IS_VOWEL(ph[i - 1]) || ph[i - 1] == EP_R) && !(flags[i - 1] & EN_WORD_END) &&
            !(flags[i] & EN_PHRASE_END) && EP_IS_VOWEL(ph[i + 1]) && !stress[i + 1];
        token(&expected, flap ? "DX" : en_ph_name((uint8_t)p), EP_IS_VOWEL(p) ? stress[i] : -1);
        if (flags[i] & EN_PHRASE_END) {
            CHECK(flags[i] & EN_WORD_END);
            token(&expected, "/", -1);
        } else if (flags[i] & EN_WORD_END) {
            token(&expected, "|", -1);
        }
    }
    CHECK(!*expected);
}

static void golden_file(const char *path)
{
    FILE *file = fopen(path, "r");
    char line[2048];
    int words = 0, sentences = 0;
    CHECK(file);
    while (fgets(line, sizeof line, file)) {
        if (line[0] == '#')
            continue;
        CHECK(line[1] == '\t');
        char *input = line + 2;
        char *expected = strchr(input, '\t');
        CHECK(expected);
        *expected++ = 0;
        expected[strcspn(expected, "\r\n")] = 0;
        if (line[0] == 'W') {
            word_golden(input, expected);
            words++;
        } else {
            CHECK(line[0] == 'T');
            text_golden(input, expected);
            sentences++;
        }
    }
    CHECK(!ferror(file));
    fclose(file);
    CHECK(words == 1012 && sentences == 30);
    printf("Checked %d reference words and %d reference sentences\n", words, sentences);
}

static void hand_syllables(void)
{
    static const struct {
        const char *word;
        int n;
        EnSyl syl[4];
        const char *text[4];
    } cases[] = {
        {"hello", 2, {{{EP_HH}, EP_AH, {0}}, {{EP_L}, EP_OW, {0}}}, {"hel", "lo"}},
        {"string", 1, {{{EP_S, EP_T, EP_R}, EP_IH, {EP_NG}}}, {"string"}},
        {"extra", 2, {{{0}, EP_EH, {EP_K}}, {{EP_S, EP_T, EP_R}, EP_AE, {0}}}, {"ex", "tra"}},
        {"monster", 2, {{{EP_M}, EP_AA, {EP_N}}, {{EP_S, EP_T}, EP_ER, {0}}}, {"mon", "ster"}},
        {"beautiful", 3, {{{EP_B, EP_Y}, EP_UW, {0}}, {{EP_T}, EP_AH, {0}}, {{EP_F}, EP_AH, {EP_L}}},
         {"beau", "ti", "ful"}},
        {"world", 1, {{{EP_W}, EP_ER, {EP_L, EP_D}}}, {"world"}},
        {"I", 1, {{{0}, EP_AY, {0}}}, {"I"}},
        {"a", 1, {{{0}, EP_AH, {0}}}, {"a"}},
        {"strengths", 1, {{{EP_S, EP_T, EP_R}, EP_EH, {EP_NG, EP_G, EP_TH, EP_S}}}, {"strengt"}},
        {"everything", 3, {{{0}, EP_EH, {EP_V}}, {{EP_R}, EP_IY, {0}}, {{EP_TH}, EP_IH, {EP_NG}}},
         {"e", "ve", "rything"}},
        {"Miku", 2, {{{EP_M}, EP_AY, {0}}, {{EP_K}, EP_AH, {0}}}, {"Mi", "ku"}},
        {"singing", 2, {{{EP_S}, EP_IH, {EP_NG}}, {{EP_G}, EP_IH, {EP_NG}}}, {"sin", "ging"}},
    };
    for (int i = 0; i < COUNT(cases); i++) {
        uint8_t p[32], st[32];
        EnSylOut out[4];
        int np = en_word(cases[i].word, 0, NULL, NULL, p, st, COUNT(p));
        CHECK(en_syllabify(p, st, np, cases[i].word, out, COUNT(out)) == cases[i].n);
        for (int j = 0; j < cases[i].n; j++) {
            if (strcmp(out[j].text, cases[i].text[j]))
                fprintf(stderr, "%s [%d]: %s\n", cases[i].word, j, out[j].text);
            CHECK(!memcmp(&out[j].s, &cases[i].syl[j], sizeof(EnSyl)));
            CHECK(!strcmp(out[j].text, cases[i].text[j]));
            CHECK(out[j].flags == (j + 1 == cases[i].n ? EN_WORD_END : 0));
        }
    }
}

static const char *override(const char *word, void *user)
{
    (*(int *)user)++;
    if (!strcmp(word, "read"))
        return "R EH1 D";
    if (!strcmp(word, "miku"))
        return "M IY1 K UW0";
    return NULL;
}

static void robustness(void)
{
    uint8_t p[8], st[8];
    EnSylOut out[101], before[101];
    char long_text[201];
    int n, at, calls = 0;
    memset(out, 0x5a, sizeof out);
    memcpy(before, out, sizeof out);
    CHECK(en_text("", NULL, NULL, out, 0, &n, &at) == 0 && n == 0 && at == -1);
    CHECK(en_text("@#_\xf0\x9f\x8e\xb5", NULL, NULL, out, 1, &n, &at) == 0 && n == 0);
    CHECK(!memcmp(out, before, sizeof out));
    CHECK(en_word("'READ'", 0, override, &calls, p, st, COUNT(p)) == 3);
    CHECK(calls == 1 && p[1] == EP_EH && st[1] == 1);
    CHECK(en_word("hello", 0, override, &calls, p, st, COUNT(p)) == 4 && calls == 2);
    CHECK(en_text("Miku", override, &calls, out, 2, &n, &at) == 0 && n == 2);
    CHECK(out[0].s.v == EP_IY && out[1].s.v == EP_UW);
    CHECK(en_text("water rider", NULL, NULL, out, 4, &n, &at) == 0 && n == 4);
    CHECK(out[1].s.on[0] == EP_T && out[3].s.on[0] == EP_D);
    memset(p, 0xa5, sizeof p);
    memset(st, 0xa5, sizeof st);
    CHECK(en_arpabet("? HH AH0 oo L1 OW2", p, st, 2) == EN_ERR_CAP);
    CHECK(p[0] == EP_HH && p[1] == EP_AH && st[1] == 0 && p[2] == 0xa5 && st[2] == 0xa5);
    CHECK(en_word("hello", 0, NULL, NULL, p, st, 2) == EN_ERR_CAP && p[2] == 0xa5 && st[2] == 0xa5);
    CHECK(en_arpabet("HH AH0 L1 OW2", p, st, COUNT(p)) == 4 && st[2] == 0 && st[3] == 2);
    memcpy(out, before, sizeof out);
    CHECK(en_syllabify(p, st, 4, "hello", out, 1) == EN_ERR_CAP);
    CHECK(!memcmp(out, before, sizeof out));
    p[3] = EP_COUNT;
    CHECK(en_syllabify(p, st, 4, "hello", out, 2) == EN_ERR_INPUT);
    CHECK(!memcmp(out, before, sizeof out));
    CHECK(en_text("a hello", NULL, NULL, out, 2, &n, &at) == EN_ERR_CAP && n == 1 && at == 2);
    CHECK(!memcmp(out + 1, before + 1, sizeof out - sizeof *out));
    CHECK(en_text("7", NULL, NULL, NULL, 0, &n, &at) == EN_ERR_CAP && n == 0 && at == 0);
    CHECK(en_text("21!", NULL, NULL, out, 3, &n, &at) == 0 && n == 3);
    CHECK(out[1].flags == EN_WORD_END && out[2].flags == (EN_WORD_END | EN_PHRASE_END));
    CHECK(en_text(" 21", NULL, NULL, out, 2, &n, &at) == EN_ERR_CAP && n == 2 && at == 1);
    for (int i = 0; i < 200; i++)
        long_text[i] = i % 2 ? ' ' : 'a';
    long_text[200] = 0;
    CHECK(en_text(long_text, NULL, NULL, out, 100, &n, &at) == 0 && n == 100);
    CHECK(!memcmp(out + 100, before + 100, sizeof *out));
    uint8_t crowded[] = {EP_F, EP_S, EP_T, EP_R, EP_AE, EP_N, EP_D, EP_Z, EP_T, EP_S, EP_T, EP_R, EP_AH};
    CHECK(en_syllabify(crowded, NULL, COUNT(crowded), "test", out, 2) == 2);
    CHECK(out[0].flags == EN_TRUNCATED && out[0].s.on[0] == EP_S && out[0].s.co[3] == EP_T);
    CHECK(out[1].flags == EN_WORD_END && out[1].s.on[0] == EP_S && out[1].s.on[2] == EP_R);
    CHECK(en_syllabify(crowded + 4, NULL, 6, "test", out, 1) == 1);
    CHECK(out[0].flags == (EN_WORD_END | EN_TRUNCATED) && out[0].s.co[3] == EP_T);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    golden_file(argv[1]);
    hand_syllables();
    robustness();
    return 0;
}

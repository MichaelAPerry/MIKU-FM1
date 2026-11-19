/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jameson Hodge (CHIMERA, https://github.com/jamesondh/chimera-vocal-synth)
 * C port for the M-VAVE FM-1: Copyright (c) 2026 Shan-Sherif.
 * Adapted for Miku-FM: record the C reference pronunciations for host tests. */
/* From the repository root, regenerate the fixture with the reference checkout:
 * cc -std=c11 -O2 -ffunction-sections -fdata-sections -Ibuild/ref/chimera/engine \
 *   tools/english_golden.c build/ref/chimera/engine/ch_speech.c \
 *   -Wl,--gc-sections -lm -o build/english_golden
 * build/english_golden tools/english_words.txt > firmware/song/tests/english_golden.txt
 */
#include "ch_speech.h"
#include <stdio.h>
#include <string.h>

static const char *const names[] = {
    "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER", "EY", "IH", "IY", "OW", "OY", "UH", "UW",
    "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K", "L", "M", "N", "NG", "P", "R", "S", "SH",
    "T", "TH", "V", "W", "Y", "Z", "ZH", "AH", "ER", "L", "DX"
};

static const char *const ipa[] = {
    "ɑ", "æ", "ʌ", "ɔ", "aʊ", "aɪ", "ɛ", "ɝ", "eɪ", "ɪ", "i", "oʊ", "ɔɪ", "ʊ",
    "u", "b", "tʃ", "d", "ð", "f", "g", "h", "dʒ", "k", "l", "m", "n", "ŋ",
    "p", "ɹ", "s", "ʃ", "t", "θ", "v", "w", "j", "z", "ʒ", "ə", "ɚ", "ɫ",
    "ɾ",
};

static int print_row(char kind, const char *input)
{
    ch_seg_t segs[512];
    ch_phrase_t phrases[128];
    int n, nphr = 0, phr = 0;
    if (kind == 'W')
        n = ch_word(input, 0, NULL, NULL, segs, 512);
    else
        nphr = ch_text_to_phrases(input, NULL, NULL, segs, 512, &n, phrases, 128);
    printf("%c\t%s\t", kind, input);
    for (int i = 0; i < n; i++) {
        unsigned p = 0;
        while (p < sizeof ipa / sizeof ipa[0] && strcmp(ipa[p], segs[i].ipa))
            p++;
        if (p == sizeof ipa / sizeof ipa[0]) {
            fprintf(stderr, "Unmapped IPA %s in %s\n", segs[i].ipa, input);
            return 1;
        }
        if (i)
            putchar(' ');
        fputs(names[p], stdout);
        if (p < 15 || p == 39 || p == 40)
            printf("%d", segs[i].stress);
        if (phr < nphr && i == phrases[phr].first + phrases[phr].n - 1) {
            fputs(" /", stdout);
            phr++;
        } else if (segs[i].word_end) {
            fputs(" |", stdout);
        }
    }
    putchar('\n');
    return 0;
}

int main(int argc, char **argv)
{
    char line[512];
    FILE *list = argc == 2 ? fopen(argv[1], "r") : NULL;
    if (!list) {
        fputs("Usage: english_golden tools/english_words.txt\n", stderr);
        return 1;
    }
    ch_lts_device = 1;
    puts("# CHIMERA reference, FM-1 device rules enabled");
    puts("# CHIMERA C port copyright (c) 2026 Shan-Sherif and upstream copyright (c) 2026 Jameson Hodge (MIT)");
    puts("# CMUdict copyright (C) 1993-2015 Carnegie Mellon University (BSD-2-Clause, LICENSES/CMUdict.txt)");
    for (int i = 0; i < ch_dict_count(); i++)
        if (print_row('W', ch_dict_word(i)))
            return 1;
    while (fgets(line, sizeof line, list)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#')
            continue;
        if (line[1] != '\t' || (line[0] != 'W' && line[0] != 'T') || !line[2]) {
            fprintf(stderr, "Invalid corpus row: %s\n", line);
            return 1;
        }
        if (print_row(line[0], line + 2))
            return 1;
    }
    int failed = ferror(list) || ferror(stdout);
    fclose(list);
    return failed;
}

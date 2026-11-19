/* SPDX-License-Identifier: GPL-3.0-only */
#include "firmware/song/phrase.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    uint8_t record[PH_REC];
    Phrase p = {.bpm = 120, .deflen = 4, .name = "            "};
    if (argc == 2 && !strcmp(argv[1], "decode")) {
        for (unsigned i = 0; i < PH_REC; i++) {
            unsigned value;
            if (scanf("%2x", &value) != 1) return 1;
            record[i] = (uint8_t)value;
        }
        if (ph_decode(record, &p)) return 1;
    } else if (ph_compile("Hello world, I sing for you", PH_EN, &p, 0)) return 1;
    if (ph_encode(&p, record)) return 1;
    for (unsigned i = 0; i < PH_REC; i++) printf("%02x", (unsigned)record[i]);
    return 0;
}

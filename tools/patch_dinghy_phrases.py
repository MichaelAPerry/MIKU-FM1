#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Open Miku-FM's phrase bank area for writing on pinned Dinghy (FM-1 only).

The bank (docs/ENGLISH_SINGING.md, section 3b) is 64 records of 1 KiB in the 64 KiB right after the ten song slots:
0xBF000..0xCF000, inside Felucca's main store FL_DATA (0x97000..0xE0000), clear of the OTA staging area and every
protected range. Dinghy's FM-1 FL_RANGE_OK allows only its own settings, blobs, slots and the OTA area, so this adds the
phrase window to it (native/dinghy.c pins the layout with static assertions against the flash map). Run after patch_dinghy_slots.py (it expects 10 slots).
Exact-match replacement: refuses to run if the pinned text changed."""
from pathlib import Path
import sys

OLD = '''#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \\
                             FL_IN(off, n, BLOBS_AT, BLOBS_AT + NBLOB * 0x1000u) || \\
                             FL_IN(off, n, SLOTS_AT, SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u))
#endif'''
NEW = '''#define PHRASES_AT 0x000BF000u                  /* Miku-FM phrase bank: 64 x 1 KiB after the ten song slots */
#define PHRASES_LEN (16u * 0x1000u)             /* 16 sectors */
#define FL_RANGE_OK(off, n) (FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_IN(off, n, SETTINGS_AT, SETTINGS_AT + 0x1000u) || \\
                             FL_IN(off, n, BLOBS_AT, BLOBS_AT + NBLOB * 0x1000u) || \\
                             FL_IN(off, n, SLOTS_AT, SLOTS_AT + NSLOT * SLOT_SECT * 0x1000u) || \\
                             FL_IN(off, n, PHRASES_AT, PHRASES_AT + PHRASES_LEN))
#endif'''


def patch(root):
    p = Path(root) / 'firmware/src/store.h'
    s = p.read_text()
    if s.count(OLD) != 1:
        raise ValueError('pinned Dinghy FM-1 FL_RANGE_OK changed')
    if '#define NSLOT 10u' not in s:
        raise ValueError('run patch_dinghy_slots.py first (10 song slots)')
    p.write_text(s.replace(OLD, NEW))


if __name__ == '__main__':
    patch(sys.argv[1])

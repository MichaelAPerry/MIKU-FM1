#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Let the app receive Yamaha DX7 SysEx (bulk dump / single voice) over USB MIDI on pinned Dinghy.

Dinghy's USB layer assembles one SysEx frame at a time into sx_frame[] (the 7-bit bytes between F0 and F7), and
ota_service() -> ota_take() decodes it as an M-UPGRADE message and discards it otherwise. This patch
  1. enlarges sx_frame from 640 to 4104 bytes: a full 32-voice bulk dump is 43 0n 09 20 00 + 4096 + checksum = 4102 bytes;
  2. in ota_service(), before ota_take(), offers a frame whose first byte is 0x43 (Yamaha) to the app's
     `static void app_sysex(const uint8_t *p, uint32_t n)` and releases it. M-UPGRADE frames never start with 0x43: their
     first decoded byte is 0x00, so their first 7-bit group is 0x00. Frames are not offered in recovery mode, and the update
     loader (which also compiles usb.c and ota.c, with FELUCCA_LOADER set) keeps the 640-byte buffer and has no hook.
Everything else (M-UPGRADE handshake, ota_session / ota_read, the soft-key and UBOOT paths, usb.c's SysEx scanner) is unchanged.
Exact-match replacement: refuses to run if the pinned text changed."""
from pathlib import Path
import sys

SX_OLD = 'static uint8_t sx_frame[640];'
SX_NEW = '''#ifdef FELUCCA_LOADER
static uint8_t sx_frame[640];                           /* the update loader keeps its small buffer */
#else
static uint8_t sx_frame[4104];                          /* the app: a DX7 32-voice bulk dump is 4102 bytes between F0 and F7 (was 640) */
#endif'''
OTA_OLD = '''static void ota_service(void)
{
    uint32_t d = ota_take();
'''
OTA_NEW = '''#ifndef FELUCCA_LOADER
static void app_sysex(const uint8_t *p, uint32_t n);   /* the app's (native/dinghy.c): a Yamaha (0x43) SysEx frame, main loop */
#endif
static void ota_service(void)
{
    uint32_t d;
#ifndef FELUCCA_LOADER
    {
        const uint8_t *fp;
        uint32_t fn;
        if (!recovery_active && ota_frame_get(&fp, &fn) && fn && fp[0] == 0x43u) {   /* never an M-UPGRADE frame (those start 0x00) */
            app_sysex(fp, fn);
            ota_frame_done();
            return;
        }
    }
#endif
    d = ota_take();
'''

def swap(path, old, new, counts=(1,)):
    s = path.read_text()
    if s.count(old) not in counts:
        raise ValueError(f'pinned Dinghy text changed: {path.name}: {old.splitlines()[0]!r}')
    path.write_text(s.replace(old, new))

def patch(root):
    src = Path(root) / 'firmware/src'
    # pristine usb.c has the buffer once; after tools/patch_dinghy_video.py (which keeps the pinned file as its #else branch,
    # so run this after it) both branches carry it
    swap(src / 'usb.c', SX_OLD, SX_NEW, (1, 2))
    swap(src / 'ota.c', OTA_OLD, OTA_NEW)

if __name__ == '__main__':
    patch(sys.argv[1])

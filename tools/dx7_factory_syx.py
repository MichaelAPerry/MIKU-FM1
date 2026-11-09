#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Write site/media/dx7/miku-fm-factory.syx: the Miku-FM factory bank as a standard DX7 32-voice bulk dump (for Dexed).

Source of truth: assets/dx7/factory.packed (the ten original voices). The bank is padded to 32 with INIT VOICE, the same
INIT VOICE as dxs_init_voice() in firmware/song/dx7_sysex.c (tests/test_bank_tool.py compares the two). The message is
F0 43 00 09 20 00 <4096 bytes> <checksum> F7 (channel 1), as the device's own export.

  tools/dx7_factory_syx.py           write the file
  tools/dx7_factory_syx.py --check   fail if the committed file is stale
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DST = ROOT / "site/media/dx7/miku-fm-factory.syx"


def init_voice():
    p = bytearray(128)
    for o in range(6):
        q = 17 * o
        p[q:q + 4] = bytes([99] * 4)
        p[q + 4:q + 7] = bytes([99] * 3)
        p[q + 7] = 0
        p[q + 8] = 39
        p[q + 12] = 7 << 3
        p[q + 14] = 99 if o == 5 else 0
        p[q + 15] = 1 << 1
    for i in range(4):
        p[102 + i] = 99
        p[106 + i] = 50
    p[111] = 8
    p[112] = 35
    p[116] = 1 | 3 << 4
    p[117] = 24
    p[118:128] = b"INIT VOICE"
    return bytes(p)


def checksum(data):
    return (-sum(data)) & 127


def bulk_dump(voices, channel=0):
    if len(voices) > 32 or any(len(v) != 128 for v in voices):
        raise ValueError("at most 32 voices of 128 bytes")
    bank = b"".join(voices) + init_voice() * (32 - len(voices))
    assert all(b < 128 for b in bank)
    return bytes([0xF0, 0x43, channel & 15, 0x09, 0x20, 0x00]) + bank + bytes([checksum(bank), 0xF7])


def generate():
    raw = (ROOT / "assets/dx7/factory.packed").read_bytes()
    if not raw or len(raw) % 128:
        raise ValueError("factory.packed must be a multiple of 128 bytes")
    return bulk_dump([raw[i:i + 128] for i in range(0, len(raw), 128)])


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    out = generate()
    if a.check:
        if not DST.exists() or DST.read_bytes() != out:
            raise SystemExit("site/media/dx7/miku-fm-factory.syx is stale: run tools/dx7_factory_syx.py")
    else:
        DST.parent.mkdir(parents=True, exist_ok=True)
        DST.write_bytes(out)
        print(f"wrote {DST.relative_to(ROOT)} ({len(out)} bytes)")

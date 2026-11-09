#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The ten original DX7 factory voices (two per GEN style), authored for MIKU-FM1.

Every value below was written for this project (no third-party cartridge data), so the voices
ship under the project licence (GPL-3.0-only). This file is the single source of truth: it writes
assets/dx7/factory.packed (10 x 128-byte DX7 packed voices, bank index = list order), which
tools/gen_dx7_bank.py turns into firmware/audio/dx7/bank.h. Order matters: GEN style s uses
factory voice 2*s for CHORDS and 2*s+1 for BASS.
    python3 tools/dx7_candidates.py [--check]
"""
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / 'assets/dx7/factory.packed'

def op(r=(99, 50, 35, 50), l=(99, 80, 0, 0), ol=0, ratio=1, fine=0, det=7, kvs=2, rs=2,
       bp=39, ld=0, rd=0, lc=0, rc=0, ams=0, fixed=0):
    """One operator, Yamaha meaning. ratio = coarse (0 = x0.5)."""
    return [*r, *l, bp, ld, rd, lc | rc << 2, det << 3 | rs, kvs << 2 | ams, ol, ratio << 1 | fixed, fine]

def voice(name, alg, ops, fb=0, sync=1, lfo=(35, 0, 0, 0, 0, 0), pms=0,
          pr=(99, 99, 99, 99), pl=(50, 50, 50, 50), transpose=24):
    """ops: OP1..OP6 (Yamaha order); packed data stores OP6 first."""
    assert len(ops) == 6
    p = []
    for o in reversed(ops):
        p += o
    speed, delay, pmd, amd, lsync, wave = lfo
    p += [*pr, *pl, alg - 1, sync << 3 | fb, speed, delay, pmd, amd, pms << 4 | wave << 1 | lsync, transpose]
    p += [ord(c) for c in name.ljust(10)[:10]]
    assert len(p) == 128 and all(0 <= v <= 127 for v in p), name
    return p

Z = op()  # silent placeholder operator (output level 0)

V = [
    # ---- HYPERPOP: glassy, over-bright and a bit unstable
    ('HYPERPOP', 'chords', voice('HYPER GLAS', 5, [
        op(r=(99, 45, 30, 45), l=(99, 85, 70, 0), ol=99, ratio=1, det=9, kvs=2),
        op(r=(99, 60, 30, 50), l=(99, 70, 50, 0), ol=80, ratio=3, fine=50, kvs=4),
        op(r=(99, 45, 30, 45), l=(99, 85, 70, 0), ol=92, ratio=2, det=5, kvs=2),
        op(r=(99, 70, 40, 50), l=(99, 60, 40, 0), ol=74, ratio=7, kvs=5),
        op(r=(99, 40, 30, 45), l=(99, 80, 65, 0), ol=84, ratio=1, det=4),
        op(r=(99, 50, 30, 50), l=(99, 75, 60, 0), ol=60, ratio=1, det=10),
    ], fb=5, lfo=(40, 20, 6, 0, 0, 4), pms=3)),
    ('HYPERPOP', 'bass', voice('HYPER 808', 2, [
        op(r=(99, 30, 25, 45), l=(99, 88, 80, 0), ol=99, ratio=1, kvs=1, rs=1),
        op(r=(99, 75, 40, 60), l=(99, 40, 20, 0), ol=58, ratio=1, kvs=3),
        op(r=(99, 30, 25, 45), l=(99, 88, 80, 0), ol=70, ratio=0, kvs=1),
        op(r=(99, 80, 50, 60), l=(80, 30, 0, 0), ol=50, ratio=2),
        Z, Z,
    ], fb=0, pr=(99, 62, 99, 99), pl=(64, 50, 50, 50))),
    # ---- FUTURE BASS: wide detuned supersaw-like chords
    ('FUTURE BASS', 'chords', voice('FUTURE SAW', 22, [
        op(r=(80, 40, 30, 55), l=(99, 92, 88, 0), ol=99, ratio=1, det=4),
        op(r=(80, 40, 30, 55), l=(99, 90, 85, 0), ol=82, ratio=1, det=10),
        op(r=(80, 40, 30, 55), l=(99, 92, 88, 0), ol=96, ratio=1, det=10),
        op(r=(80, 40, 30, 55), l=(99, 92, 88, 0), ol=96, ratio=1, det=7),
        op(r=(80, 40, 30, 55), l=(99, 92, 88, 0), ol=96, ratio=2, det=3),
        op(r=(80, 40, 30, 55), l=(99, 90, 85, 0), ol=80, ratio=1, det=7),
    ], fb=7, lfo=(30, 0, 0, 0, 0, 4), pms=2)),
    ('FUTURE BASS', 'bass', voice('FUTURE SUB', 1, [
        op(r=(99, 40, 30, 55), l=(99, 95, 92, 0), ol=99, ratio=1, kvs=1),
        op(r=(99, 55, 30, 55), l=(99, 60, 45, 0), ol=40, ratio=1, kvs=3),
        Z, Z, Z, Z,
    ])),
    # ---- JERSEY CLUB: short, punchy stabs and a bouncy bass
    ('JERSEY CLUB', 'chords', voice('CLUB STAB', 5, [
        op(r=(99, 62, 50, 70), l=(99, 40, 0, 0), ol=99, ratio=1, det=8, kvs=3),
        op(r=(99, 70, 50, 70), l=(99, 35, 0, 0), ol=78, ratio=1, kvs=5),
        op(r=(99, 62, 50, 70), l=(99, 40, 0, 0), ol=94, ratio=2, det=6, kvs=3),
        op(r=(99, 72, 50, 70), l=(99, 30, 0, 0), ol=70, ratio=3, kvs=5),
        op(r=(99, 60, 50, 70), l=(99, 45, 0, 0), ol=80, ratio=1, det=5),
        op(r=(99, 70, 50, 70), l=(99, 35, 0, 0), ol=68, ratio=1),
    ], fb=6)),
    ('JERSEY CLUB', 'bass', voice('CLUB BOUNC', 1, [
        op(r=(99, 55, 40, 65), l=(99, 70, 50, 0), ol=99, ratio=1, kvs=2),
        op(r=(99, 72, 45, 65), l=(99, 45, 25, 0), ol=72, ratio=1, kvs=4),
        Z, Z, Z, Z,
    ], fb=0, pr=(99, 70, 99, 99), pl=(58, 50, 50, 50))),
    # ---- DIGICORE: bitty, bright plucks and a gritty growl
    ('DIGICORE', 'chords', voice('DIGI PLUCK', 9, [
        op(r=(99, 58, 40, 60), l=(99, 50, 0, 0), ol=99, ratio=1, kvs=3),
        op(r=(99, 75, 45, 60), l=(99, 40, 0, 0), ol=80, ratio=5, kvs=5),
        op(r=(99, 58, 40, 60), l=(99, 50, 0, 0), ol=90, ratio=1, det=9, kvs=3),
        op(r=(99, 80, 50, 60), l=(99, 30, 0, 0), ol=74, ratio=11, kvs=5),
        op(r=(99, 82, 50, 60), l=(99, 25, 0, 0), ol=62, ratio=17),
        Z,
    ], fb=0, lfo=(60, 0, 0, 0, 0, 5), pms=1)),
    ('DIGICORE', 'bass', voice('DIGI GROWL', 5, [
        op(r=(99, 45, 35, 60), l=(99, 90, 85, 0), ol=96, ratio=0, kvs=2),
        op(r=(99, 60, 40, 60), l=(99, 55, 40, 0), ol=55, ratio=1, kvs=3),
        Z, Z,
        op(r=(99, 45, 35, 60), l=(99, 85, 75, 0), ol=92, ratio=1, kvs=2),
        op(r=(90, 50, 35, 60), l=(99, 80, 70, 0), ol=78, ratio=0, kvs=3),
    ], fb=7, lfo=(55, 0, 0, 12, 0, 0))),
    # ---- IDOL POP: bright classic keys and a round finger bass
    ('IDOL POP', 'chords', voice('IDOL KEYS', 5, [
        op(r=(95, 35, 25, 50), l=(99, 80, 0, 0), ol=99, ratio=1, kvs=3, rs=3, bp=48, rd=20, rc=0),
        op(r=(95, 70, 35, 50), l=(99, 55, 0, 0), ol=60, ratio=14, kvs=7, rs=3),
        op(r=(95, 35, 25, 50), l=(99, 80, 0, 0), ol=94, ratio=1, det=9, kvs=3, rs=3),
        op(r=(95, 50, 35, 50), l=(99, 60, 0, 0), ol=72, ratio=1, kvs=6, rs=3),
        Z, Z,
    ], lfo=(34, 30, 4, 0, 0, 4), pms=2)),
    ('IDOL POP', 'bass', voice('IDOL BASS', 5, [
        op(r=(99, 40, 30, 60), l=(99, 75, 0, 0), ol=99, ratio=1, kvs=3),
        op(r=(99, 65, 35, 60), l=(99, 50, 0, 0), ol=64, ratio=1, kvs=5),
        Z, Z,
        op(r=(99, 45, 30, 60), l=(99, 70, 0, 0), ol=70, ratio=1, kvs=3),
        op(r=(99, 70, 40, 60), l=(99, 45, 0, 0), ol=55, ratio=2, kvs=5),
    ], fb=3)),
]

def main():
    data = bytes(b for _, _, v in V for b in v)
    if '--check' in sys.argv:
        ok = OUT.exists() and OUT.read_bytes() == data
        print('factory.packed up to date' if ok else 'factory.packed differs; rerun'); sys.exit(0 if ok else 1)
    OUT.write_bytes(data)
    for i, (style, role, v) in enumerate(V):
        print(f'{i}: {bytes(v[118:]).decode()!r:13} {style:12} {role:6} alg {v[110]+1}')

if __name__ == '__main__':
    main()

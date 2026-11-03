#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Offline constants for firmware/audio/fvoice (integer-only on the device).
# Usage: tools/gen_fvoice_tables.py [out.h]   (default firmware/audio/fvoice/fvoice_tables.h)
import math, os, sys

FS = 44100
BLK = 16                    # frames per control block (fvoice.c FV_BLK)
BT = BLK / FS               # block time, s
# Six parallel 2-pole resonators per vowel, fixed slot order (alternating sign + - + - + -):
#   F1, NAS (nasal body between F1 and F2), F2, F3, SING (singer's formant/presence), AIR (5-7 kHz sheen)
# Young, small vocal tract: formants ~12% above an adult table; Japanese i/e keep F2 high, u is unrounded.
BW = (90, 180, 110, 160, 350, 800)
VOW = {  # name: (F1, NAS, F2, F3, SING, AIR) Hz, peak dB of each, loudness trim dB
    "A": ((1010, 1250, 1570, 3300, 3100, 5800), (0.0, -9.0, -3.0, -6.0, -4.0, -12.0), 0.0),
    "I": ((370, 1000, 2900, 3700, 3200, 6000), (-4.0, -12.0, 0.0, -6.0, -4.0, -11.0), 0.0),
    "U": ((400, 850, 1650, 3150, 3000, 5600), (0.0, -13.0, -4.0, -8.0, -5.0, -13.0), 0.0),
    "E": ((620, 1100, 2450, 3400, 3150, 5800), (-3.0, -10.0, 0.0, -6.0, -4.0, -12.0), 0.0),
    "O": ((560, 780, 1000, 3300, 3100, 5600), (0.0, -9.0, -4.0, -8.0, -5.0, -13.0), 0.0),
}
TRIM = {"A": -1.9, "I": 0.7, "U": 1.3, "E": 1.1, "O": -1.1}   # loudness match, calibrated against tests/test_fvoice level print
# loudness vs pitch: dB correction at these MIDI notes (linear in between, flat outside), calibrated likewise
PG = [(36, 0.0), (48, 0.0), (60, 0.0), (72, 0.5), (84, -0.5), (96, 6.0)]
ZB = {"A": -3.0, "I": 1.0, "U": 6.0, "E": 0.0, "O": 1.0}   # soprano-zone loudness trim (F1 near f0 changes the level)
TILT = 0.8
MASTER = 9.0                # source->output scale; peak ~0.5 FS at velocity 127
F0HI = 940.0                # high-pitch zone: F1 is pulled up toward f0 (soprano behaviour)
for _kv in os.environ.get("FV_CFG", "").split(";"):
    if "=" in _kv:
        _k, _v = _kv.split("=")
        if _k == "TRIM": TRIM.update(dict(zip("AIUEO", map(float, _v.split(",")))))
        elif _k == "PG": PG[:] = list(zip([36, 48, 60, 72, 84, 96], map(float, _v.split(","))))
        else: globals()[_k] = float(_v)

def res(f, bw):
    r = math.exp(-math.pi * bw / FS)
    return 2 * r * math.cos(2 * math.pi * f / FS), -r * r

def peak(a1, a2):           # max |1/(1-a1 z^-1-a2 z^-2)| on a fine grid
    m = 0.0
    for i in range(1, 4096):
        w = math.pi * i / 4096
        re = 1 - a1 * math.cos(w) - a2 * math.cos(2 * w)
        im = a1 * math.sin(w) + a2 * math.sin(2 * w)
        m = max(m, 1 / math.hypot(re, im))
    return m

def q14(x):
    return int(round(x * 16384))

def arr(name, ty, vals, per=12, fmt="%d"):
    s = "static const %s %s[%d]={\n" % (ty, name, len(vals))
    for i in range(0, len(vals), per):
        s += " " + ",".join(fmt % v for v in vals[i:i + per]) + ",\n"
    return s + "};\n"

def main():
    out = []
    # glottal flow derivative, Rosenberg-C pulse: open 0.40, sharp close 0.07 of the period (bright)
    N = 256
    def glottal(tp, tn, rms=None):
        def flow(t):
            t %= 1.0
            if t < tp: return 0.5 * (1 - math.cos(math.pi * t / tp))
            if t < tp + tn: return math.cos(0.5 * math.pi * (t - tp) / tn)
            return 0.0
        d = [flow((i + 1) / N) - flow(i / N) for i in range(N)]
        mean = sum(d) / N
        d = [x - mean for x in d]
        if rms is None: sc = 32767 / max(abs(x) for x in d)          # sharp pulse: peak = full scale
        else: sc = rms / math.sqrt(sum(x * x for x in d) / N)        # soft pulse: same RMS as the sharp one
        r = [int(round(sc * x)) for x in d]
        assert max(abs(x) for x in r) <= 32767
        return r + [r[0]]
    sharp = glottal(0.40, float(os.environ.get("FV_TN", "0.07")))
    out.append(arr("fv_glot", "int16_t", sharp, 16))
    # soft, nearly band-limited pulse crossfaded in above ~C5 so high notes do not alias
    out.append(arr("fv_glot2", "int16_t", glottal(0.50, float(os.environ.get("FV_TN2", "0.32")), math.sqrt(sum(x * x for x in sharp[:N]) / N)), 16))
    out.append(arr("fv_sin", "int16_t", [int(round(32767 * math.sin(2 * math.pi * i / 256))) for i in range(256)], 16))
    # phase increment per sample for MIDI note 0 (8.1758 Hz) * 2^(i/256), 257 entries (octave 0)
    f0 = 440.0 * 2 ** (-69 / 12)
    out.append(arr("fv_inc", "uint32_t", [int(round(f0 / FS * 2 ** 32 * 2 ** (i / 256))) for i in range(257)], 8))
    out.append(arr("fv_notep", "uint16_t", [int(round(n * 4096 / 12)) for n in range(128)], 16))
    # per-block rates indexed by ms>>1 (0..255 -> 0..510 ms)
    atk = [min(11800, int(round(32767 * BT / (max(i * 2, 1) / 1000)))) for i in range(256)]
    rel = [int(round(65536 * (1 - math.exp(-BT / (max(i * 2, 1) / 1000 / math.log(100)))))) for i in range(256)]   # -40 dB in ms
    por = [32767 if i == 0 else int(round(32768 * (1 - math.exp(-BT / (i * 2 / 1000 / 3.5))))) for i in range(256)]
    out.append(arr("fv_katk", "uint16_t", atk, 16))
    out.append(arr("fv_krel", "uint16_t", [min(x, 65535) for x in rel], 16))
    out.append(arr("fv_kpor", "uint16_t", [min(x, 32767) for x in por], 16))
    # vowel resonators: {g,a1,a2} Q14 x 6 per vowel and zone; y=(g*x+a1*y1+a2*y2)>>14;
    # g carries the sign (+ - + - + -) and the peak-normalised amplitude, so each peak is its dB value.
    # zone 0 = normal; zone 1 = soprano (F1 pulled toward f0, F2 kept above it), blended per block by pitch
    names = "AIUEO"
    s = "static const int16_t fv_vow[2][5][6][3]={\n"
    for zone in (0, 1):
        s += "{\n"
        for n in names:
            fs, db, _ = VOW[n]
            fs = list(fs)
            if zone:
                fs[0] = max(fs[0], fs[0] + 0.55 * (F0HI - fs[0]))
                fs[1] = max(fs[1], fs[0] * 1.25)
                fs[2] = max(fs[2], fs[0] + 450)
            s += " {"
            for i in range(6):
                a1, a2 = res(fs[i], BW[i])
                a = db[i] + TILT * math.log2(max(fs[i], 500) / 500)    # undo the source's residual tilt
                g = 10 ** ((a + TRIM[n] + (ZB[n] if zone else 0.0)) / 20) * MASTER / peak(a1, a2) * (1 if i % 2 == 0 else -1)
                assert abs(g) < 1.99, (n, i, g)
                s += "{%d,%d,%d}," % (q14(g), q14(a1), q14(a2))
            s += "},\n"
        s += "},\n"
    out.append(s + "};\n")
    def pgdb(n):
        if n <= PG[0][0]: return PG[0][1]
        for (n0, d0), (n1, d1) in zip(PG, PG[1:]):
            if n <= n1: return d0 + (d1 - d0) * (n - n0) / (n1 - n0)
        return PG[-1][1]
    out.append(arr("fv_pgain", "uint16_t", [int(round(8192 * 10 ** (pgdb(n) / 20))) for n in range(128)], 16))
    km = int(round(4096 * (1 - math.exp(-BT / 0.008))))    # vowel morph: tau 8 ms (95% in 24 ms)
    lfo = int(round(2 ** 32 * BLK / FS / 10))              # LFO phase per block per 0.1 Hz
    out.append("#define FV_KM %d\n#define FV_LFOK %d\n" % (km, lfo))
    hdr = ("/* SPDX-License-Identifier: GPL-3.0-only */\n"
           "/* GENERATED by tools/gen_fvoice_tables.py - do not edit. */\n"
           "#ifndef FVOICE_TABLES_H\n#define FVOICE_TABLES_H\n#include <stdint.h>\n")
    return hdr + "".join(out) + "#endif\n"

if __name__ == "__main__":
    dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware", "audio", "fvoice", "fvoice_tables.h")
    with open(dst, "w", newline="\n") as f:
        f.write(main())

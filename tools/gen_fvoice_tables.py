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

# Measured steady RMS, machine-exact MIKU, velocity 127, bright 100, breath 0;
# notes 72..96, AIUEO. tools/voice_compare.c documents the evaluation protocol.
COMP_RMS = [[6560, 5969, 4702, 5746, 6036], [3427, 5638, 5458, 6003, 5719], [2437, 5860, 4455, 5504, 6128], [2497, 3852, 4747, 6554, 6696], [2009, 3824, 4056, 6601, 6018], [1595, 3757, 4242, 6741, 4924], [1768, 3563, 5242, 6525, 4685], [2162, 2888, 5229, 5742, 4106], [2042, 3043, 4951, 5235, 3489], [2587, 3231, 4695, 3602, 3469], [3929, 4211, 4012, 3387, 3452], [7019, 4715, 4406, 3802, 3652], [6167, 4377, 3940, 5067, 4197], [3433, 3889, 3246, 5517, 4817], [2873, 3710, 3983, 5113, 8295], [3015, 3235, 4776, 5202, 9299], [2974, 2850, 5700, 3247, 4595], [3568, 4504, 8310, 3329, 3085], [5363, 5195, 12639, 3720, 2653], [8848, 4589, 19889, 4780, 3068], [4432, 5488, 27401, 5517, 1888], [2088, 4955, 14332, 4006, 1250], [1325, 4356, 7298, 4532, 1342], [938, 3878, 4211, 5150, 1069], [614, 4109, 2547, 6180, 941]]
ZB = {"A": -3.0, "I": 1.0, "U": 6.0, "E": 0.0, "O": 1.0}   # soprano-zone loudness trim (F1 near f0 changes the level)
TILT = 0.8
MASTER = 9.0                # source->output scale; peak ~0.5 FS at velocity 127
F0HI = 940.0                # high-pitch zone: F1 is pulled up toward f0 (soprano behaviour)
CHR = (0.84, 0.92, 1.0, 1.08, 1.18)   # voice character (formant size): DEEP LOW MIKU YOUNG TINY; formants and bandwidths scale
for _kv in os.environ.get("FV_CFG", "").split(";"):
    if "=" in _kv:
        _k, _v = _kv.split("=")
        if _k == "TRIM": TRIM.update(dict(zip("AIUEO", map(float, _v.split(",")))))
        elif _k == "PG": PG[:] = list(zip([36, 48, 60, 72, 84, 96], map(float, _v.split(","))))
        else: globals()[_k] = float(_v)

# ---- Step 3: consonants (source-filter, no samples) -----------------------------------------------------------
# Extra resonator "shapes" after the five vowels (same 6 slots): voice bar, nasal n, nasal m, flap r.
# name: (freqs Hz, peak dB per slot, bandwidths Hz per slot)
SHAPES = {
    "BAR": ((250, 700, 1500, 2800, 3000, 5500), (0.0, -30.0, -38.0, -42.0, -45.0, -50.0), (100, 250, 300, 400, 500, 900)),
    "NN":  ((380, 900, 1500, 2500, 3200, 5800), (0.0, -24.0, -24.0, -28.0, -34.0, -42.0), (200, 250, 250, 300, 400, 800)),
    "NM":  ((360, 1000, 1250, 2200, 3000, 5600), (0.0, -26.0, -26.0, -32.0, -38.0, -45.0), (200, 250, 250, 300, 400, 800)),
    "FL":  ((400, 900, 1400, 2400, 3000, 5600), (0.0, -12.0, -4.0, -6.0, -8.0, -20.0), (110, 200, 160, 200, 350, 800)),
}
SHN = list("AIUEO") + ["BAR", "NN", "NM", "FL"]
SHI = {n: i for i, n in enumerate(SHN)}
SHI["V"] = 15                      # "the sung vowel"
# Noise bands (2-pole resonators on the LFSR): name: (centre Hz per vowel A I U E O, bandwidth Hz)
NBN = ["K", "T", "P", "S", "SH", "F", "H"]
NB = {
    "K":  ((1900, 3100, 1700, 2600, 1300), 900),     # velar burst: follows F2 of the next vowel
    "T":  ((4600, 4600, 4600, 4600, 4600), 1800),
    "P":  ((800, 800, 800, 800, 800), 1400),
    "S":  ((6500, 6500, 6500, 6500, 6500), 2500),
    "SH": ((3800, 3800, 3800, 3800, 3800), 2200),
    "F":  ((1800, 1800, 1800, 1800, 1800), 4000),
    "H":  ((3600, 3600, 3600, 3600, 3600), 2500, (0.0, 1.0, 0.0, 0.0, 0.0)),   # c-like friction of "hi" only
}
NASAL_DB = ((55, -5.0), (72, 7.0))   # murmur level vs note: the nasal formant sits below f0 when singing high, so the murmur is raised there
BURST_END = 0.25                    # burst into a vowel: the noise falls to this fraction of its start
NRMS = 8192                         # noise level 1.0 = this RMS at the band output
KV_TAU, KL_TAU, KN_TAU = 3.0, 1.5, 0.8   # ms: voicing, level, noise/aspiration smoothing
KM_TAU = {"std": 8.0, "fast": 5.0, "slow": 12.0}   # ms: formant morph speed
KMI = {"std": 0, "fast": 1, "slow": 2}
def P(ms, sh="V", vg=1.0, lv=1.0, nk=None, nl=0.0, asp=0.0, km="std", lip=0, bst=0):
    return (ms, sh, vg, lv, nk, nl, asp, km, lip, bst)
HOLD = 255
# MC_* order: NONE K S SH T CH TS N H F M Y R W G Z J D B P NN.  Phase: P(ms, shape, voicing, level, noise band, noise, aspiration, morph, lips closed,
# burst: the noise starts at nl and falls exponentially to the next phase's noise (BURST_END of nl if that is 0) by the phase end; a flat
# block of noise at vowel level sounds like a crackle of white noise, a release transient does not)
CONS = [
    [],
    [P(11, vg=0, lv=0), P(7, vg=0, lv=0, nk="K", nl=0.32, bst=1), P(22, vg=0, lv=0.35, nk="K", nl=0.10, asp=0.10)],             # K
    [P(48, vg=0, lv=0, nk="S", nl=0.22), P(14, vg=0.5, lv=0.6, nk="S", nl=0.05)],                                           # S
    [P(52, vg=0, lv=0, nk="SH", nl=0.26), P(14, vg=0.5, lv=0.6, nk="SH", nl=0.05)],                                         # SH
    [P(9, vg=0, lv=0), P(6, vg=0, lv=0, nk="T", nl=0.32, bst=1), P(18, vg=0, lv=0.35, nk="T", nl=0.08, asp=0.08)],                # T
    [P(9, vg=0, lv=0), P(4, vg=0, lv=0, nk="SH", nl=0.45, bst=1), P(34, vg=0, lv=0.2, nk="SH", nl=0.36), P(10, vg=0.5, lv=0.6, nk="SH", nl=0.08)],   # CH
    [P(9, vg=0, lv=0), P(4, vg=0, lv=0, nk="T", nl=0.45, bst=1), P(30, vg=0, lv=0.2, nk="S", nl=0.38), P(10, vg=0.5, lv=0.6, nk="S", nl=0.08)],      # TS
    [P(50, "NN", lv=0.38), P(60, km="fast")],                                                                                 # N
    [P(40, vg=0, lv=0.55, nk="H", nl=0.12, asp=0.22), P(12, vg=0.5, lv=0.8, asp=0.06)],                                      # H (hi also gets the c-like band)
    [P(40, vg=0, lv=0.25, nk="F", nl=0.07, asp=0.06), P(10, vg=0.5, lv=0.6)],                                                # F
    [P(55, "NM", lv=0.38, lip=1), P(60, km="fast")],                                                                          # M
    [P(10, "I", lv=0.55), P(50, km="slow")],                                                                                 # Y
    [P(16, "FL", lv=0.4, km="fast"), P(20, km="fast")],                                                                      # R
    [P(10, "U", lv=0.5), P(50, km="slow")],                                                                                  # W
    [P(24, "BAR", lv=0.22, km="fast"), P(5, lv=0.45, nk="K", nl=0.18, km="fast", bst=1)],                                           # G
    [P(40, "BAR", lv=0.5, nk="S", nl=0.15, km="fast"), P(12, lv=0.8, nk="S", nl=0.05, km="fast")],                         # Z
    [P(40, "BAR", lv=0.5, nk="SH", nl=0.17, km="fast"), P(12, lv=0.8, nk="SH", nl=0.05, km="fast")],                       # J
    [P(24, "BAR", lv=0.22, km="fast"), P(5, lv=0.45, nk="T", nl=0.18, km="fast", bst=1)],                                           # D
    [P(24, "BAR", lv=0.22, km="fast", lip=1), P(5, lv=0.45, nk="P", nl=0.18, km="fast", bst=1)],                             # B
    [P(14, vg=0, lv=0, lip=1), P(6, vg=0, lv=0, nk="P", nl=0.32, bst=1), P(16, vg=0, lv=0.35, nk="P", nl=0.06, asp=0.08)],         # P
    [P(HOLD, "NN", lv=0.55, lip=1)],                                                                                          # NN (moraic n: hum)
]

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
    s = "#define FV_NCHR %d\n#define FV_CHR0 %d\nstatic const int16_t fv_vow[FV_NCHR][2][%d][6][3]={\n" % (len(CHR), CHR.index(1.0), len(SHN))
    for ck in CHR:
     s += "{\n"
     for zone in (0, 1):
        s += "{\n"
        for n in SHN:
            if n in VOW: fs, db, _ = VOW[n]; bws = BW
            else: fs, db, bws = SHAPES[n]
            fs = [f * ck for f in fs]; bws = [b * ck for b in bws]
            if zone:
                fs[0] = max(fs[0], fs[0] + 0.55 * (F0HI - fs[0]))
                fs[1] = max(fs[1], fs[0] * 1.25)
                fs[2] = max(fs[2], fs[0] + 450)
            s += " {"
            for i in range(6):
                a1, a2 = res(fs[i], bws[i])
                a = db[i] + TILT * math.log2(max(fs[i], 500) / 500)    # undo the source's residual tilt
                g = 10 ** ((a + TRIM.get(n, 0.0) + (ZB.get(n, 0.0) if zone else 0.0)) / 20) * MASTER / peak(a1, a2) * (1 if i % 2 == 0 else -1)
                assert abs(g) < 1.99, (n, i, g)
                s += "{%d,%d,%d}," % (q14(g), q14(a1), q14(a2))
            s += "},\n"
        s += "},\n"
     s += "},\n"
    out.append(s + "};\n")
    def pgdb(n):
        if n <= PG[0][0]: return PG[0][1]
        for (n0, d0), (n1, d1) in zip(PG, PG[1:]):
            if n <= n1: return d0 + (d1 - d0) * (n - n0) / (n1 - n0)
        return PG[-1][1]
    out.append(arr("fv_pgain", "uint16_t", [int(round(8192 * 10 ** (pgdb(n) / 20))) for n in range(128)], 16))
    # noise bands: {g,a1,a2} Q14 per kind and following vowel; g normalises the band output to NRMS (white input = uniform int16)
    nb = []
    for k in NBN:
        cf, bw = NB[k][0], NB[k][1]
        lvf = NB[k][2] if len(NB[k]) > 2 else (1.0,) * 5
        for vi in range(5):
            a1, a2 = res(cf[vi], bw)
            gain = (1 - a2) / ((1 + a2) * ((1 - a2) ** 2 - a1 * a1))      # AR(2) variance gain
            g = NRMS / (32768 / math.sqrt(3) * math.sqrt(gain)) * lvf[vi]
            assert g < 1.99, (k, g)
            nb += [q14(g), q14(a1), q14(a2)]
    out.append(arr("fv_nbt", "int16_t", nb, 15))
    cph, cpn = [], []
    for ph in CONS:
        assert len(ph) <= 4
        for i in range(4):
            if i < len(ph):
                ms, sh, vg, lv, nk, nl, asp, kmn, lip, bst = ph[i]
                n = HOLD if ms == HOLD else max(1, int(round(ms / (BT * 1000))))
                assert n <= 255 and (ms != HOLD or n == 255)
                tail = sh == "V" and vg >= 1 and lv >= 1 and nk is None and not nl and not asp and ms != HOLD   # pure vowel: the consonant is over
                cph += [n, SHI[sh] | KMI[kmn] << 4 | lip << 6 | tail << 7, int(round(vg * 128)), int(round(lv * 128)), 0 if nk is None else NBN.index(nk) + 1]
                nx = ph[i + 1][5] if i + 1 < len(ph) else 0.0
                kd = 0 if not bst else int(round(32768 * (1 - ((nx if nx else BURST_END * nl) / nl) ** (1 / n))))
                assert not bst or 0 < kd < 32768
                cpn += [min(65535, int(round(nl * 32768))), min(65535, int(round(asp * 32768))), kd]
            else:
                cph += [0, 15, 128, 128, 0]; cpn += [0, 0, 0]
    out.append("#define FV_NPH 4\n#define FV_NCLS %d\n" % len(CONS))
    out.append(arr("fv_cph", "uint8_t", cph, 20))          # [class][phase]{blocks (255 = hold), shape | morph<<4 | lips<<6 | vowel-tail<<7, voicing, level (128 = 1.0), noise kind+1}
    out.append(arr("fv_cpn", "uint16_t", cpn, 12))         # [class][phase]{noise level, aspiration, burst decay per block} Q15
    def nbo(n):
        (n0, d0), (n1, d1) = NASAL_DB
        return d0 if n <= n0 else d1 if n >= n1 else d0 + (d1 - d0) * (n - n0) / (n1 - n0)
    out.append(arr("fv_nbo", "uint16_t", [int(round(4096 * 10 ** (nbo(n) / 20))) for n in range(128)], 16))   # Q12 level of the nasal shapes per note
    out.append("#define FV_NAS0 %d\n#define FV_NAS1 %d\n" % (SHI["NN"], SHI["NM"]))
    def ek(tau): return int(round(32768 * (1 - math.exp(-BT * 1000 / tau))))
    out.append("#define FV_KV %d\n#define FV_KL %d\n#define FV_KN %d\n" % (ek(KV_TAU), ek(KL_TAU), ek(KN_TAU)))
    out.append(arr("fv_kmt", "uint16_t", [int(round(4096 * (1 - math.exp(-BT * 1000 / KM_TAU[k])))) for k in ("std", "fast", "slow")], 3))
    km = int(round(4096 * (1 - math.exp(-BT / 0.008))))    # vowel morph: tau 8 ms (95% in 24 ms)
    assert km == int(round(4096 * (1 - math.exp(-BT * 1000 / KM_TAU["std"]))))
    lfo = int(round(2 ** 32 * BLK / FS / 10))              # LFO phase per block per 0.1 Hz
    out.append("#define FV_KM %d\n#define FV_LFOK %d\n" % (km, lfo))
    # Conservative maximum +6 dB; do not attenuate strong notes or boost low registers.
    comp = [min(8192, max(4096, round(4096 * COMP_RMS[0][w] / row[w]))) for row in COMP_RMS for w in range(5)]
    out.append(arr("fv_comp", "uint16_t", comp, 5))  # [note-72][vowel], Q12
    hdr = ("/* SPDX-License-Identifier: GPL-3.0-only */\n"
           "/* GENERATED by tools/gen_fvoice_tables.py - do not edit. */\n"
           "#ifndef FVOICE_TABLES_H\n#define FVOICE_TABLES_H\n#include <stdint.h>\n")
    return hdr + "".join(out) + "#endif\n"

if __name__ == "__main__":
    dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "firmware", "audio", "fvoice", "fvoice_tables.h")
    with open(dst, "w", newline="\n") as f:
        f.write(main())

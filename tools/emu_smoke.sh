#!/bin/sh
# SPDX-License-Identifier: MIT
# Boot a Miku-FM package in fm1-emu (github.com/joakimeriksson/fm1-emu), press PLAY, and check that it sounds and
# draws: audio RMS above a floor, the screen not one colour. Evidence (WAV, BMPs, log) goes to OUT_DIR.
#   tools/emu_smoke.sh PACKAGE.fwsc OUT_DIR [FM1_EMU_BIN]
set -eu
PKG="$(cd "$(dirname "${1:?usage: tools/emu_smoke.sh PACKAGE.fwsc OUT_DIR [FM1_EMU_BIN]}")" && pwd)/$(basename "$1")"
OUT="${2:?out dir}"; EMU="${3:-fm1-emu}"
case "$EMU" in */*) EMU="$(cd "$(dirname "$EMU")" && pwd)/$(basename "$EMU")";; esac   # (we cd below)
mkdir -p "$OUT/work"
cd "$OUT"
"$EMU" --fwsc "$PKG" --ms 3000 --keys '200:PLAY,1500:enc0:+2' --snap 1000,2500 --wav audio.wav > boot.log 2>&1
python3 - <<'PY'
import array, wave
from PIL import Image
with wave.open("audio.wav", "rb") as w:
    n, a = w.getnframes(), array.array("h", w.readframes(w.getnframes()))
rms = (sum(x * x for x in a) / max(len(a), 1)) ** 0.5
colours = len(Image.open("work/lcd_2500.bmp").convert("RGB").getcolors(1 << 16))
print(f"emu_smoke: {n} frames, RMS {rms:.0f}, {colours} screen colours")
assert n > 10000, "short audio"
assert rms > 10, "silent"
assert colours > 2, "blank screen"
PY

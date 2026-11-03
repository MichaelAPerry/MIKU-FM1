#!/bin/bash
# SPDX-License-Identifier: MIT
# Build the Miku-FM FM-1 firmware (GPL-3.0-only, on Dinghy).  native/build.sh [--release X.Y[.Z]]
#   JIELI_TOOLCHAIN, AC79_SDK: read by Dinghy's build.sh   DINGHY_COMMIT: Dinghy revision to build on
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DINGHY_COMMIT="${DINGHY_COMMIT:-f77bf03283bbb2c05294b01cdad68a30b410f93d}"
REL=""
[ "${1:-}" = --release ] && REL="${2:?--release X.Y[.Z]}"
[[ -z "$REL" || "$REL" =~ ^([0-9]{1,2})\.([0-9]{1,2})(\.[0-9]{1,2})?(-[A-Za-z0-9]+)?$ ]] || { echo "build.sh: --release $REL: use X.Y or X.Y.Z (0-99 each)"; exit 1; }
[ $# -eq 0 ] || [ -n "$REL" ] || { echo "usage: build.sh [--release X.Y[.Z]]"; exit 1; }
mkdir -p "$ROOT/build"
[ -d "$ROOT/build/dinghy/.git" ] || git clone --depth 1 https://github.com/zvenson/dinghy.git "$ROOT/build/dinghy"
cd "$ROOT/build/dinghy"
git reset -q --hard
git clean -qfd
git fetch --depth 1 origin "$DINGHY_COMMIT"
git checkout -q --detach "$DINGHY_COMMIT"
cp "$ROOT/native/dinghy.c" firmware/src/dinghy.c
python3 - <<'PY'
from pathlib import Path
p=Path('tools/build.py')
s=p.read_text()
def sub(a,b):
    global s
    if s.count(a)!=1: raise SystemExit(f"build.sh: patch did not match: {a}")
    s=s.replace(a,b)
sub('SCREEN_NAME = "DINGHY"','SCREEN_NAME = "MIKU-FM"')
sub('DATA_FAMILY = "DINGHY"','DATA_FAMILY = "MIKUFM"')
sub('SCREEN_COLOUR = 0xB47F','SCREEN_COLOUR = 0x067F')
sub('BOARD_PREFIX = {"fm1": "FM-1_99",','BOARD_PREFIX = {"fm1": "FM-1_39",')
# identity: FM-1_39XXYYZZ (dev: 000000), version X.Y or X.Y.Z, 0-99 each
sub('    PRODUCT = BOARD_PREFIX[BOARD] + "0"\n','    PRODUCT = BOARD_PREFIX[BOARD] + ("000000" if BOARD == "fm1" else "0")\n')
sub('r"(\\d)\\.(\\d)(\\.\\d)?(-[A-Za-z0-9]+)?"','r"(\\d{1,2})\\.(\\d{1,2})(?:\\.(\\d{1,2}))?(-[A-Za-z0-9]+)?"')
sub('        PRODUCT = BOARD_PREFIX[BOARD] + m[2]\n        VERSION = m[1] + "." + m[2] + (m[3] or "")\n',
    '        PRODUCT = BOARD_PREFIX[BOARD] + (f"{int(m[1]):02d}{int(m[2]):02d}{int(m[3] or 0):02d}" if BOARD == "fm1" else m[2])\n'
    '        VERSION = m[1] + "." + m[2] + ("." + m[3] if m[3] else "")\n')
p.write_text(s)
PY
export DINGHY_DEFS="${DINGHY_DEFS:-} -DSTORE_MAGIC=0x554B494Du"   # "MIKU": never parse Dinghy's saved data
./build.sh ${REL:+--release "$REL"}
mkdir -p "$ROOT/build/native"
V="${REL:+-${REL%%-*}}"
cp "build/dinghy${REL:+-$REL}.fwsc" "$ROOT/build/native/miku-fm$V.fwsc"
cp build/dinghy.bin "$ROOT/build/native/miku-fm$V-app.bin"

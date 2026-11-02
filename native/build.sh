#!/bin/bash
# SPDX-License-Identifier: MIT
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build"
git clone --depth 1 https://github.com/zvenson/dinghy.git "$ROOT/build/dinghy"
cd "$ROOT/build/dinghy"
git fetch --depth 1 origin "$DINGHY_COMMIT"
git checkout --detach "$DINGHY_COMMIT"
cp "$ROOT/native/dinghy.c" firmware/src/dinghy.c
python3 - <<'PY'
from pathlib import Path
p=Path('tools/build.py')
s=p.read_text().replace('SCREEN_NAME = "DINGHY"','SCREEN_NAME = "MIKU-FM"').replace('DATA_FAMILY = "DINGHY"','DATA_FAMILY = "MIKUFM"').replace('SCREEN_COLOUR = 0xB47F','SCREEN_COLOUR = 0x067F').replace('BOARD_PREFIX = {"fm1": "FM-1_99",','BOARD_PREFIX = {"fm1": "FM-1_94",').replace('PRODUCT = "FM-1_990"','PRODUCT = "FM-1_949"')
p.write_text(s)
PY
./build.sh
mkdir -p "$ROOT/build/native"
cp build/dinghy.fwsc "$ROOT/build/native/miku-fm1.fwsc"

#!/bin/bash
# SPDX-License-Identifier: MIT
# Build the Miku-FM FM-1 firmware (GPL-3.0-only, on Dinghy).  native/build.sh [--release X.Y[.Z]] [--video|--video-bench]
#   JIELI_TOOLCHAIN, AC79_SDK: read by Dinghy's build.sh   DINGHY_COMMIT: Dinghy revision to build on
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DINGHY_COMMIT="${DINGHY_COMMIT:-f77bf03283bbb2c05294b01cdad68a30b410f93d}"
REL=""
VIDEO=0
BENCH=0
while [ $# -gt 0 ]; do
    case "$1" in
        --release) REL="${2:?--release X.Y[.Z]}"; shift 2 ;;
        --video) VIDEO=1; shift ;;
        --video-bench) VIDEO=1; BENCH=1; shift ;;
        *) echo "usage: build.sh [--release X.Y[.Z]] [--video|--video-bench]"; exit 1 ;;
    esac
done
[[ -z "$REL" || "$REL" =~ ^([0-9]{1,2})\.([0-9]{1,2})(\.[0-9]{1,2})?(-[A-Za-z0-9]+)?$ ]] || { echo "build.sh: --release $REL: use X.Y or X.Y.Z (0-99 each)"; exit 1; }
[ "$VIDEO" -eq 0 ] || [ -z "$REL" ] || { echo "video is a development-only profile; omit --release"; exit 1; }
mkdir -p "$ROOT/build"
[ -d "$ROOT/build/dinghy/.git" ] || git clone --depth 1 https://github.com/zvenson/dinghy.git "$ROOT/build/dinghy"
cd "$ROOT/build/dinghy"
git reset -q --hard
git clean -qfd
git fetch --depth 1 origin "$DINGHY_COMMIT"
git checkout -q --detach "$DINGHY_COMMIT"
python3 "$ROOT/tools/patch_dinghy_slots.py" "$ROOT/build/dinghy"
cp "$ROOT/native/dinghy.c" firmware/src/dinghy.c
cp "$ROOT/native/lyrics_ui.inc" "$ROOT/native/visuals_ui.inc" "$ROOT/native/library_ui.inc" firmware/src/
# the modules keep their repo paths (their .c files #include "firmware/..."; -Ifirmware/src finds them); dinghy.c includes the .c files
mkdir -p firmware/src/firmware/audio/fvoice firmware/src/firmware/audio/vfx firmware/src/firmware/audio/fmband firmware/src/firmware/ui firmware/src/firmware/song firmware/src/firmware/app
for f in fvoice.c fvoice.h fvoice_tables.h; do cp "$ROOT/firmware/audio/fvoice/$f" firmware/src/firmware/audio/fvoice/; done
cp "$ROOT"/firmware/audio/fmband/fmband*.[ch] firmware/src/firmware/audio/fmband/
cp "$ROOT"/firmware/audio/vfx/vfx.[ch] firmware/src/firmware/audio/vfx/
cp "$ROOT"/firmware/song/*.[ch] firmware/src/firmware/song/
cp "$ROOT"/firmware/app/engine.[ch] firmware/src/firmware/app/
cp "$ROOT/firmware/ui/singer_sprites.h" firmware/src/firmware/ui/
cp "$ROOT"/firmware/ui/performance.[ch] firmware/src/firmware/ui/
cp "$ROOT"/firmware/ui/sprite_codec.[ch] firmware/src/firmware/ui/
cp "$ROOT/firmware/ui/singer_compact.h" "$ROOT/firmware/ui/wardrobe_sprites.h" "$ROOT/firmware/ui/wardrobe_meta.h" "$ROOT/firmware/ui/stagefx_sprites.h" firmware/src/firmware/ui/
cp "$ROOT"/firmware/ui/stagefx.[ch] firmware/src/firmware/ui/
if [ "$VIDEO" -eq 1 ]; then
    mkdir -p firmware/src/firmware/usb/video firmware/src/firmware/usb/jpeg
    cp "$ROOT"/firmware/usb/video/uvc.[ch] firmware/src/firmware/usb/video/
    cp "$ROOT"/firmware/usb/jpeg/*.[ch] firmware/src/firmware/usb/jpeg/
    cp "$ROOT/native/video_usb.inc" firmware/src/
    python3 "$ROOT/tools/patch_dinghy_video.py" "$ROOT/build/dinghy"
fi
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
if [ "$VIDEO" -eq 1 ]; then
    export DINGHY_DEFS="${DINGHY_DEFS:-} -DMIKU_UVC=1 -DMIKU_VIDEO_BENCH=$BENCH -DFELUCCA_UAC=0 -DFELUCCA_CDC=0"
fi
export DINGHY_DEFS="${DINGHY_DEFS:-} -DSTORE_MAGIC=0x554B494Du"   # "MIKU": never parse Dinghy's saved data
./build.sh ${REL:+--release "$REL"}
mkdir -p "$ROOT/build/native"
V="${REL:+-${REL%%-*}}"
[ "$VIDEO" -eq 0 ] || V="-video"
[ "$BENCH" -eq 0 ] || V="-video-bench"
cp "build/dinghy${REL:+-$REL}.fwsc" "$ROOT/build/native/miku-fm$V.fwsc"
cp build/dinghy.bin "$ROOT/build/native/miku-fm$V-app.bin"
"$JIELI_TOOLCHAIN/common/bin/objdump" -t build/dinghy.elf > "$ROOT/build/native/miku-fm$V-symbols.txt"
python3 "$ROOT/tools/check_resources.py" --image "$ROOT/build/native/miku-fm$V-app.bin" \
    --symbols "$ROOT/build/native/miku-fm$V-symbols.txt" --output "$ROOT/build/native/miku-fm$V-resources.json"

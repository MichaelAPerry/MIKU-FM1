#!/bin/sh
# SPDX-License-Identifier: MIT
# Add the firmware package, emulated, to the site: fm1-emu's web build (MIT, github.com/joakimeriksson/fm1-emu) in
# SITE/fm1-emu/, its catalog holding only firmware/miku-fm-VERSION.fwsc. (After MPFM1's tools/site_emu.sh.)
#   tools/site_emu.sh SITE_DIR VERSION [FM1_EMU_DIR]   (FM1_EMU_DIR: a clone at FM1_EMU_REV; cloned if absent)
set -eu
SITE="${1:?usage: tools/site_emu.sh SITE_DIR VERSION [FM1_EMU_DIR]}"; V="${2:?version}"
REV="${FM1_EMU_REV:-9de1ef0}"; EMU="${3:-build/fm1-emu-src}"
[ -f "$SITE/firmware/miku-fm-$V.fwsc" ] || { echo "site_emu.sh: no $SITE/firmware/miku-fm-$V.fwsc"; exit 1; }
[ -d "$EMU/.git" ] || git clone -q https://github.com/joakimeriksson/fm1-emu "$EMU"
git -C "$EMU" -c advice.detachedHead=false checkout -q "$REV"
rustup target add wasm32-unknown-unknown >/dev/null 2>&1 || true
bash "$EMU/web/build.sh" "$(cd "$SITE" && pwd)/fm1-emu"
cp "$EMU/LICENSE" "$SITE/fm1-emu/LICENSE"
python3 - "$SITE/fm1-emu/firmwares.json" "$V" <<'PY'
import json, sys
json.dump({"note": "Miku-FM only: the PI32 firmware package, emulated.", "default": "miku",
           "firmwares": [{"id": "miku", "name": "Miku-FM", "version": sys.argv[2], "author": "Michael Perry",
                          "license": "GPL-3.0-only", "repo": "https://github.com/MichaelAPerry/MIKU-FM1",
                          "about": "A singing synth and songwriting workstation: the firmware package itself, emulated.",
                          "package": f"../firmware/miku-fm-{sys.argv[2]}.fwsc"}]},
          open(sys.argv[1], "w"), indent=1)
PY
echo "site_emu: $SITE/fm1-emu (fm1-emu $REV, miku-fm-$V.fwsc)"

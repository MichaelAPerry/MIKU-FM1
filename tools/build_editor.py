#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the distributable offline Miku-FM phrase editor."""
import argparse
import base64
import gzip
import hashlib
import html
import io
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "site/editor"
OUTPUT = ROOT / "site/editor.html"
SDK = "/home/perry/.local/opt/wasi-sdk-24.0-x86_64-linux"
CMU_SHA = "81917843c7f44ce2b094ac63873c2c7a4cf802040792c455ba3ca406891c3d22"
FIRMWARE = (
    "app/engine.c", "song/phrase.c", "song/english.c", "song/lyrics.c", "song/mora.c",
    "song/song.c", "song/song_io.c", "song/seq.c", "song/dx7_edit.c",
    "audio/fvoice/fvoice.c", "audio/dx7/dx7.c", "audio/vfx/vfx.c",
    "audio/fmband/fmband_tables.c", "audio/fmband/fmband_mx.c",
    "audio/fmband/fmband_ch.c", "audio/fmband/fmband_bs.c", "audio/fmband/fmband_dr.c",
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check committed HTML for reproducibility")
    args = parser.parse_args()
    clang = Path(os.environ.get("WASI_SDK", SDK)) / "bin/clang"
    dictionary = Path(os.environ.get("CMUDICT", ROOT / "build/ref/cmudict.dict"))
    license_file = dictionary.with_name("cmudict.LICENSE")
    if not license_file.is_file():
        license_file = ROOT / "build/ref/cmudict.LICENSE"
    missing = [str(path) for path in (clang, dictionary, license_file) if not path.is_file()]
    if missing:
        print(("Skipping editor rebuild, missing " if args.check else "Missing ") + ", ".join(missing))
        return 77 if args.check else 1
    raw = dictionary.read_bytes()
    if hashlib.sha256(raw).hexdigest() != CMU_SHA:
        parser.error("CMUdict SHA-256 mismatch")
    bridge = SOURCE / "bridge.c"
    exports = re.findall(r"\b(ed_\w+)\(", bridge.read_text())
    with tempfile.TemporaryDirectory() as tmp:
        wasm_file = Path(tmp) / "voice.wasm"
        subprocess.run([
            str(clang), "--target=wasm32", "-Oz", "-flto", "-nostdlib", "-ffreestanding",
            "-DPH_EXTERNAL_LOOKUP", "-Wl,--no-entry", "-Wl,--strip-all",
            "-Wl,-z,stack-size=65536",
            *["-Wl,--export=" + name for name in exports], "-I.", str(bridge),
            *["firmware/" + path for path in FIRMWARE], "-o", str(wasm_file),
        ], check=True, cwd=ROOT)
        wasm = wasm_file.read_bytes()
    # Strip external media URLs from the shared stylesheet for the offline page.
    css = re.sub(r"url\([^)]*\)", "none", (ROOT / "site/site.css").read_text())
    encode = lambda data: base64.b64encode(data).decode("ascii")
    compressed = io.BytesIO()
    with gzip.GzipFile(fileobj=compressed, mode="wb", mtime=0) as stream:
        stream.write(raw)
    fields = {
        "CSS": css + (SOURCE / "editor.css").read_text(),
        "MKX": (ROOT / "site/js/mkx.js").read_text(),
        "JS": "\n".join((SOURCE / name).read_text() for name in ("core.js", "midi.js", "ui.js")),
        "WASM": encode(wasm),
        "DICT": encode(compressed.getvalue()),
        "CMU_LICENSE": html.escape(license_file.read_text()),
        "CHIMERA_LICENSE": html.escape("\n".join(
            (ROOT / "LICENSES" / name).read_text() for name in ("MIT-CHIMERA.txt", "MIT-Shan-Sherif.txt")
        )),
    }
    page = re.sub(r"@@([A-Z_]+)@@", lambda match: fields[match[1]], (SOURCE / "editor.html").read_text())
    result = page.encode("utf-8")
    print(f"Editor HTML: {len(result):,} bytes, WASM: {len(wasm):,} bytes")
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_bytes() != result:
            print("Run tools/build_editor.py to regenerate site/editor.html")
            return 1
    else:
        OUTPUT.write_bytes(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

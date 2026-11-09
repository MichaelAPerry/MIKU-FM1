# SPDX-License-Identifier: GPL-3.0-only
"""The DX7 bank tool: the page's files exist and are linked, the factory .syx is current and equals what the firmware's own
C code builds, and (when node is installed) site/js/dx7syx.js matches that C code on test vectors.
Usage: test_bank_tool.py ROOT DX7_VECTORS_EXE"""
import json, re, shutil, subprocess, sys
from html.parser import HTMLParser
from pathlib import Path

root = Path(sys.argv[1])
vectors_exe = sys.argv[2]
sys.path.insert(0, str(root / "tools"))
import dx7_factory_syx as gen  # noqa: E402

site = root / "site"


class Refs(HTMLParser):
    def __init__(self):
        super().__init__()
        self.refs = []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        for k in ("href", "src"):
            if a.get(k):
                self.refs.append(a[k])


def local_refs(page):
    p = Refs()
    p.feed((site / page).read_text())
    out = []
    for r in p.refs:
        if re.match(r"^(https?:|mailto:|#|data:|javascript:)", r) or "{{" in r:
            continue
        if r in ("fm1-emu/", "./", "firmware/{{PACKAGE}}"):   # deployed by other steps
            continue
        out.append(r.split("#")[0].split("?")[0])
    return [r for r in out if r]


for page in ("index.html", "install.html", "bank.html", "guide.html", "syx.html"):
    assert (site / page).exists(), page
    for r in local_refs(page):
        assert (site / r).exists(), f"{page}: {r} does not exist"

bank = (site / "bank.html").read_text()
for need in ("js/dx7syx.js", "site.css", "media/dx7/miku-fm-factory.syx", "requestMIDIAccess", "REC"):
    assert need in bank, f"bank.html does not mention {need}"
assert not re.search(r"<script[^>]+src=[\"']https?:", bank), "bank.html loads an external script"
assert "bank.html" in (site / "syx.html").read_text() and len((site / "syx.html").read_text()) < 1500, "syx.html is a redirect"
for page in ("index.html", "install.html", "guide.html"):
    assert "bank.html" in (site / page).read_text(), f"{page} does not link the bank tool"
for page in ("index.html", "install.html", "bank.html"):
    assert "guide.html" in (site / page).read_text(), f"{page} does not link the guide"
front = (root / "tools/site_front.sh").read_text()
for name in ("bank.html", "syx.html", "guide.html"):
    assert name in front, f"tools/site_front.sh does not deploy {name}"

# the C side
out = subprocess.run([vectors_exe], check=True, capture_output=True, text=True).stdout
vec = json.loads(out)
assert bytes(vec["init"]) == gen.init_voice(), "tools/dx7_factory_syx.py INIT VOICE differs from dxs_init_voice()"
factory = site / "media/dx7/miku-fm-factory.syx"
assert factory.read_bytes() == gen.generate(), "miku-fm-factory.syx is stale (tools/dx7_factory_syx.py)"
assert bytes(vec["bulkMessage"]) == factory.read_bytes(), "the factory .syx differs from dxs_build_bulk()"

node = shutil.which("node")
if node:
    vf = Path(sys.argv[3]) if len(sys.argv) > 3 else Path(vectors_exe).with_suffix(".json")
    vf.write_text(out)
    r = subprocess.run([node, str(root / "tests/test_bank_tool.js"), str(vf), str(factory)], capture_output=True, text=True)
    print(r.stdout + r.stderr)
    if r.returncode:
        sys.exit(1)
else:
    print("node not found: the JS conversion test was skipped")
print("bank tool: OK")

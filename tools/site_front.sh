#!/bin/sh
# SPDX-License-Identifier: MIT
# Install the front page (site/index.html) into SITE, filling in the version, the build kind and the commit, and the
# SHA-256 of the package. release: the attested package of a published release. dev: built by the Pages workflow
# from a commit of main (not attested).
#   tools/site_front.sh SITE_DIR VERSION [release|dev] [COMMIT]
set -eu
SITE="${1:?usage: tools/site_front.sh SITE_DIR VERSION [release|dev] [COMMIT]}"; V="${2:?version}"
KIND="${3:-release}"; COMMIT="${4:-}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="miku-fm-$V.fwsc"
[ -f "$SITE/firmware/$PKG" ] || { echo "site_front.sh: no $SITE/firmware/$PKG"; exit 1; }
python3 - "$ROOT" "$SITE" "$V" "$PKG" "$KIND" "$COMMIT" <<'PY'
import hashlib, html, sys
from pathlib import Path
root, site, v, pkg, kind, commit = Path(sys.argv[1]), Path(sys.argv[2]), *sys.argv[3:7]
repo = "https://github.com/MichaelAPerry/MIKU-FM1"
sha = hashlib.sha256((site / "firmware" / pkg).read_bytes()).hexdigest()
if kind == "release":
    build = (f'Release {html.escape(v)}, built and attested on GitHub: '
             f'<a href="{repo}/releases/tag/v{html.escape(v)}">release page</a>, '
             f'<a href="{repo}/attestations">attestations</a>.')
else:
    build = (f'Development build of <a href="{repo}/commit/{html.escape(commit)}">{html.escape(commit[:7])}</a> '
             f'on main: <strong>not attested</strong>, for the emulator. Install a release, not this.')
page = (root / "site" / "index.html").read_text()
for k, val in {"{{VERSION}}": html.escape(v), "{{PACKAGE}}": html.escape(pkg), "{{SHA256}}": sha, "{{BUILD}}": build}.items():
    assert k in page, k
    page = page.replace(k, val)
(site / "index.html").write_text(page)
(site / ".nojekyll").touch()
PY
echo "site_front: $SITE/index.html ($KIND $V)"

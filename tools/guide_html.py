#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Render docs/MANUAL.md as site/guide.html (the website's guide page). One source of truth: edit the manual, run this.

  tools/guide_html.py           write site/guide.html
  tools/guide_html.py --check   fail if the committed page is stale

Understands the small Markdown subset the manual uses: # ## ### headings, paragraphs, - and 1. lists (with indented
continuation lines), pipe tables, **bold**, `code` and [links](url). Links to other files in docs/ become GitHub links.
"""
import argparse
import html
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "docs/MANUAL.md"
DST = ROOT / "site/guide.html"
REPO = "https://github.com/MichaelAPerry/MIKU-FM1"
SITE = "https://michaelaperry.github.io/MIKU-FM1/"


def slug(text):
    return re.sub(r"[^a-z0-9]+", "-", re.sub(r"\*\*|`", "", text).lower()).strip("-")


def fix_link(url):
    if url.startswith(SITE):
        return url[len(SITE):] or "./"
    if re.match(r"^(https?:|#|mailto:)", url):
        return url
    if url.endswith(".md") or ".md#" in url:
        name, _, frag = url.partition("#")
        if name == "MANUAL.md":
            return "#" + frag if frag else "guide.html"
        return f"{REPO}/blob/main/docs/{name}" + (f"#{frag}" if frag else "")
    return url


def inline(text):
    out, pos = [], 0
    for m in re.finditer(r"`([^`]+)`|\*\*(.+?)\*\*|\[([^\]]+)\]\(([^)]+)\)", text):
        out.append(html.escape(text[pos:m.start()], quote=False))
        if m.group(1) is not None:
            out.append(f"<code>{html.escape(m.group(1), quote=False)}</code>")
        elif m.group(2) is not None:
            out.append(f"<b>{inline(m.group(2))}</b>")
        else:
            out.append(f'<a href="{html.escape(fix_link(m.group(4)))}">{inline(m.group(3))}</a>')
        pos = m.end()
    out.append(html.escape(text[pos:], quote=False))
    return "".join(out)


def cells(line):
    return [c.strip() for c in line.strip().strip("|").split("|")]


def blocks(lines):
    """yield (kind, payload) for the manual's Markdown subset"""
    i = 0
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            i += 1
        elif ln.startswith("#"):
            m = re.match(r"(#+)\s+(.*)", ln)
            yield "h%d" % len(m.group(1)), m.group(2)
            i += 1
        elif ln.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append(lines[i])
                i += 1
            yield "table", [cells(r) for r in rows if not re.match(r"^\|[\s|:-]+\|$", r)]
        elif re.match(r"^(- |\d+\. )", ln):
            ordered = ln[0].isdigit()
            items = []
            while i < len(lines) and re.match(r"^(- |\d+\. )", lines[i]):
                item = [re.sub(r"^(- |\d+\. )", "", lines[i])]
                i += 1
                while i < len(lines) and lines[i].startswith("  ") and lines[i].strip():
                    item.append(lines[i].strip())
                    i += 1
                items.append(" ".join(item))
            yield "ol" if ordered else "ul", items
        else:
            para = []
            while i < len(lines) and lines[i].strip() and not re.match(r"^(#|\||- |\d+\. )", lines[i]):
                para.append(lines[i].strip())
                i += 1
            yield "p", " ".join(para)


def render():
    lines = SRC.read_text().split("\n")
    title, intro, sections, cur = None, [], [], None
    for kind, val in blocks(lines):
        if kind == "h1":
            title = val
        elif kind == "h2":
            cur = {"title": val, "id": slug(val), "html": []}
            sections.append(cur)
        else:
            if kind == "h3":
                h = f'<h3 id="{slug(val)}">{inline(val)}</h3>'
            elif kind == "p":
                h = f"<p>{inline(val)}</p>"
            elif kind in ("ul", "ol"):
                h = f"<{kind}>" + "".join(f"<li>{inline(x)}</li>" for x in val) + f"</{kind}>"
            elif kind == "table":
                head, body = val[0], val[1:]
                h = ('<div class="tablewrap"><table><thead><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr></thead><tbody>"
                     + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in body) + "</tbody></table></div>")
            else:
                raise ValueError(kind)
            (cur["html"] if cur else intro).append(h)
    toc = " ".join(f'<a href="#{s["id"]}">{inline(s["title"])}</a>' for s in sections)
    body = "".join(f'<section id="{s["id"]}">\n<h2>{inline(s["title"])}</h2>\n' + "\n".join(s["html"]) + "\n</section>\n" for s in sections)
    return f"""<!doctype html>
<!-- SPDX-License-Identifier: GPL-3.0-only -->
<!-- Generated from docs/MANUAL.md by tools/guide_html.py: edit the manual, not this file. -->
<html lang="en">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Miku-FM · guide</title>
<meta name="description" content="The Miku-FM 0.7 user guide: every page, the song generator, lyrics, looks, the song library, the DX7 voices, importing a cartridge and MIDI in.">
<link rel="icon" href="media/favicon-32.png"><link rel="apple-touch-icon" href="media/apple-touch-icon.png">
<link rel="stylesheet" href="site.css">
</head>
<body class="page"><main>
<div class="top">
  <div class="brand">Miku<span>-FM</span> · <span style="color:var(--dim);font-weight:500">guide</span></div>
  <nav><a href="./">Home</a><a href="guide.html" class="here">Guide</a><a href="bank.html">DX7 bank tool</a><a href="install.html">Install</a></nav>
</div>
<header>
  <p class="label">USER GUIDE · M-VAVE FM-1 · MIKU-FM 0.7</p>
  <h1>Miku<span>-FM</span> guide<i>.</i></h1>
  {"".join(intro) if intro else ""}
  <nav class="card toc" aria-label="Contents">{toc}</nav>
</header>
<div class="guide">
{body}</div>
<footer>
  Miku-FM · an unofficial, free, non-commercial fan firmware for the M-VAVE FM-1, not associated with or endorsed by Crypton
  Future Media, Yamaha or M-VAVE. This page is generated from <a href="{REPO}/blob/main/docs/MANUAL.md">docs/MANUAL.md</a>;
  every control in one table is in <a href="{REPO}/blob/main/docs/CONTROLS.md">CONTROLS</a>.
  <a href="{REPO}">Source</a>.
</footer>
</main></body>
</html>
"""


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    out = render()
    if a.check:
        if not DST.exists() or DST.read_text() != out:
            raise SystemExit("site/guide.html is stale: run tools/guide_html.py")
    else:
        DST.write_text(out)
        print(f"wrote {DST.relative_to(ROOT)}")

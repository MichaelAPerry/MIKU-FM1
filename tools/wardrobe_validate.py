#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Independent source QA for matched portrait/dance production wardrobe sheets.

Reads uniform 4x3 portrait/4x2 dance sources or one paired4x5 source sheet.
Does not alter artwork, quantize it, or treat a concept sheet as an animation atlas.
Pixel metrics are diagnostics, not automatic approval of a design or identity.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ORIGINALS = ["assets/singer/faces.webp", "assets/singer/stage.webp", "assets/singer/reference.webp"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_path(parent, name):
    path = Path(name)
    return path if path.is_absolute() else parent / path


def sheet(path, cols, rows, region=None):
    with Image.open(path) as src:
        im = src.convert("RGBA")
    if region is not None:
        im = im.crop(region)
    if im.width % cols or im.height % rows:
        raise ValueError(f"{path}: {im.size} is not an exact {cols}x{rows} grid")
    w, h = im.width // cols, im.height // rows
    if min(w, h) < 16:
        raise ValueError(f"{path}: cells {w}x{h} too small for a production source")
    frames, stats = [], []
    for index in range(cols * rows):
        x, y = index % cols * w, index // cols * h
        cell = im.crop((x, y, x + w, y + h))
        # Same broad magenta-key family as original asset recovery, evaluated
        # independently here. No crop or compositing changes the source image.
        foreground = []
        for red, green, blue, alpha in cell.getdata():
            magenta = ((red - 255) ** 2 + green ** 2 + (blue - 255) ** 2 < 105 ** 2 or
                       (min(red, blue) - green > 140 and red > 170 and blue > 170 and abs(red - blue) < 60))
            foreground.append(alpha > 0 and not magenta)
        visible = sum(foreground)
        if visible < w * h // 100:
            raise ValueError(f"{path}: frame {index} blank/less than1% foreground")
        stats.append({"frame": index, "foreground_fraction": round(visible / (w * h), 4),
                      "rgba_sha256": sha(cell.tobytes())})
        frames.append(cell)
    unique = len({s["rgba_sha256"] for s in stats})
    return {"path": str(path), "sha256": sha(path.read_bytes()), "size": list(im.size),
            "cell": [w, h], "source_region": list(region) if region else None,
            "frames": len(frames), "distinct_frames": unique,
            "frame_metrics": stats}, frames


def thumbnail(im, size):
    im = im.copy()
    im.thumbnail(size, Image.Resampling.NEAREST)
    return im


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "build/evidence/wardrobe-validation")
    parser.add_argument("--expected-ready", type=int)
    parser.add_argument("--allow-pending", action="store_true")
    parser.add_argument("--baseline-ref", default="origin/main")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.manifest.read_text())
    report = {"manifest": str(args.manifest), "manifest_sha256": sha(args.manifest.read_bytes()),
              "originals": [], "outfits": [], "errors": [], "warnings": []}
    for name in ORIGINALS:
        try:
            baseline = subprocess.check_output(["git", "show", f"{args.baseline_ref}:{name}"], cwd=ROOT, stderr=subprocess.PIPE)
            actual = (ROOT / name).read_bytes()
            same = baseline == actual
            report["originals"].append({"path": name, "sha256": sha(actual), "matches_baseline": same})
            if not same:
                report["errors"].append(f"Original source changed: {name}")
        except (OSError, subprocess.CalledProcessError) as exc:
            report["errors"].append(f"Cannot verify baseline for {name}: {exc}")
    entries = manifest.get("outfits", [])
    ids, seen_pairs = set(), set()
    for index, outfit in enumerate(entries):
        name = outfit.get("name", str(index))
        ident = str(outfit.get("id", outfit.get("concept", index)))
        row = {"id": ident, "name": name, "status": outfit.get("status", "concept")}
        report["outfits"].append(row)
        if ident in ids:
            report["errors"].append(f"Duplicate outfit ID: {ident}")
        ids.add(ident)
        layout = outfit.get("layout", "uniform")
        paired = outfit.get("sheet") if layout == "paired4x5" else None
        faces = outfit.get("faces", outfit.get("portrait_atlas"))
        stage = outfit.get("stage", outfit.get("dance_atlas"))
        if (not paired and (not faces or not stage)) or outfit.get("status") == "pending":
            row["ready"] = False
            if not args.allow_pending:
                report["errors"].append(f"{name}: no matched production portrait/dance source pair")
            continue
        if layout not in ["uniform", "paired4x5"] or outfit.get("margin", 0) != 0:
            report["errors"].append(f"{name}: validator requires uniform or paired4x5 grids without margins")
            continue
        try:
            if paired:
                path = source_path(args.manifest.parent, paired)
                with Image.open(path) as im:
                    width, height = im.size
                if width % 4 or height % 5:
                    raise ValueError(f"{path}: {width}x{height} is not exact paired4x5 grid")
                split = height // 5 * 3
                row["faces"], fs = sheet(path, 4, 3, (0, 0, width, split))
                row["stage"], ss = sheet(path, 4, 2, (0, split, width, height))
            else:
                row["faces"], fs = sheet(source_path(args.manifest.parent, faces), 4, 3)
                row["stage"], ss = sheet(source_path(args.manifest.parent, stage), 4, 2)
            pair = (row["faces"]["sha256"], row["stage"]["sha256"])
            if pair in seen_pairs:
                report["errors"].append(f"{name}: duplicate complete source pair masquerades as another outfit")
            seen_pairs.add(pair)
            row["ready"] = True
            for kind in ["faces", "stage"]:
                if row[kind]["distinct_frames"] == 1:
                    report["warnings"].append(f"{name}: all {kind} frames identical; inspect expressions/choreography")
            # Evidence pairs include every expression and pose, not only one
            # attractive concept thumbnail. This enables human matching review.
            contact = Image.new("RGB", (4 * 160, 5 * 160 + 28), (16, 24, 40))
            ImageDraw.Draw(contact).text((8, 6), f"{ident}: {name} — portraits12 / dance8", fill="white")
            for i, cell in enumerate(fs + ss):
                preview = thumbnail(cell, (154, 154))
                px, py = (i % 4) * 160, (i // 4) * 160 + 28
                contact.paste(preview, (px + (160 - preview.width) // 2, py), preview)
            contact.save(args.out / f"outfit-{index:02d}.png")
        except (OSError, ValueError) as exc:
            row["ready"] = False
            report["errors"].append(f"{name}: {exc}")
    report["ready"] = sum(o.get("ready", False) for o in report["outfits"])
    if not report["ready"] and not args.allow_pending:
        report["errors"].append("No production outfits validated; concept art is not an integrated skin")
    if args.expected_ready is not None and report["ready"] != args.expected_ready:
        report["errors"].append(f"Expected {args.expected_ready} ready outfits, found {report['ready']}")
    report["passed"] = not report["errors"]
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"passed": report["passed"], "ready": report["ready"],
                      "errors": report["errors"], "warnings": report["warnings"]}, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())

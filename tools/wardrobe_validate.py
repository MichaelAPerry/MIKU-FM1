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


def opaque_pixel(pixel):
    red, green, blue, alpha = pixel
    magenta = ((red - 255) ** 2 + green ** 2 + (blue - 255) ** 2 < 105 ** 2 or
               (min(red, blue) - green > 140 and red > 170 and blue > 170 and abs(red - blue) < 60))
    return alpha >= 128 and not magenta


def recover_boxes(path, cols, rows):
    # Independently inspect full-width/full-height background gutters. The
    # generated art may have shorter portrait rows than body rows; equal-grid
    # division alone can silently import a neighboring cell's hair or hat.
    with Image.open(path) as src:
        im = src.convert("RGBA")
    pixels=list(getattr(im,"get_flattened_data",im.getdata)())
    dark_x,dark_y=[0]*im.width,[0]*im.height
    for pos,(red,green,blue,alpha) in enumerate(pixels):
        if alpha>=128 and max(red,green,blue)<48:
            dark_x[pos%im.width]+=1;dark_y[pos//im.width]+=1
    seam_x=[n/im.height>.92 for n in dark_x];seam_y=[n/im.width>.92 for n in dark_y]
    occupied_x,occupied_y=[False]*im.width,[False]*im.height
    for pos,pixel in enumerate(pixels):
        x,y=pos%im.width,pos//im.width
        if not seam_x[x] and not seam_y[y] and opaque_pixel(pixel):occupied_x[x]=True;occupied_y[y]=True
    axes = []
    for occupied, seam, count, window in [(occupied_x,seam_x,cols,.15),(occupied_y,seam_y,rows,.28)]:
        length, edges = len(occupied), [0]
        for index in range(1, count):
            centre = index * length / count
            lo = max(edges[-1] + 8, round(centre - length / count * window))
            hi = min(length - 8, round(centre + length / count * window))
            mids, run = [], []
            for pos in range(lo, hi + 1):
                if pos < hi and not occupied[pos]:
                    run.append(pos)
                else:
                    if len(run) >= 2:
                        mids.append((run[0] + run[-1] + 1) // 2)
                    run = []
            lines=[j for j in range(lo,hi) if seam[j]]
            candidates=lines or mids
            edges.append(min(candidates,key=lambda v:abs(v-centre)) if candidates else round(centre))
        edges.append(length)
        axes.append(edges)
    xx, yy = axes
    return [(xx[x], yy[y], xx[x + 1], yy[y + 1]) for y in range(rows) for x in range(cols)], axes


def sheet(path, cols, rows, boxes=None, margin=0):
    with Image.open(path) as src:
        im = src.convert("RGBA")
    if boxes is None:
        boxes = [(x * im.width // cols, y * im.height // rows,
                  (x + 1) * im.width // cols, (y + 1) * im.height // rows)
                 for y in range(rows) for x in range(cols)]
    boxes=[(a+margin,b+margin,c-margin,d-margin) for a,b,c,d in boxes]
    w = min(box[2] - box[0] for box in boxes)
    h = min(box[3] - box[1] for box in boxes)
    if min(w, h) < 16:
        raise ValueError(f"{path}: cells {w}x{h} too small for a production source")
    frames, stats = [], []
    for index, box in enumerate(boxes):
        cell = im.crop(box)
        cw, ch = cell.size
        foreground = [opaque_pixel(pixel) for pixel in getattr(cell, "get_flattened_data", cell.getdata)()]
        visible = sum(foreground)
        if visible < cw * ch // 100:
            raise ValueError(f"{path}: frame {index} blank/less than1% foreground")
        stats.append({"frame": index, "source_box": list(box), "cell_size": [cw, ch],
                      "foreground_fraction": round(visible / (cw * ch), 4),
                      "rgba_sha256": sha(cell.tobytes())})
        frames.append(cell)
    unique = len({s["rgba_sha256"] for s in stats})
    return {"path": str(path), "sha256": sha(path.read_bytes()), "size": list(im.size),
            "minimum_cell": [w, h],
            "uniform_cell_sizes": len({tuple(x["cell_size"]) for x in stats}) == 1,
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
        margin=outfit.get("margin",0)
        if layout not in ["uniform", "paired4x5"] or not isinstance(margin,int) or margin<0:
            report["errors"].append(f"{name}: validator requires uniform or paired4x5 grids and a nonnegative integer margin")
            continue
        try:
            if paired:
                path = source_path(args.manifest.parent, paired)
                boxes,axes=recover_boxes(path,4,5)
                if 'grid' in outfit:
                    axes=[outfit['grid']['x'],outfit['grid']['y']]
                    with Image.open(path) as source_im:
                        for bounds,count,limit in [(axes[0],4,source_im.width),(axes[1],5,source_im.height)]:
                            if len(bounds)!=count+1 or any(not isinstance(v,int) for v in bounds) or bounds[0]<0 or bounds[-1]>limit or any(a>=b for a,b in zip(bounds,bounds[1:])):raise ValueError('invalid explicit source grid')
                    boxes=[(axes[0][x],axes[1][y],axes[0][x+1],axes[1][y+1]) for y in range(5) for x in range(4)]
                row["source_grid_boundaries"] = {"x": axes[0], "y": axes[1]}
                row["faces"], fs = sheet(path, 4, 3, boxes[:12],margin)
                row["stage"], ss = sheet(path, 4, 2, boxes[12:],margin)
            else:
                face_path = source_path(args.manifest.parent, faces)
                stage_path = source_path(args.manifest.parent, stage)
                face_boxes, face_axes = recover_boxes(face_path, 4, 3)
                stage_boxes, stage_axes = recover_boxes(stage_path, 4, 2)
                row["source_grid_boundaries"] = {"faces": face_axes, "stage": stage_axes}
                row["faces"], fs = sheet(face_path, 4, 3, face_boxes,margin)
                row["stage"], ss = sheet(stage_path, 4, 2, stage_boxes,margin)
            pair = (row["faces"]["sha256"], row["stage"]["sha256"])
            if pair in seen_pairs:
                report["errors"].append(f"{name}: duplicate complete source pair masquerades as another outfit")
            seen_pairs.add(pair)
            row["ready"] = True
            for kind in ["faces", "stage"]:
                if not row[kind]["uniform_cell_sizes"]:
                    report["warnings"].append(f"{name}: {kind} source has unequal grid cells; inspect recovered gutters/contact sheet for cross-cell bleed")
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

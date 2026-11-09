#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode video_render fixtures through Pillow's libjpeg backend (no generated art)."""
import json
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1])
report = []
for bg in range(3):
    for frame in range(8):
        path = root / f"stage-bg{bg}-frame{frame}.jpg"
        with Image.open(path) as im:
            im.load()
            assert im.size == (160, 120) and im.mode == "RGB", path
            assert im.getextrema() != ((0, 0), (0, 0), (0, 0)), path
        report.append({"file": path.name, "bytes": path.stat().st_size})
for name, size in [("odd-noise", (159, 119)), ("one-pixel", (1, 1))]:
    with Image.open(root / f"{name}.jpg") as im:
        im.load()
        assert im.size == size and im.mode == "RGB", name
print(json.dumps({"decoded": len(report) + 2, "scene_min_bytes": min(x["bytes"] for x in report),
                  "scene_max_bytes": max(x["bytes"] for x in report)}))

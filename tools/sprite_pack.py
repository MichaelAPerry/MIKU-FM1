#!/usr/bin/env python3
"""Pack the singer art (assets/singer/faces.webp, stage.webp) into firmware/ui/singer_sprites.h.

    tools/sprite_pack.py              regenerate the header
    tools/sprite_pack.py --check      exit 1 if the committed header differs from a fresh build
    tools/sprite_pack.py --preview    decode the header's own data to docs/screens/*.png and docs/card_singer.png
    tools/sprite_pack.py --debug DIR  also dump intermediate images (recovered grids, palettes) to DIR

Needs Python 3 + Pillow + numpy.  Deterministic: no randomness, stable sorts.

Why the output sizes are what they are
  The art is AI-generated pixel art that was resized, so the "game pixels" are blurred and the pixel grid is
  neither 96x96 per cell nor square.  The grid is recovered from the image's edge spectrum (scan for the
  periodic pitch, then the phase), per cell and per axis:
    faces.webp : 362x362 px cells, pitch 4.01 x 3.11 px   -> 90 x 115 game pixels per cell (pixels are 1.29x
                 taller than wide in the file).  Each game pixel is read as the median of its block centre.
                 Horizontally the grid is kept 1:1 (90 columns); vertically 115 rows are re-sampled (weighted
                 vote in palette space, so edges stay crisp) to 90 rows so pixels are square on the LCD
                 -> 90x90 faces (2x = 180x180 on the 240x240 panel).
    stage.webp : 443.5 px cells, pitch ~5.76 x ~6.04 px -> kept at the native grid (about 1.05 aspect ignored);
                 frames are cropped to a common box with the feet on one baseline.
"""
import argparse
import os
import re
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FACES_SRC = os.path.join(ROOT, "assets", "singer", "faces.webp")
STAGE_SRC = os.path.join(ROOT, "assets", "singer", "stage.webp")
HEADER = os.path.join(ROOT, "firmware", "ui", "singer_sprites.h")
SCREENS = os.path.join(ROOT, "docs", "screens")
CARD = os.path.join(ROOT, "docs", "card_singer.png")
NAVY_BG = (0x0D, 0x1A, 0x3D)

FACE_NAMES = ["NEUTRAL", "BLINK", "A", "I", "U", "E", "O", "N", "HAPPY", "SURPRISED", "TILT_L", "TILT_R"]
STAGE_NAMES = ["IDLE0", "IDLE1", "IDLE2", "IDLE3", "SING0", "SING1", "ARMS", "BOW"]
FACE_W = FACE_H = 90
# eyes + mouth area in the native (90 x 115) face grid: x0, y0, x1, y1 (exclusive); frames 2..10 contribute only this
FACE_RECT = (27, 50, 66, 91)
FACE_COMPOSITE = range(1, 10)      # frame indices 2..10 (0-based 1..9) are pasted onto frame 1
FACE_MAXCOLORS = 15
STAGE_MAXCOLORS = 15


# --------------------------------------------------------------------------- grid recovery

def is_magenta(rgb):
    """Background key: tolerant distance to #FF00FF plus a tint test for blended pixels."""
    rgb = rgb.astype(np.float64)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    d = np.sqrt((r - 255) ** 2 + g ** 2 + (b - 255) ** 2)
    tint = np.minimum(r, b) - g
    return (d < 105) | ((tint > 140) & (r > 170) & (b > 170) & (np.abs(r - b) < 60))


def fit_period(profile, lo, hi, step=0.002):
    """Strongest periodic component of an edge profile: returns (pitch, first boundary offset)."""
    idx = np.arange(len(profile)) + 1.0          # diff[i] is the edge between pixel i and i+1
    best = (-1.0, lo, 0.0)
    for p in np.arange(lo, hi, step):
        z = (profile * np.exp(2j * np.pi * idx / p)).sum()
        if abs(z) > best[0]:
            best = (abs(z), p, (np.angle(z) / (2 * np.pi) * p) % p)
    return best[1], best[2]


def edge_profiles(img):
    f = img.astype(np.float64)
    gx = np.abs(np.diff(f, axis=1)).sum(2).sum(0)
    gy = np.abs(np.diff(f, axis=0)).sum(2).sum(1)
    return gx, gy


def sheet_pitch(img, lo, hi):
    gx, gy = edge_profiles(img)
    return fit_period(gx, lo, hi)[0], fit_period(gy, lo, hi)[0]


def cell_pitch(cell, px, py):
    """Pitch of one cell (searched within +-12% of the sheet estimate)."""
    gx, gy = edge_profiles(cell)
    return fit_period(gx, px * 0.88, px * 1.12)[0], fit_period(gy, py * 0.88, py * 1.12)[0]


def sample_grid(cell, px, py):
    """Recover the game-pixel grid of one cell at a known pitch (phase fitted per cell and axis).
    Returns (rgb[ny,nx,3] uint8, pitch_x, pitch_y)."""
    h, w, _ = cell.shape
    gx, gy = edge_profiles(cell)
    ox = fit_period(gx, px, px + 1e-6)[1]
    oy = fit_period(gy, py, py + 1e-6)[1]
    nx = int((w - ox) // px)
    ny = int((h - oy) // py)
    out = np.zeros((ny, nx, 3), np.uint8)
    for j in range(ny):
        y0 = oy + j * py
        ya, yb = int(y0 + py * 0.3), int(y0 + py * 0.7)
        yb = max(yb, ya + 1)
        for i in range(nx):
            x0 = ox + i * px
            xa, xb = int(x0 + px * 0.25), int(x0 + px * 0.75)
            xb = max(xb, xa + 1)
            blk = cell[ya:yb, xa:xb].reshape(-1, 3)
            out[j, i] = np.median(blk, axis=0)
    return out, px, py


def cell_native(img, box, px, py):
    x0, y0, x1, y1 = box
    return sample_grid(img[y0:y1, x0:x1], px, py)


# --------------------------------------------------------------------------- palette

CHROMA_BOOST = 2.0
_YCC = np.array([[0.299, 0.587, 0.114], [-0.1687, -0.3313, 0.5], [0.5, -0.4187, -0.0813]])
_YCC_SCALE = np.array([1.0, CHROMA_BOOST, CHROMA_BOOST])


def _fwd(rgb):
    return (rgb @ _YCC.T) * _YCC_SCALE


def _inv(f):
    return (f / _YCC_SCALE) @ np.linalg.inv(_YCC).T


def kmeans_palette(pixels, weights, k, iters=40):
    """Weighted median-cut seeded k-means in a chroma-boosted YCbCr space (thin saturated lines such as the
    pink trim survive instead of being merged into greys).  Deterministic."""
    pixels = _fwd(pixels.astype(np.float64))
    boxes = [np.arange(len(pixels))]
    while len(boxes) < k:
        # split the box with the biggest weighted variance along its widest axis
        best, bi = -1, 0
        for n, b in enumerate(boxes):
            if len(b) < 2:
                continue
            v = (pixels[b].var(axis=0) * weights[b].sum()).max()
            if v > best:
                best, bi = v, n
        if best <= 0:
            break
        b = boxes.pop(bi)
        ax = int(np.argmax(pixels[b].var(axis=0)))
        order = b[np.argsort(pixels[b][:, ax], kind="stable")]
        cw = np.cumsum(weights[order])
        cut = int(np.searchsorted(cw, cw[-1] / 2.0)) + 1
        cut = min(max(cut, 1), len(order) - 1)
        boxes += [order[:cut], order[cut:]]
    cents = np.array([np.average(pixels[b], axis=0, weights=weights[b]) for b in boxes])
    for _ in range(iters):
        d = ((pixels[:, None, :] - cents[None]) ** 2).sum(2)
        lab = d.argmin(1)
        new = cents.copy()
        for c in range(len(cents)):
            m = lab == c
            if m.any():
                new[c] = np.average(pixels[m], axis=0, weights=weights[m])
        if np.allclose(new, cents, atol=0.01):
            break
        cents = new
    return np.clip(np.rint(_inv(cents)), 0, 255).astype(np.uint8)


def build_palette(frames_rgb, frames_alpha, maxcolors, exclude_masks=None):
    """frames: list of [h,w,3] uint8 and bool opaque masks.  Returns uint8 [n,3], sorted dark..light."""
    cols = []
    for f, a, ex in zip(frames_rgb, frames_alpha, exclude_masks or [None] * len(frames_rgb)):
        m = a if ex is None else (a & ~ex)
        cols.append(f[m])
    cols = np.concatenate(cols)
    uniq, cnt = np.unique(cols.reshape(-1, 3), axis=0, return_counts=True)
    # sqrt of the count: rare-but-important colours (eye teal, mouth pink) keep a cluster
    pal = kmeans_palette(uniq, np.sqrt(cnt.astype(np.float64)), maxcolors)
    lum = pal[:, 0] * 0.3 + pal[:, 1] * 0.59 + pal[:, 2] * 0.11
    order = np.lexsort((pal[:, 2], pal[:, 1], pal[:, 0], lum))
    return pal[order]


def snap(rgb, pal):
    d = ((rgb[..., None, :].astype(np.int32) - pal[None, None].astype(np.int32)) ** 2).sum(-1)
    return d.argmin(-1)


def to565(c):
    r, g, b = [int(v) for v in c]
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def tint(rgb):
    rgb = rgb.astype(np.int32)
    return np.minimum(rgb[..., 0], rgb[..., 2]) - rgb[..., 1]


def neighbours_transparent(opaque):
    t = ~opaque
    p = np.pad(t, 1, constant_values=True)
    h, w = opaque.shape
    n = np.zeros_like(opaque)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dy or dx:
                n |= p[1 + dy:1 + dy + h, 1 + dx:1 + dx + w]
    return n


def clean_native(rgb):
    """Key the background out; remove magenta-tinted fringe pixels.  Returns (rgb, opaque, fringe)."""
    bg = is_magenta(rgb)
    opaque = ~bg
    edge = opaque & neighbours_transparent(opaque)
    lum = rgb[..., 0].astype(int) * 3 + rgb[..., 1].astype(int) * 6 + rgb[..., 2].astype(int)
    fringe = edge & (tint(rgb) > 45) & (lum < 1400)   # purple-ish edge blend: really outline navy
    # isolated opaque specks (no opaque 4-neighbour) are encoding noise
    p = np.pad(opaque, 1, constant_values=False)
    nb = p[:-2, 1:-1].astype(int) + p[2:, 1:-1] + p[1:-1, :-2] + p[1:-1, 2:]
    opaque = opaque & (nb > 0)
    return rgb, opaque, fringe & opaque


# --------------------------------------------------------------------------- alignment

def shift_img(a, dx, dy, fill):
    """out[y, x] = a[y + dy, x + dx]."""
    h, w = a.shape[:2]
    out = np.empty_like(a)
    out[...] = fill
    ys, ye = max(0, -dy), min(h, h - dy)
    xs, xe = max(0, -dx), min(w, w - dx)
    if ys < ye and xs < xe:
        out[ys:ye, xs:xe] = a[ys + dy:ye + dy, xs + dx:xe + dx]
    return out


def best_shift(base, img, mask, rng=6):
    """Integer (dx, dy) minimising clipped colour difference over mask."""
    best = None
    bf = base.astype(np.int32)
    for dy in range(-rng, rng + 1):
        for dx in range(-rng, rng + 1):
            s = shift_img(img, dx, dy, (255, 0, 255)).astype(np.int32)
            d = np.minimum(np.abs(s - bf).sum(-1), 200)[mask].mean()
            if best is None or d < best[0] - 1e-9:
                best = (d, dx, dy)
    return best[1], best[2], best[0]


def vote_resample_rows(idx, n_out, prio):
    """Resample palette-index image vertically to n_out rows by an area-weighted vote.
    idx: [h,w] ints (0 = transparent).  prio: per-index weight multiplier."""
    h, w = idx.shape
    s = h / n_out
    ncol = int(idx.max()) + 1
    out = np.zeros((n_out, w), np.uint8)
    for ty in range(n_out):
        a, b = ty * s, (ty + 1) * s
        acc = np.zeros((ncol, w))
        for sy in range(int(a), min(int(np.ceil(b)), h)):
            wt = min(b, sy + 1) - max(a, sy)
            if wt <= 0:
                continue
            row = idx[sy]
            acc[row, np.arange(w)] += wt * prio[row]
        out[ty] = acc.argmax(0)
    return out


# --------------------------------------------------------------------------- faces

def build_faces(dbg=None):
    img = np.asarray(Image.open(FACES_SRC).convert("RGB"))
    H, W, _ = img.shape
    cw, ch = W // 4, H // 3
    px, py = sheet_pitch(img, 2.5, 6.0)
    boxes = [((k % 4) * cw, (k // 4) * ch, (k % 4 + 1) * cw, (k // 4 + 1) * ch) for k in range(12)]
    fits = [cell_pitch(img[b[1]:b[3], b[0]:b[2]], px, py) for b in boxes]
    px = float(np.median([f[0] for f in fits]))
    py = float(np.median([f[1] for f in fits]))
    nat = [cell_native(img, b, px, py)[0] for b in boxes]
    nh = min(n.shape[0] for n in nat)
    nw = min(n.shape[1] for n in nat)
    nat = [n[:nh, :nw] for n in nat]
    info = {"cell": (cw, ch), "pitch": (px, py), "native": (nw, nh)}

    # align every frame to frame 1 on everything outside the face rectangle
    x0, y0, x1, y1 = FACE_RECT
    mask = np.ones((nh, nw), bool)
    mask[max(0, y0 - 2):y1 + 2, max(0, x0 - 2):x1 + 2] = False
    base = nat[0]
    shifts = [(0, 0)]
    aligned = [base]
    for k in range(1, 12):
        if k >= 10:   # tilts: whole frame, whatever matches best
            m = np.ones((nh, nw), bool)
        else:
            m = mask
        dx, dy, d = best_shift(base, nat[k], m)
        shifts.append((dx, dy))
        aligned.append(shift_img(nat[k], dx, dy, (255, 0, 255)))
    info["shifts"] = shifts

    # composite frames 2..10: base + eyes/mouth rect of the aligned frame
    comp = []
    for k in range(12):
        f = aligned[k].copy()
        if k in FACE_COMPOSITE:
            f = base.copy()
            f[y0:y1, x0:x1] = aligned[k][y0:y1, x0:x1]
        comp.append(f)

    cleaned = [clean_native(f) for f in comp]
    rgbs = [c[0] for c in cleaned]
    opq = [c[1] for c in cleaned]
    fringe = [c[2] for c in cleaned]
    pal = build_palette(rgbs, opq, FACE_MAXCOLORS, fringe)
    navy = int(np.argmin(pal.astype(int).sum(1)))
    idx_native = []
    for rgb, o, fr in zip(rgbs, opq, fringe):
        i = snap(rgb, pal) + 1
        i[fr] = navy + 1
        i[~o] = 0
        idx_native.append(i)
    # vertical re-sample 115 -> 90 rows: transparent loses ties, dark outline colours win them
    prio = np.ones(len(pal) + 1)
    prio[0] = 0.85
    lum = pal[:, 0] * 0.3 + pal[:, 1] * 0.59 + pal[:, 2] * 0.11
    prio[1:][lum < 70] = 1.25
    prio[1:][(tint(pal) > 40) & (lum < 170)] = 1.4     # thin pink trim (headphones, jacket piping)
    frames = [vote_resample_rows(i, FACE_H, prio)[:, :FACE_W] for i in idx_native]
    out = []
    for f in frames:
        g = np.zeros((FACE_H, FACE_W), np.uint8)
        g[:, :f.shape[1]] = f
        out.append(g)
    if dbg:
        for k, f in enumerate(idx_native):
            Image.fromarray(render_idx(f, pal, (255, 0, 255))).resize((f.shape[1] * 4, f.shape[0] * 4), Image.NEAREST).save(
                os.path.join(dbg, "face_native_%02d.png" % k))
    dirty = []
    for k, f in enumerate(out):
        dirty.append(dirty_rect(out[0], f) if k else (0, 0, 0, 0))
    return out, pal, dirty, info


def render_idx(idx, pal, bg):
    full = np.vstack([np.array([bg], np.uint8), pal.astype(np.uint8)])
    return full[idx]


def dirty_rect(a, b):
    d = a != b
    if not d.any():
        return (0, 0, 0, 0)
    ys, xs = np.where(d)
    return (int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1))


# --------------------------------------------------------------------------- stage

def stage_cells(img):
    h, w, _ = img.shape
    dark = (img.max(2) < 110)
    cp, rp = dark.mean(0), dark.mean(1)

    def lines(p, n):
        pos = [i for i in range(len(p)) if p[i] > 0.45 and (i < 6 or i > len(p) - 7 or True)]
        # group runs
        groups, cur = [], [pos[0]]
        for i in pos[1:]:
            if i - cur[-1] <= 2:
                cur.append(i)
            else:
                groups.append(cur)
                cur = [i]
        groups.append(cur)
        cs = [g for g in groups if len(g) >= 2 or g[0] < 3 or g[0] > len(p) - 4]
        return [int(round(sum(g) / len(g))) for g in cs]
    xl = lines(cp, 5)
    yl = lines(rp, 3)
    # keep the strongest 5 / 3 boundaries (full-height lines)
    xl = [x for x in xl if cp[x] > 0.9] or xl
    yl = [y for y in yl if rp[y] > 0.75] or yl
    return xl, yl


def build_stage(dbg=None):
    img = np.asarray(Image.open(STAGE_SRC).convert("RGB"))
    xl, yl = stage_cells(img)
    assert len(xl) == 5 and len(yl) == 3, (xl, yl)
    inset = 5
    px, py = sheet_pitch(img[yl[0] + inset:yl[1] - inset, xl[0] + inset:xl[1] - inset], 4.0, 9.0)
    boxes = [(xl[k % 4] + inset, yl[k // 4] + inset, xl[k % 4 + 1] - inset, yl[k // 4 + 1] - inset) for k in range(8)]
    fits = [cell_pitch(img[b[1]:b[3], b[0]:b[2]], px, py) for b in boxes]
    px = float(np.median([f[0] for f in fits]))
    py = float(np.median([f[1] for f in fits]))
    raw = [cell_native(img, b, px, py)[0] for b in boxes]
    nh = min(r.shape[0] for r in raw)
    nw = min(r.shape[1] for r in raw)
    raw = [r[:nh, :nw] for r in raw]
    cleaned = [clean_native(r) for r in raw]
    # feet baseline: bottom-most opaque row of each frame, aligned to the lowest one
    bottoms = [int(np.where(c[1].any(1))[0].max()) for c in cleaned]
    base_y = max(bottoms)
    shifted = []
    for (rgb, o, fr), b in zip(cleaned, bottoms):
        dy = b - base_y
        shifted.append((shift_img(rgb, 0, dy, (255, 0, 255)), shift_img(o, 0, dy, False), shift_img(fr, 0, dy, False)))
    # common bounding box
    anyo = np.zeros((nh, nw), bool)
    for s in shifted:
        anyo |= s[1]
    ys, xs = np.where(anyo)
    bx0, bx1, by0, by1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    crop = [(s[0][by0:by1, bx0:bx1], s[1][by0:by1, bx0:bx1], s[2][by0:by1, bx0:bx1]) for s in shifted]
    pal = build_palette([c[0] for c in crop], [c[1] for c in crop], STAGE_MAXCOLORS, [c[2] for c in crop])
    navy = int(np.argmin(pal.astype(int).sum(1)))
    frames = []
    for rgb, o, fr in crop:
        i = snap(rgb, pal) + 1
        i[fr] = navy + 1
        i[~o] = 0
        frames.append(i.astype(np.uint8))
    info = {"pitch": (px, py), "native": (nw, nh), "crop": (int(bx1 - bx0), int(by1 - by0)), "lines": (xl, yl)}
    return frames, pal, info


# --------------------------------------------------------------------------- header I/O

def pack4(idx):
    flat = idx.reshape(-1).astype(np.uint8)
    if len(flat) % 2:
        flat = np.append(flat, 0)
    return ((flat[0::2] << 4) | flat[1::2]).astype(np.uint8)


def unpack4(data, w, h):
    d = np.asarray(data, np.uint8)
    flat = np.empty(len(d) * 2, np.uint8)
    flat[0::2] = d >> 4
    flat[1::2] = d & 15
    return flat[:w * h].reshape(h, w)


def c_bytes(arr, indent="    "):
    lines = []
    for i in range(0, len(arr), 16):
        lines.append(indent + ", ".join("0x%02X" % v for v in arr[i:i + 16]) + ",")
    return "\n".join(lines)


def make_header(faces, fpal, fdirty, stage, spal):
    L = []
    a = L.append
    a("/* firmware/ui/singer_sprites.h -- generated by tools/sprite_pack.py, do not edit.")
    a(" *")
    a(" * Source art: assets/singer/faces.webp and stage.webp: fan art of Hatsune Miku (Crypton Future Media, INC.)")
    a(" * under the Piapro Character License, free and non-commercial only. The pixel data below is that art: it is")
    a(" * NOT under the code's GPL-3.0 (see LICENSING.md).  (c) Crypton Future Media, INC. www.piapro.net")
    a(" * Rebuild: python3 tools/sprite_pack.py")
    a(" *")
    a(" * Pixels are 4 bits per pixel, row-major, two pixels per byte (even x in the high nibble), no row padding.")
    a(" * Palette index 0 is transparent (its RGB565 value is magenta, only to make leaks visible); 1..15 opaque.")
    a(" * Palettes are plain uint16_t RGB565 values (swap bytes for the panel in the driver if it needs that).")
    a(" * Faces 1..10 share identical hair/head/shoulders; only the eyes+mouth region differs.  SINGER_FACE_DIRTY[f]")
    a(" * is the box {x,y,w,h} where frame f differs from SF_NEUTRAL (all zero for SF_NEUTRAL itself): when changing")
    a(" * face from a to b, redraw the union of the two boxes with b's pixels.  The tilts differ almost everywhere.")
    a(" */")
    a("#ifndef SINGER_SPRITES_H")
    a("#define SINGER_SPRITES_H")
    a("")
    a("#include <stdint.h>")
    a("")
    a("#define SINGER_FACE_W %d" % FACE_W)
    a("#define SINGER_FACE_H %d" % FACE_H)
    a("#define SINGER_FACE_BYTES %d" % (FACE_W * FACE_H // 2))
    a("#define SINGER_STAGE_W %d" % stage[0].shape[1])
    a("#define SINGER_STAGE_H %d" % stage[0].shape[0])
    a("#define SINGER_STAGE_BYTES %d" % ((stage[0].shape[0] * stage[0].shape[1] + 1) // 2))
    a("")
    a("enum {")
    for n_, nm in enumerate(FACE_NAMES):
        a("    SF_%s = %d," % (nm, n_))
    a("    SF_COUNT = %d" % len(FACE_NAMES))
    a("};")
    a("")
    a("enum {")
    for n_, nm in enumerate(STAGE_NAMES):
        a("    ST_%s = %d," % (nm, n_))
    a("    ST_COUNT = %d" % len(STAGE_NAMES))
    a("};")
    a("")

    def pal_block(name, pal):
        a("static const uint16_t %s[16] = {" % name)
        vals = [0xF81F] + [to565(c) for c in pal]
        vals += [0] * (16 - len(vals))
        a("    " + ", ".join("0x%04X" % v for v in vals))
        a("};")
        a("")
    pal_block("SINGER_FACE_PAL", fpal)
    pal_block("SINGER_STAGE_PAL", spal)

    a("static const uint8_t SINGER_FACE_DIRTY[SF_COUNT][4] = { /* x, y, w, h */")
    for n_, d in enumerate(fdirty):
        a("    {%d, %d, %d, %d}, /* SF_%s */" % (d + (FACE_NAMES[n_],)))
    a("};")
    a("")
    total = 0
    for n_, f in enumerate(faces):
        data = pack4(f)
        total += len(data)
        a("static const uint8_t singer_face_%d[%d] = { /* SF_%s */" % (n_, len(data), FACE_NAMES[n_]))
        a(c_bytes(data))
        a("};")
        a("")
    a("static const uint8_t *const SINGER_FACES[SF_COUNT] = {")
    for n_ in range(12):
        a("    singer_face_%d," % n_)
    a("};")
    a("")
    for n_, f in enumerate(stage):
        data = pack4(f)
        total += len(data)
        a("static const uint8_t singer_stage_%d[%d] = { /* ST_%s */" % (n_, len(data), STAGE_NAMES[n_]))
        a(c_bytes(data))
        a("};")
        a("")
    a("static const uint8_t *const SINGER_STAGE[ST_COUNT] = {")
    for n_ in range(8):
        a("    singer_stage_%d," % n_)
    a("};")
    a("")
    a("/* total pixel data: %d bytes (faces %d, stage %d) + palettes 64 + dirty rects %d */" % (
        total, FACE_W * FACE_H // 2 * 12, total - FACE_W * FACE_H // 2 * 12, 12 * 4))
    a("")
    a("#endif /* SINGER_SPRITES_H */")
    return "\n".join(L) + "\n", total


def build(dbg=None):
    faces, fpal, fdirty, finfo = build_faces(dbg)
    stage, spal, sinfo = build_stage(dbg)
    text, total = make_header(faces, fpal, fdirty, stage, spal)
    return text, total, dict(faces=faces, fpal=fpal, fdirty=fdirty, finfo=finfo, stage=stage, spal=spal, sinfo=sinfo)


# --------------------------------------------------------------------------- preview (decodes the header itself)

def parse_header(text):
    def arr(name):
        m = re.search(r"%s\[[^\]]*\](?:\[\d+\])? = \{(.*?)\n\};" % re.escape(name), text, re.S)
        body = re.sub(r"/\*.*?\*/", "", m.group(1))
        return [int(v, 16) if v.lower().startswith("0x") else int(v) for v in re.findall(r"0x[0-9A-Fa-f]+|\d+", body)]
    consts = {k: int(v) for k, v in re.findall(r"#define (SINGER_\w+) (\d+)", text)}
    fw, fh, sw, sh = consts["SINGER_FACE_W"], consts["SINGER_FACE_H"], consts["SINGER_STAGE_W"], consts["SINGER_STAGE_H"]
    fpal = arr("SINGER_FACE_PAL")
    spal = arr("SINGER_STAGE_PAL")
    dirty = np.array(arr("SINGER_FACE_DIRTY")).reshape(-1, 4)
    faces = [unpack4(arr("singer_face_%d" % i), fw, fh) for i in range(12)]
    stage = [unpack4(arr("singer_stage_%d" % i), sw, sh) for i in range(8)]
    return fpal, spal, dirty, faces, stage


def rgb_of(pal, idx, bg):
    lut = np.array([bg] + [from565(v) for v in pal[1:]], np.uint8)
    return lut[idx]


def sheet(frames, pal, cols, scale, bg, gap=6, dirty=None):
    h, w = frames[0].shape
    rows = (len(frames) + cols - 1) // cols
    W = cols * (w * scale + gap) + gap
    Hh = rows * (h * scale + gap) + gap
    canvas = np.empty((Hh, W, 3), np.uint8)
    canvas[...] = bg
    for k, f in enumerate(frames):
        r, c = divmod(k, cols)
        im = np.repeat(np.repeat(rgb_of(pal, f, bg), scale, 0), scale, 1)
        y, x = gap + r * (h * scale + gap), gap + c * (w * scale + gap)
        canvas[y:y + h * scale, x:x + w * scale] = im
    return Image.fromarray(canvas)


def write_previews(text):
    fpal, spal, dirty, faces, stage = parse_header(text)
    os.makedirs(SCREENS, exist_ok=True)
    sheet(faces, fpal, 4, 2, NAVY_BG).save(os.path.join(SCREENS, "singer_faces.png"))
    sheet(stage, spal, 4, 3, NAVY_BG).save(os.path.join(SCREENS, "singer_stage.png"))
    # 1x contact strip for judging the real on-device size (not committed to docs: cheap, tiny)
    card = np.empty((240, 240, 3), np.uint8)
    card[...] = NAVY_BG
    face = np.repeat(np.repeat(rgb_of(fpal, faces[2], NAVY_BG), 2, 0), 2, 1)
    fh, fw = face.shape[:2]
    x0, y0 = (240 - fw) // 2, 6
    card[y0:y0 + fh, x0:x0 + fw] = face
    Image.fromarray(card).save(CARD)
    return fpal, spal, dirty, faces, stage


# --------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if the committed header is out of date")
    ap.add_argument("--preview", action="store_true", help="decode the header and write docs/screens/*.png, docs/card_singer.png")
    ap.add_argument("--debug", metavar="DIR", help="dump intermediate images")
    args = ap.parse_args()
    if args.debug:
        os.makedirs(args.debug, exist_ok=True)
    text, total, info = build(args.debug)
    if args.check:
        cur = open(HEADER).read() if os.path.exists(HEADER) else ""
        if cur != text:
            print("singer_sprites.h is out of date: run tools/sprite_pack.py", file=sys.stderr)
            return 1
        print("singer_sprites.h is up to date (%d bytes of pixel data)" % total)
        return 0
    if args.preview:
        write_previews(open(HEADER).read())
        print("wrote previews from", HEADER)
        return 0
    os.makedirs(os.path.dirname(HEADER), exist_ok=True)
    with open(HEADER, "w") as f:
        f.write(text)
    fi, si = info["finfo"], info["sinfo"]
    print("faces: cell %s pitch %.3f x %.3f native %s -> %dx%d, shifts %s" % (
        fi["cell"], fi["pitch"][0], fi["pitch"][1], fi["native"], FACE_W, FACE_H, fi["shifts"]))
    print("face palette: %d colours; dirty %s" % (len(info["fpal"]), info["fdirty"]))
    print("stage: pitch %.3f x %.3f native %s crop %s, palette %d colours" % (
        si["pitch"][0], si["pitch"][1], si["native"], si["crop"], len(info["spal"])))
    print("pixel data total: %d bytes; header %d bytes" % (total, len(text)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

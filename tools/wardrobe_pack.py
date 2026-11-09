#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Pack real paired portrait/dance outfit sheets into compact native atlases.
Source character images remain PCL art; code license does not relicense pixels.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import re
import numpy as np
from PIL import Image
ROOT = Path(__file__).resolve().parents[1]


class BudgetError(ValueError):
    def __init__(self, report):
        self.report = report
        super().__init__(f"wardrobe exceeds additional flash budget: {report['additional_bytes']} > {report['budget_bytes']}")


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'tools' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


source = module('sprite_pack')
compact = module('sprite_compact')


def grid_boundaries(image, columns, rows):
    """Snap near-equal divisions to genuine magenta/alpha gutters when present.
    Image generation can place a cell seam a few percent away from the nominal
    fraction; do not admit a neighboring body's hair into a portrait tile.
    Portrait/body row heights can differ: search28% vertically and15%
    horizontally; no genuine gutter means exact equal division.
    """
    im = np.asarray(image.convert('RGBA'))
    alpha = im[..., 3]
    opaque = alpha >= 128 if np.any(alpha < 128) else ~source.is_magenta(im[..., :3])
    dark=im[..., :3].max(axis=2)<48
    seams=[dark.mean(axis=0)>.92,dark.mean(axis=1)>.92]
    opaque[seams[1],:]=False;opaque[:,seams[0]]=False
    result = []
    for axis, count, length in ((0, columns, im.shape[1]), (1, rows, im.shape[0])):
        profile = opaque.any(axis=axis)
        points = [0]
        pitch = length / count
        for i in range(1, count):
            nominal = i * pitch
            window = .15 if axis == 0 else .28
            lo, hi = max(points[-1] + 8, round(nominal - pitch * window)), min(length - 8, round(nominal + pitch * window))
            runs, begin = [], None
            for j in range(lo, hi + 1):
                blank = j < hi and not profile[j]
                if blank and begin is None:
                    begin = j
                if not blank and begin is not None:
                    if j - begin >= 2:
                        runs.append((begin + j) // 2)
                    begin = None
            lines=[j for j in range(lo,hi) if seams[axis][j]]
            candidates=lines or runs
            points.append(min(candidates,key=lambda x:abs(x-nominal)) if candidates else round(nominal))
        points.append(length)
        result.append(points)
    return result


def cells(image, columns, rows, margin=0, grid=None):
    im = np.asarray(image.convert('RGBA'))
    h, w = im.shape[:2]
    if w < columns * 8 or h < rows * 8 or margin < 0:
        raise ValueError('sheet dimensions or margin invalid')
    xb,yb=grid_boundaries(image,columns,rows) if grid is None else (grid['x'],grid['y'])
    for bounds,count,limit in [(xb,columns,w),(yb,rows,h)]:
        if len(bounds)!=count+1 or any(not isinstance(v,int) for v in bounds) or bounds[0]<0 or bounds[-1]>limit or any(a>=b for a,b in zip(bounds,bounds[1:])):
            raise ValueError('invalid explicit source grid')
    result = []
    for y in range(rows):
        for x in range(columns):
            x0, x1 = xb[x] + margin, xb[x + 1] - margin
            y0, y1 = yb[y] + margin, yb[y + 1] - margin
            if x1 <= x0 or y1 <= y0:
                raise ValueError('margin consumes a cell')
            result.append(im[y0:y1, x0:x1].copy())
    return result


def normalize(frames, width, height, feet=False):
    """One union crop/scale for all frames; body feet share a common baseline.
    A true alpha background wins over magenta keying; otherwise use the project's
    existing magenta/fringe key. No synthetic recolours or pose substitutions.
    """
    max_h, max_w = max(f.shape[0] for f in frames), max(f.shape[1] for f in frames)
    rgbs, masks = [], []
    for frame in frames:
        rgb, alpha = frame[..., :3], frame[..., 3]
        if np.any(alpha < 128):
            mask = alpha >= 128
        else:
            _, mask, fringe = source.clean_native(rgb)
            rgb = rgb.copy()
            rgb[fringe] = (16, 24, 40)
        if np.count_nonzero(mask) < 16:
            raise ValueError('blank or almost empty source frame')
        canvas = np.full((max_h, max_w, 3), (255, 0, 255), np.uint8)
        opaque = np.zeros((max_h, max_w), bool)
        canvas[:rgb.shape[0], :rgb.shape[1]] = rgb
        opaque[:rgb.shape[0], :rgb.shape[1]] = mask
        rgbs.append(canvas)
        masks.append(opaque)
    if feet:
        # Source sheets draw singing poses at a different scale from idle poses.
        # Equalise upright silhouette height to the median idle height before
        # the common atlas transform; frame 7 is a deliberate bow, not upright.
        bounds = []
        for mask in masks:
            ys, xs = np.where(mask)
            bounds.append((int(xs.min()), int(ys.min()), int(xs.max())+1, int(ys.max())+1))
        target = int(np.median([y1-y0 for x0,y0,x1,y1 in bounds[:4]]))
        sized = []
        for k, (rgb, mask, (x0,y0,x1,y1)) in enumerate(zip(rgbs,masks,bounds)):
            h = target if k < 7 else y1-y0
            w = max(1, round((x1-x0)*h/(y1-y0)))
            r = np.asarray(Image.fromarray(rgb[y0:y1,x0:x1]).resize((w,h),Image.Resampling.NEAREST))
            m = np.asarray(Image.fromarray(mask[y0:y1,x0:x1]).resize((w,h),Image.Resampling.NEAREST))
            sized.append((r,m))
        max_w, max_h = max(r.shape[1] for r,m in sized), max(r.shape[0] for r,m in sized)
        rgbs, masks = [], []
        for r,m in sized:
            rgb = np.full((max_h,max_w,3),(255,0,255),np.uint8)
            mask = np.zeros((max_h,max_w),bool)
            x,y=(max_w-r.shape[1])//2,max_h-r.shape[0]
            rgb[y:y+r.shape[0],x:x+r.shape[1]]=r
            mask[y:y+r.shape[0],x:x+r.shape[1]]=m
            rgbs.append(rgb);masks.append(mask)
    if feet:
        bottoms = [int(np.where(m.any(1))[0][-1]) for m in masks]
        baseline = max(bottoms)
        rgbs = [source.shift_img(rgb, 0, b - baseline, (255, 0, 255)) for rgb, b in zip(rgbs, bottoms)]
        masks = [source.shift_img(mask, 0, b - baseline, False) for mask, b in zip(masks, bottoms)]
    union = np.logical_or.reduce(masks)
    ys, xs = np.where(union)
    x0, x1, y0, y1 = int(xs.min()), int(xs.max()) + 1, int(ys.min()), int(ys.max()) + 1
    # Preserve the source aspect ratio. Use one common target transform so idle
    # motion and tilted faces retain their generated relative positions.
    scale = min((width - 4) / (x1 - x0), (height - 4) / (y1 - y0))
    nw, nh = max(1, round((x1 - x0) * scale)), max(1, round((y1 - y0) * scale))
    dx, dy = (width - nw) // 2, height - nh if feet else (height - nh) // 2
    out_rgb, out_mask = [], []
    for rgb, mask in zip(rgbs, masks):
        r = np.asarray(Image.fromarray(rgb[y0:y1, x0:x1]).resize((nw, nh), Image.Resampling.NEAREST))
        m = np.asarray(Image.fromarray(mask[y0:y1, x0:x1]).resize((nw, nh), Image.Resampling.NEAREST))
        canvas = np.full((height, width, 3), (255, 0, 255), np.uint8)
        opaque = np.zeros((height, width), bool)
        canvas[dy:dy + nh, dx:dx + nw] = r
        opaque[dy:dy + nh, dx:dx + nw] = m
        out_rgb.append(canvas)
        out_mask.append(opaque)
    pal = source.build_palette(out_rgb, out_mask, 15)
    indexed, errors = [], []
    for rgb, mask in zip(out_rgb, out_mask):
        idx = (source.snap(rgb, pal) + 1).astype(np.uint8)
        idx[~mask] = 0
        indexed.append(idx)
        error = rgb[mask].astype(float) - pal[idx[mask] - 1].astype(float)
        errors.append(float(np.sqrt(np.mean(error * error))))
    palette = [0xf81f] + [source.to565(c) for c in pal]
    palette += [0] * (16 - len(palette))
    return indexed, palette, {'source_union': [x0, y0, x1 - x0, y1 - y0], 'target_size': [width, height],
                              'opaque_pixels': [int(m.sum()) for m in out_mask],
                              'distinct_frames': len({a.tobytes() for a in indexed}),
                              'quantization_rgb_rmse': errors, 'palette_opaque_count': len(pal)}


def compose_portraits(frames, palette, box=(25, 28, 42, 39)):
    """Same stable head/garment contract as original build_faces: generated
    eye/mouth region for states1..9; actual whole-frame tilts10/11 retained.
    """
    x, y, w, h = box
    if x < 0 or y < 0 or w <= 0 or h <= 0 or x + w > 90 or y + h > 90:
        raise ValueError('invalid normalized portrait expression box')
    rgb_palette = np.array([source.from565(p) for p in palette], np.uint8)
    base = frames[0]
    mask = np.ones(base.shape, bool)
    mask[y:y + h, x:x + w] = False
    result, shifts = [base], [(0, 0)]
    for i in range(1, 12):
        if i >= 10:
            result.append(frames[i])
            shifts.append((0, 0))
            continue
        dx, dy, _ = source.best_shift(rgb_palette[base], rgb_palette[frames[i]], mask, rng=3)
        aligned = source.shift_img(frames[i], dx, dy, 0)
        combined = base.copy()
        combined[y:y + h, x:x + w] = aligned[y:y + h, x:x + w]
        result.append(combined)
        shifts.append((int(dx), int(dy)))
    return result, {'expression_box': list(box), 'alignment_shifts': shifts,
                    'stable_head_composite_frames': list(range(1, 10))}


def atlas_text(name, frames):
    h, w = frames[0].shape
    data = compact.pack_atlas([a.reshape(-1).tolist() for a in frames], w, h, nearby=5)
    ids, offsets, payload, originals = data
    for i, row in enumerate(originals):
        if compact.decode_python(data, i) != list(row):
            raise ValueError('compact roundtrip failure')
    text = compact.array(name + '_ROWS', 'uint16_t', ids)
    text += compact.array(name + '_OFFSETS', 'uint16_t', offsets)
    text += compact.array(name + '_DATA', 'uint8_t', payload)
    text += f'static const SprAtlas {name} = {{ {w}, {h}, {len(frames)}, {len(originals)}, {len(payload)}, {name}_ROWS, {name}_OFFSETS, {name}_DATA }};\n'
    size = len(ids) * 2 + len(offsets) * 2 + len(payload)
    return text, {'data_bytes': size, 'raw_4bpp_bytes': (w * h + 1) // 2 * len(frames),
                  'unique_rows': len(originals), 'delta_rows': sum(payload[offsets[i]] == 2 for i in range(len(originals)))}


def load_entry(entry, base):
    layout = entry.get('layout', 'paired4x5')
    margin = entry.get('margin', 0)
    if not isinstance(margin, int):
        raise ValueError('margin must be an integer')
    paths = []
    if layout == 'paired4x5':
        path = base / entry['sheet']
        with Image.open(path) as im:
            frames = cells(im, 4, 5, margin, entry.get('grid'))
        faces, body = frames[:12], frames[12:]
        paths.append(path)
    elif layout == 'uniform':
        source.FACES_SRC, source.STAGE_SRC = str(base / entry['faces']), str(base / entry['stage'])
        paths = [Path(source.FACES_SRC), Path(source.STAGE_SRC)]
        with Image.open(source.FACES_SRC) as im:
            faces = cells(im, 4, 3, margin)
        with Image.open(source.STAGE_SRC) as im:
            body = cells(im, 4, 2, margin)
    elif layout == 'legacy':
        source.FACES_SRC, source.STAGE_SRC = str(base / entry['faces']), str(base / entry['stage'])
        paths = [Path(source.FACES_SRC), Path(source.STAGE_SRC)]
        ff, fp, _, _ = source.build_faces()
        sf, sp, _ = source.build_stage()
        # Reconstruct the legacy packer's keyed RGB for common-canvas fitting.
        def rgba(fs, pal):
            return [np.dstack((source.render_idx(f, pal, (255, 0, 255)), (f != 0).astype(np.uint8) * 255)) for f in fs]
        faces, body = rgba(ff, fp), rgba(sf, sp)
    else:
        raise ValueError('unknown layout ' + layout)
    return faces, body, [{'path': str(p.relative_to(base)) if p.is_relative_to(base) else str(p),
                           'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths]


def preview(path, frames, palette, columns=4):
    h, w = frames[0].shape
    rgb = np.array([source.from565(v) for v in palette], np.uint8)
    canvas = np.full((h * ((len(frames) + columns - 1) // columns), w * columns, 3), (255, 0, 255), np.uint8)
    for i, frame in enumerate(frames):
        y, x = i // columns * h, i % columns * w
        canvas[y:y + h, x:x + w] = rgb[frame]
    Image.fromarray(canvas).save(path)


def generate(manifest, base, preview_dir=None):
    if manifest.get('version') != 1 or not isinstance(manifest.get('outfits'), list):
        raise ValueError('expected wardrobe manifest version1 and outfits array')
    out = ['''/* GENERATED by tools/wardrobe_pack.py; do not edit.
 * Character images are Hatsune Miku fan art under the Piapro Character License,
 * free/noncommercial, NOT the code GPL. (c) Crypton Future Media, INC.
 * www.piapro.net. See LICENSING.md and source manifest attribution.
 */
#ifndef MIKU_WARDROBE_SPRITES_H
#define MIKU_WARDROBE_SPRITES_H
#include "firmware/ui/singer_compact.h"
typedef struct {
    const char *id, *name;
    const SprAtlas *faces, *stage;
    const uint16_t *face_palette, *stage_palette;
    const uint8_t *face_dirty;
    uint8_t approved;
} WardrobeSprites;
''']
    records, reports, seen, names = [], [], {'original'}, set()
    for entry in manifest['outfits']:
        slug, name = entry.get('id', ''), entry.get('name', '')
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,31}', slug) or slug in seen:
            raise ValueError('invalid or duplicate outfit id: ' + slug)
        seen.add(slug)
        cname = re.sub('[^A-Z0-9]', '_', slug.upper())
        if cname in names:
            raise ValueError('C identifier collision: ' + slug)
        names.add(cname)
        if not isinstance(name, str) or not 1 <= len(name) <= 20 or any(not 32 <= ord(c) <= 126 for c in name):
            raise ValueError('outfit name must be 1..20 printable ASCII chars')
        status = entry.get('status', 'pending')
        if status not in ('pending', 'prototype', 'approved'):
            raise ValueError('unknown approval status for ' + slug)
        if status == 'pending':
            continue
        ff, sf, hashes = load_entry(entry, base)
        faces, fp, fi = normalize(ff, 90, 90)
        faces, composite = compose_portraits(faces, fp, tuple(entry.get('face_expression_box', [25, 28, 42, 39])))
        fi.update(composite)
        fi['distinct_frames'] = len({a.tobytes() for a in faces})
        stage, sp, si = normalize(sf, 61, 71, feet=True)
        if fi['distinct_frames'] < 2 or si['distinct_frames'] < 2:
            raise ValueError('outfit has no actual portrait or dance animation: ' + slug)
        prefix = 'WD_' + cname
        ft, fs = atlas_text(prefix + '_FACE', faces)
        st, ss = atlas_text(prefix + '_STAGE', stage)
        dirty = [source.dirty_rect(faces[0], f) if i else (0, 0, 0, 0) for i, f in enumerate(faces)]
        out += [ft, st, compact.array(prefix + '_FPAL', 'uint16_t', fp), compact.array(prefix + '_SPAL', 'uint16_t', sp),
                compact.array(prefix + '_DIRTY', 'uint8_t', [x for box in dirty for x in box])]
        records.append(f'    {{ "{slug}", {json.dumps(name)}, &{prefix}_FACE, &{prefix}_STAGE, {prefix}_FPAL, {prefix}_SPAL, {prefix}_DIRTY, {int(status == "approved")} }},\n')
        # 32-bit target accounting: two SprAtlas descriptors24B each, registry32B,
        # palettes64B, dirty48B, and NUL-terminated names/IDs; align conservative.
        overhead = 48 + 32 + 64 + 48 + len(slug) + len(name) + 2 + 16
        report = {'id': slug, 'name': name, 'status': status, 'sources': hashes,
                  'faces': dict(fi, **fs), 'stage': dict(si, **ss),
                  'accounted_bytes': fs['data_bytes'] + ss['data_bytes'] + overhead}
        reports.append(report)
        if preview_dir:
            preview_dir.mkdir(parents=True, exist_ok=True)
            preview(preview_dir / (slug + '-faces.png'), faces, fp)
            preview(preview_dir / (slug + '-stage.png'), stage, sp)
    total = sum(r['accounted_bytes'] for r in reports)
    budget = manifest.get('budget_bytes', 330000)
    if not isinstance(budget, int) or not 0 <= budget <= 581564:
        raise ValueError('invalid explicit additional flash budget')
    report = {'version': 1, 'outfit_count': len(records) + 1, 'additional_bytes': total,
              'budget_bytes': budget, 'outfits': reports}
    if total > budget:
        raise BudgetError(report)
    out += [f'#define WD_OUTFIT_N {len(records) + 1}u\n#define WD_ADDITIONAL_BYTES {total}u\n',
            'static const WardrobeSprites WD_OUTFITS[WD_OUTFIT_N] = {\n',
            '    { "original", "ORIGINAL", &SC_FACE_ATLAS, &SC_STAGE_ATLAS, SC_FACE_PAL, SC_STAGE_PAL, SC_FACE_DIRTY, 1 },\n']
    out += records + ['};\n#endif\n']
    return ''.join(out), {'version': 1, 'outfit_count': len(records) + 1, 'additional_bytes': total,
                          'budget_bytes': budget, 'outfits': reports}


def metadata(report):
    names=['ORIGINAL']+[r['name'] for r in report['outfits']]
    rows=','.join('{'+json.dumps(name)+','+str(i)+',0,8,61,71,3,28,0}' for i,name in enumerate(names))
    return ('/* Generated by tools/wardrobe_pack.py. Geometry/labels only; pixels remain in wardrobe_sprites.h. */\n'
            '#ifndef MIKU_WARDROBE_META_H\n#define MIKU_WARDROBE_META_H\n'
            f'#define PERF_OUTFIT_N {len(names)}u\n#define PERF_OUTFIT_ROWS {{'+rows+'}\n#endif\n')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--manifest', type=Path, default=ROOT / 'assets/singer/outfits/manifest.json')
    ap.add_argument('--output', type=Path, default=ROOT / 'firmware/ui/wardrobe_sprites.h')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--metadata-output',type=Path,default=ROOT/'firmware/ui/wardrobe_meta.h')
    ap.add_argument('--preview-dir', type=Path)
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    manifest = json.loads(args.manifest.read_text())
    try:
        text, report = generate(manifest, args.manifest.resolve().parent, args.preview_dir)
    except BudgetError as error:
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(error.report, indent=2) + '\n')
        raise SystemExit(str(error))
    meta=metadata(report)
    if args.check:
        if not args.metadata_output.exists() or args.metadata_output.read_text()!=meta:
            raise SystemExit('wardrobe metadata is stale')
        if not args.output.exists() or args.output.read_text() != text:
            raise SystemExit('wardrobe sprite header is stale')
    else:
        args.output.write_text(text)
        args.metadata_output.write_text(meta)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f"wardrobe: {report['outfit_count']} outfits, {report['additional_bytes']}/{report['budget_bytes']} additional bytes")


if __name__ == '__main__':
    main()

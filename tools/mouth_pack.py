#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the shared viseme atlas and approval sheets from the singer portraits."""
import json
import re

import numpy as np
from PIL import Image, ImageDraw, PngImagePlugin

import sprite_compact as compact
import sprite_pack as singer
import wardrobe_pack as wardrobe

ROOT = wardrobe.ROOT
OUTFITS = ROOT / 'assets/singer/outfits'
SOURCE_PNG = ROOT / 'assets/singer/mouths.png'
APPROVAL_PNG = ROOT / 'docs/evidence/mouths/approval_sheet.png'
HEADER = ROOT / 'firmware/ui/mouth_sprites.h'
VISEMES = 'REST MBP FV TH EE EH AH OH OO UH L R'.split()
VOWELS = 'A I U E O N'.split()
FRAME_IDS = [singer.FACE_NAMES.index(name) for name in VOWELS]
EXPRESSION = (25, 28, 42, 39)
SIZE_LIMIT = 6000
BG = (24, 34, 53)
CREDIT = (
    'This work depicts Crypton Future Media, INC.\'s character "Hatsune Miku" under the '
    'Piapro Character License (PCL), https://piapro.jp/license/pcl/summary.\n'
    '© Crypton Future Media, INC. www.piapro.net · Free and noncommercial. '
    'See LICENSING.md and assets/NOTICE.md.'
)
CREDIT_JA = (
    'この作品はピアプロ・キャラクター・ライセンスに基づいてクリプトン・フューチャー・メディア株式会社の'
    'キャラクター「初音ミク」を描いたものです。'
)

# Review windows (left, top, right, bottom; exclusive) isolate mouth changes
# from the eye, cheek and collar changes in the same dirty rectangles.
WINDOWS = {
    'original': (38, 56, 55, 66),
    'encore': (40, 49, 53, 58),
    'off-duty': (38, 50, 52, 59),
    'retro-wave': (40, 49, 54, 59),
    'prism': (40, 49, 53, 58),
    'neon-punk': (39, 53, 53, 63),
    'city-beat': (40, 53, 52, 61),
    'signal-suit': (39, 49, 52, 57),
    'summer-beat': (40, 52, 51, 60),
    'snow-glow': (39, 51, 51, 59),
    'festival': (39, 53, 52, 61),
    'moon-circuit': (39, 53, 51, 61),
}

# Neutral, A, I, U, E, O, N cutouts within the original search window.
ORIGINAL_CUTOUTS = (
    (43, 61, 48, 62), (41, 56, 50, 66), (39, 57, 52, 63),
    (43, 59, 48, 63), (40, 58, 52, 64), (42, 56, 51, 66), (39, 59, 54, 62),
)


def portraits():
    """Rebuild source portraits in firmware registry order."""
    ui = HEADER.parent
    registry = (ui / 'wardrobe_sprites.h').read_text().split('WD_OUTFITS[WD_OUTFIT_N] = {', 1)[1]
    entries = dict((entry['id'], entry) for entry in
                   json.loads((OUTFITS / 'manifest.json').read_text())['outfits'])
    # The source portraits still have the vowel art used to design the overlay.
    for slug, label in re.findall(
            r'\{ "([^"]+)", "([^"]+)", &\w+, &\w+, \w+, \w+, \w+, [01] \}', registry):
        if slug == 'original':
            frames, rgb, _, _ = singer.build_faces()
            palette = [singer.to565((255, 0, 255))] + [singer.to565(c) for c in rgb]
            expression = EXPRESSION
        else:
            entry = entries[slug]
            source, _, _ = wardrobe.load_entry(entry, OUTFITS)
            frames, palette, _ = wardrobe.normalize(source, 90, 90)
            expression = tuple(entry.get('face_expression_box', EXPRESSION))
            frames, _ = wardrobe.compose_portraits(frames, palette, expression)
        yield slug, label, np.asarray(frames), palette, expression


def locate(slug, frames):
    x0, y0, x1, y1 = WINDOWS[slug]
    neutral = frames[0, y0:y1, x0:x1]
    skin = int(np.bincount(neutral.ravel()).argmax())
    crops = frames[FRAME_IDS, y0:y1, x0:x1]
    changed = np.any(crops != neutral, axis=0)
    # The window can contain chin shading. Keep neutral ink only near the
    # vowel center, with the rest position inferred from U for blank faces.
    u_y, u_x = np.where((crops[2] != skin) & (crops[2] != neutral))
    cx, cy = round(float(np.median(u_x))), round(float(np.median(u_y)))
    yy, xx = np.indices(neutral.shape)
    ink = ((neutral != skin) & changed & (abs(xx - cx) <= 4) & (abs(yy - cy) <= 2))
    iy, ix = np.where(ink)
    rest = (x0 + round(float(np.median(ix))), y0 + int(iy.min())) if len(ix) else (x0 + cx, y0 + cy)
    clean = frames[0].copy()
    # Normalize the tiny neutral mark only. The surrounding complexion and
    # all transparent overlay pixels retain the outfit's own palette.
    clean[y0:y1, x0:x1][ink] = skin
    mouth_changes = (crops != neutral) & (crops != skin)
    if slug == 'original':
        keep = np.zeros_like(mouth_changes)
        for mask, (left, top, right, bottom) in zip(keep, ORIGINAL_CUTOUTS[1:]):
            mask[top-y0:bottom-y0, left-x0:right-x0] = True
        mouth_changes &= keep
    diff = np.any(mouth_changes, axis=0) | ink
    dy, dx = np.where(diff)
    box = (x0 + int(dx.min()), y0 + int(dy.min()), int(np.ptp(dx)) + 1, int(np.ptp(dy)) + 1)
    return clean, skin, rest, box


def stamp(frame, x, y, rows, colors):
    """Replace a patch in face-pixel coordinates using source palette roles."""
    for offset, row in enumerate(rows):
        frame[y + offset, x:x + len(row)] = [colors[pixel] for pixel in row]


def make_frames(frames, skin):
    x0, y0, x1, y1 = WINDOWS['original']
    indices = [0, *FRAME_IDS]
    # Tight source cutouts keep cheek and jaw pixels out of the overlay.
    crops = np.zeros((len(indices), y1-y0, x1-x0), np.uint8)
    for dest, fi, (left, top, right, bottom) in zip(crops, indices, ORIGINAL_CUTOUTS):
        crop = frames[fi, top:bottom, left:right]
        dest[top-y0:bottom-y0, left-x0:right-x0] = np.where(crop == skin, 0, crop)
    rest, ah, ee, uh, eh, oh, mbp = crops
    # Remove repeated interior rows so the open mouths clear shorter chins.
    for vowel in (ah, oh):
        vowel[3:8] = vowel[5:10]
        vowel[8:] = 0
    ee[6, 2] = 0  # A lower corner pixel overhangs the narrowest chins.
    # Keep the source navy, mauve, lip pink and tooth highlights.
    colors = {'.': 0, '#': 1, 's': 5, 'd': 7, 'p': 10, 'm': 11, 't': 15}
    # Straighten N's two lobes for the pressed-lip closure.
    closure = {role: colors[role] for role in ".#sm"}
    if any(color not in mbp for color in closure.values()):
        raise ValueError("Source N no longer has the pressed-lip shades")
    mbp[:] = 0
    stamp(mbp, 3, 4, ['.s####s.', 's######s', '..smm...'], closure)

    # FV uses the tooth row from I, with E's lip pink along the lower edge.
    fv = ee.copy()
    stamp(fv, 1, 1, [
        '..s########s..',
        '.#mtttttttm#..',
        '.#mtttttttm#..',
        '..#ppppppp#...',
        '...s#####s....',
        '..............',
    ], colors)

    # TH uses E's open contour with a short tongue through the teeth.
    th = eh.copy()
    stamp(th, 2, 2, [
        '.s########s..',
        '#ttttttttt#..',
        '#ttsppsttt#..',
        '.##pppp###...',
        '...spps......',
        '....ss.......',
    ], colors)

    # OO pinches O inward. U retains its flat, soft opening.
    oo = np.zeros_like(oh)
    stamp(oo, 5, 3, ['..ss..', '.####.', '##pp##', '#spps#', '.####.', '..ss..'], colors)

    # L keeps A's open jaw with a tongue reaching toward the palate.
    tongue_l = ah.copy()
    stamp(tongue_l, 3, 0, [
        '.s######s.',
        '#tt#pp#tt#',
        '#m#ppps#m#',
        '###ppps###',
        '#sspppss#.',
        '.#ppppp#..',
        '.s#ppp#s..',
        '..s###s...',
    ], colors)

    # R widens U and lifts the two corners around a dark center.
    r = np.zeros_like(uh)
    stamp(r, 3, 3, ['...ssss...', '..######..', '.#s####s#.', '..#pppp#..', '...ssss...'], colors)
    return np.asarray([rest, mbp, fv, th, ee, eh, ah, oh, oo, uh, tongue_l, r])


def save_png(image, path):
    metadata = PngImagePlugin.PngInfo()
    metadata.add_itxt('Attribution', CREDIT_JA + '\n' + CREDIT)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, pnginfo=metadata, optimize=True)


def source_sheet(frames, rgb):
    _, h, w = frames.shape
    cell = 8*w + 8
    margin = 28
    sheet = Image.new('P', (margin + cell * len(VISEMES), 8*h + 71))
    label = rgb.index(singer.from565(singer.to565((101, 119, 147))))
    sheet.putpalette([component for color in rgb for component in color])
    sheet.info['transparency'] = 0
    draw = ImageDraw.Draw(sheet)
    for i, (name, frame) in enumerate(zip(VISEMES, frames)):
        x = margin + i*cell
        draw.text((x + cell//2, 4), name, fill=label, anchor='mt')
        for scale, y in [(1, 19), (8, 39)]:
            pixels = Image.frombytes('P', (w, h), frame.tobytes()).resize(
                (scale*w, scale*h), Image.Resampling.NEAREST)
            sheet.paste(pixels, (x + (cell - scale*w)//2, y))
    draw.text((3, 19), '1x', fill=label)
    draw.text((3, 39), '8x', fill=label)
    draw.multiline_text((4, 8*h + 43), CREDIT, fill=label, spacing=2)
    return sheet


def approval_sheet(portrait_rows, frames, mouth_rgb):
    left, tile, gutter, top = 116, 184, 20, 60
    compare = left + len(VISEMES)*tile + gutter
    width = compare + len(VOWELS)*tile
    bottom = top + len(portrait_rows)*tile
    sheet = Image.new('RGB', (width, bottom + 162), BG)
    draw = ImageDraw.Draw(sheet)
    draw.text((left, 10), 'SHARED VISEMES ON NEUTRAL · 2x', fill='white')
    draw.text((compare, 10), 'OUTFIT SOURCE FRAMES · 2x', fill='white')
    draw.line((compare-gutter//2, 8, compare-gutter//2, bottom), fill='#65758b')
    for start, names in [(left, VISEMES), (compare, VOWELS)]:
        for i, name in enumerate(names):
            draw.text((start+i*tile+90, 40), name, fill='white', anchor='mt')

    def face_at(rgb, x, y):
        sheet.paste(Image.fromarray(rgb).resize((180, 180), Image.Resampling.NEAREST), (x, y))

    for j, (label, faces, palette, clean, x, y) in enumerate(portrait_rows):
        y_screen = top+j*tile
        draw.text((8, y_screen+90), label, fill='white', anchor='lm')
        rgb = np.array([BG] + [singer.from565(p) for p in palette[1:]], np.uint8)
        for i, frame in enumerate(frames):
            face = rgb[clean]
            h, w = frame.shape
            crop = face[y:y+h, x:x+w]
            mask = frame != 0
            crop[mask] = mouth_rgb[frame[mask]]
            face_at(face, left+i*tile, y_screen)
        for i, fi in enumerate(FRAME_IDS):
            face_at(rgb[faces[fi]], compare+i*tile, y_screen)

    draw.text((left, bottom+12), 'MOUTH OVERLAYS · 6x', fill='white')
    h, w = frames.shape[1:]
    for i, frame in enumerate(frames):
        x = left + i*tile + (180-6*w)//2
        draw.text((left+i*tile+90, bottom+33), VISEMES[i], fill='white', anchor='mt')
        panel = np.broadcast_to((66, 78, 99), (h, w, 3)).astype(np.uint8).copy()
        mask = frame != 0
        panel[mask] = mouth_rgb[frame[mask]]
        sheet.paste(Image.fromarray(panel).resize((6*w, 6*h), Image.Resampling.NEAREST),
                    (x, bottom+50))
    draw.multiline_text((left, bottom+125), CREDIT, fill='white', spacing=3)
    return sheet


def generate():
    located = []
    for slug, label, faces, palette, expression in portraits():
        clean, skin, rest, box = locate(slug, faces)
        located.append((slug, label, faces, palette, expression, clean, skin, rest, box))
    original = located[0]
    frames = make_frames(original[2], original[6])
    used = np.unique(frames)
    palette = [original[3][i] for i in used]
    frames = np.searchsorted(used, frames).astype(np.uint8)
    h, w = frames.shape[1:]
    footprint = np.any(frames != 0, axis=0)
    reference_x, reference_y = original[7]
    x0, y0, _, _ = WINDOWS['original']
    anchors, clean_pixels, clean_offsets, rows = [], [], [0], []
    print(f'Shared mouth box: {w}x{h}; original top-left ({x0}, {y0})')
    for slug, label, faces, pal, expression, clean, skin, rest, box in located:
        x, y = x0 + rest[0] - reference_x, y0 + rest[1] - reference_y
        ex, ey, ew, eh = expression
        if not (ex <= x <= ex+ew-w and ey <= y <= ey+eh-h):
            raise ValueError(f'{slug}: shared mouth box leaves expression box')
        if np.any(clean[y:y+h, x:x+w][footprint] != skin):
            raise ValueError(f'{slug}: overlay touches non-skin pixels')
        erase_y, erase_x = np.where(clean != faces[0])
        for px, py in zip(erase_x, erase_y):
            if not (x <= px < x+w and y <= py < y+h):
                raise ValueError(f'{slug}: neutral lip pixel leaves mouth box')
            clean_pixels.extend((int((py-y)*w + px-x), skin))
        clean_offsets.append(len(clean_pixels)//2)
        anchors.extend((x, y))
        rows.append((label, faces, pal, clean, x, y))
        print(f'{slug:12} source mouth {box}, anchor ({x}, {y}), '
              f'offset ({x-x0:+}, {y-y0:+}), neutral lip pixels {len(erase_x)}')

    atlas, stats = wardrobe.atlas_text('MOUTH_ATLAS', frames)
    sheet = source_sheet(frames, [singer.from565(p) for p in palette])
    size = stats['data_bytes'] + 2*len(palette) + len(anchors) + len(clean_pixels) + len(clean_offsets) + 24
    if w*h > 256 or clean_offsets[-1] > 255:
        raise ValueError('Neutral lip removal table exceeds 8-bit limits')
    if size > SIZE_LIMIT or len(palette) > 16:
        raise ValueError(f'Mouth atlas exceeds budget: {size} bytes, {len(palette)} colors')
    header = f'''/* GENERATED by tools/mouth_pack.py; do not edit.
 * Character images are Hatsune Miku fan art under the Piapro Character License,
 * free/noncommercial, NOT the code GPL. (c) Crypton Future Media, INC.
 * www.piapro.net. See LICENSING.md and assets/NOTICE.md.
 * {CREDIT_JA}
 */
#ifndef MIKU_MOUTH_SPRITES_H
#define MIKU_MOUTH_SPRITES_H
#include "firmware/ui/sprite_codec.h"

/* FVV_* order: {" ".join(VISEMES)}. Palette index 0 is transparent. */
#define MOUTH_W {w}u
#define MOUTH_H {h}u
#define MOUTH_COUNT {len(frames)}u
#define MOUTH_OUTFIT_COUNT {len(rows)}u
#define MOUTH_FLASH_BYTES {size}u

'''
    header += atlas
    header += compact.array('MOUTH_PAL', 'uint16_t', palette)
    header += '/* Top-left x,y pairs in WD_OUTFITS order. */\n'
    header += compact.array('MOUTH_ANCHORS', 'uint8_t', anchors)
    header += '''/* Per-outfit range of (row-major mouth offset, outfit face palette index)
 * pairs. Apply to the neutral patch before alpha-compositing the overlay. */
'''
    header += compact.array('MOUTH_CLEAN_OFFSETS', 'uint8_t', clean_offsets)
    header += compact.array('MOUTH_CLEAN_PIXELS', 'uint8_t', clean_pixels)
    header += '#endif\n'
    save_png(sheet, SOURCE_PNG)
    save_png(approval_sheet(rows, frames, np.array([singer.from565(p) for p in palette], np.uint8)), APPROVAL_PNG)
    HEADER.write_text(header)
    print(f'{size} bytes on 32-bit target ({stats["data_bytes"]} atlas arrays, '
          f'{len(palette)} palette entries, including transparency); {HEADER.stat().st_size} byte header')


if __name__ == '__main__':
    generate()

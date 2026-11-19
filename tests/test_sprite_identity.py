#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare retained sprite bytes with a fingerprint of the pre-overlay headers."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from export_companion_art import atlas_frames
from sprite_compact import parse

BASELINE = ROOT / 'tests/sprite_identity_baseline.json'
RETIRED = range(2, 8)


def array_bytes(header, name, code):
    values = parse(header, name)
    return struct.pack(f'<{len(values)}{code}', *values)


def snapshot(directory, check_retired=False):
    original = (directory / 'singer_compact.h').read_text()
    wardrobe = (directory / 'wardrobe_sprites.h').read_text()
    registry = wardrobe.split('WD_OUTFITS[WD_OUTFIT_N] = {', 1)[1]
    result = {}
    for entry in re.finditer(
            r'\{ "([^"]+)", "[^"]+", &(\w+), &(\w+), (\w+), (\w+), (\w+), [01] \}', registry):
        slug, face, stage, fpal, spal, dirty = entry.groups()
        header = original if slug == 'original' else wardrobe
        for atlas, geometry in ((face, [90, 90, 12]), (stage, [61, 71, 8])):
            fields = re.search(rf'\b{atlas} = \{{ ([^}}]+)\}};', header)[1].split(',')
            assert list(map(int, fields[:3])) == geometry, atlas
        face, stage = face.removesuffix('_ATLAS'), stage.removesuffix('_ATLAS')
        face_rows = parse(header, face + '_ROWS')
        boxes = array_bytes(header, dirty, 'B')
        faces = list(atlas_frames(header, face, 90, 90, 12))
        if check_retired:
            for i in RETIRED:
                assert faces[i] == faces[0], (slug, i, 'pixels')
                assert face_rows[i*90:(i+1)*90] == face_rows[:90], (slug, i, 'row IDs')
                assert boxes[i*4:(i+1)*4] == bytes(4), (slug, i, 'dirty rect')
        retained = [i for i in range(12) if i not in RETIRED]
        face_palette = array_bytes(header, fpal, 'H')
        stage_palette = array_bytes(header, spal, 'H')
        stage_arrays = b''.join(array_bytes(header, stage + suffix, code)
                                for suffix, code in (('_ROWS', 'H'), ('_OFFSETS', 'H'), ('_DATA', 'B')))
        face_bytes = (2 * len(face_rows) + len(face_palette) + len(boxes)
                      + len(array_bytes(header, face + '_OFFSETS', 'H'))
                      + len(array_bytes(header, face + '_DATA', 'B')))
        result[slug] = {
            'face_bytes': face_bytes,
            'retained_faces': [sha256(faces[i]).hexdigest() for i in retained],
            'retained_dirty': b''.join(boxes[i*4:(i+1)*4] for i in retained).hex(),
            'face_palette': face_palette.hex(),
            'stage_palette': stage_palette.hex(),
            'stage_arrays': sha256(stage_arrays).hexdigest(),
            'stage_frames': [sha256(frame).hexdigest()
                             for frame in atlas_frames(header, stage, 61, 71, 8)],
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old-headers', type=Path, help='also decode the archived pre-overlay headers')
    args = parser.parse_args()
    fixture = json.loads(BASELINE.read_text())
    before = fixture['outfits']
    if args.old_headers:
        assert snapshot(args.old_headers) == before, 'old headers differ from the recorded baseline'
    after = snapshot(ROOT / 'firmware/ui', check_retired=True)
    assert list(before) == list(after), 'outfit registry order or coverage changed'
    old_total = new_total = 0
    for slug, old in before.items():
        new = after[slug]
        for field in old:
            if field != 'face_bytes':
                assert old[field] == new[field], (slug, field)
        old_bytes, new_bytes = old['face_bytes'], new['face_bytes']
        assert new_bytes < old_bytes, slug
        print(f'{slug}: {old_bytes} -> {new_bytes} bytes ({old_bytes - new_bytes} saved)')
        old_total += old_bytes
        new_total += new_bytes
    print(f'Total: {old_total} -> {new_total} bytes ({old_total - new_total} saved)')


if __name__ == '__main__':
    main()

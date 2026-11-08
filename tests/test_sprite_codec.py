#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host generator gates, stdlib only; python3 -I tests/test_sprite_codec.py."""
import importlib.util
from pathlib import Path
import random
import unittest
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('sprite_compact', ROOT / 'tools/sprite_compact.py')
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


class CompactTests(unittest.TestCase):
    def test_generated_deterministic_and_current(self):
        src = (ROOT / 'firmware/ui/singer_sprites.h').read_text()
        first, report = codec.generate(src)
        second, report2 = codec.generate(src)
        self.assertEqual((first, report), (second, report2))
        self.assertEqual(first, (ROOT / 'firmware/ui/singer_compact.h').read_text())

    def test_random_odd_even_rows(self):
        rng = random.Random(390006)
        for width in (1, 3, 16, 61, 90, 255):
            base = [rng.randrange(16) for _ in range(width * 5)]
            changed = base[:]
            changed[width + width // 2] ^= 15
            frames = [base, changed, base[:], [0] * len(base)]
            for delta in (False, True):
                packed = codec.pack_atlas(frames, width, 5, delta)
                for fi, frame in enumerate(frames):
                    for y in range(5):
                        expected = frame[y * width:(y + 1) * width]
                        self.assertEqual(codec.decode_python(packed, packed[0][fi * 5 + y]), expected)

    def test_invalid_source_rejected(self):
        src = (ROOT / 'firmware/ui/singer_sprites.h').read_text()
        src = src.replace('singer_face_0', 'missing_face_0')
        with self.assertRaisesRegex(ValueError, 'missing array'):
            codec.generate(src)


if __name__ == '__main__':
    unittest.main()

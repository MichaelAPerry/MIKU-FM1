#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Wardrobe pipeline fixture tests. Synthetic fixtures are NEVER production art."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('wardrobe_pack', ROOT / 'tools/wardrobe_pack.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


def fixture(path):
    image = Image.new('RGB', (128, 160), (255, 0, 255))
    draw = ImageDraw.Draw(image)
    for i in range(20):
        x, y = i % 4 * 32, i // 4 * 32
        draw.rectangle((x + 10, y + 3, x + 23, y + 29), fill=(10, 180, 180))
        draw.rectangle((x + 13, y + 7, x + 20, y + 18), fill=(250, 220, 180))
        draw.rectangle((x + 15, y + 11, x + 18, y + 12 + i % 4), fill=(16, 20, 40))
        draw.rectangle((x + 4 + i % 4, y + 18, x + 11, y + 22), fill=(40, 50, 80))
    image.save(path)


class WardrobeTests(unittest.TestCase):
    def test_original_and_pending(self):
        text, report = pack.generate({'version': 1, 'outfits': [dict(id='future', name='FUTURE', status='pending')]}, ROOT)
        self.assertIn('#define WD_OUTFIT_N 1u', text)
        self.assertEqual(report['additional_bytes'], 0)
        self.assertIn('&SC_FACE_ATLAS', text)

    def test_complete_roundtrip_and_report(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            fixture(base / 'sheet.png')
            m = {'version': 1, 'outfits': [dict(id='test', name='TEST', status='prototype', sheet='sheet.png', layout='paired4x5')]}
            a, r = pack.generate(m, base, base / 'preview')
            b, r2 = pack.generate(m, base)
            self.assertEqual((a, r), (b, r2))
            self.assertIn('#define WD_OUTFIT_N 2u', a)
            self.assertEqual(r['outfit_count'], 2)
            self.assertEqual(len(r['outfits'][0]['sources'][0]['sha256']), 64)
            self.assertEqual(len(r['outfits'][0]['faces']['opaque_pixels']), 12)
            self.assertEqual(len(r['outfits'][0]['stage']['opaque_pixels']), 8)
            self.assertEqual(r['outfits'][0]['faces']['target_size'], [90, 90])
            self.assertTrue((base / 'preview/test-faces.png').exists())
            m['budget_bytes'] = 1
            with self.assertRaises(pack.BudgetError) as caught:
                pack.generate(m, base)
            self.assertGreater(caught.exception.report['additional_bytes'], 1)

    def test_schema_missing_ready_and_collisions(self):
        for m in ({'version': 2, 'outfits': []}, {'version': 1, 'outfits': [dict(id='original', name='BAD')]}):
            with self.assertRaises(ValueError):
                pack.generate(m, ROOT)
        m = {'version': 1, 'outfits': [dict(id='missing', name='MISSING', status='prototype', sheet='missing.png')]}
        with self.assertRaises(FileNotFoundError):
            pack.generate(m, ROOT)
        m['outfits'] = [dict(id='test-a', name='A'), dict(id='test_a', name='B')]
        with self.assertRaisesRegex(ValueError, 'collision'):
            pack.generate(m, ROOT)

    def test_blank_art_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            Image.new('RGB', (128, 160), (255, 0, 255)).save(base / 'sheet.png')
            m = {'version': 1, 'outfits': [dict(id='blank', name='BLANK', status='prototype', sheet='sheet.png')]}
            with self.assertRaisesRegex(ValueError, 'empty'):
                pack.generate(m, base)


if __name__ == '__main__':
    unittest.main()

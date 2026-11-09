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
    def test_unequal_black_grid(self):
        im=Image.new('RGB',(400,500),(255,0,255));d=ImageDraw.Draw(im)
        ys=[0,95,190,280,390,500]
        for x in [0,100,200,300,399]:d.line((x,0,x,499),fill=(0,0,0),width=2)
        for y in ys:d.line((0,min(y,499),399,min(y,499)),fill=(0,0,0),width=2)
        for y0,y1 in zip(ys,ys[1:]):
            for x in range(4):d.rectangle((x*100+25,y0+6,x*100+75,y1-6),fill=(20,180,180))
        xb,yb=pack.grid_boundaries(im,4,5)
        self.assertTrue(all(abs(a-b)<=2 for a,b in zip(yb,ys)),yb)
        self.assertTrue(all(abs(a-b)<=2 for a,b in zip(xb,[0,100,200,300,400])),xb)

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

    def test_shared_head_and_real_tilts(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / 'sheet.png'
            fixture(image)
            with Image.open(image) as im:
                frames = pack.cells(im, 4, 5)[:12]
            frames, pal, _ = pack.normalize(frames, 90, 90)
            result, info = pack.compose_portraits(frames, pal)
            mask = pack.np.ones((90, 90), bool)
            x, y, w, h = info['expression_box']
            mask[y:y + h, x:x + w] = False
            for i in range(1, 10):
                self.assertTrue(pack.np.array_equal(result[i][mask], result[0][mask]))
            self.assertTrue(pack.np.array_equal(result[10], frames[10]))
            self.assertTrue(pack.np.array_equal(result[11], frames[11]))

    def test_recover_real_gutter_before_body(self):
        im = Image.new('RGB', (128, 160), (255, 0, 255))
        draw = ImageDraw.Draw(im)
        boundaries = [0, 30, 60, 90, 125, 160]
        for y in range(5):
            for x in range(4):
                draw.rectangle((x * 32 + 8, boundaries[y] + 2,
                                x * 32 + 23, boundaries[y + 1] - 3), fill=(20, 180, 180))
        _, rows = pack.grid_boundaries(im, 4, 5)
        self.assertLess(rows[3], 94)  # equal96 would admit next tile's hair
        self.assertGreater(rows[3], 86)

    def test_blank_art_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            Image.new('RGB', (128, 160), (255, 0, 255)).save(base / 'sheet.png')
            m = {'version': 1, 'outfits': [dict(id='blank', name='BLANK', status='prototype', sheet='sheet.png')]}
            with self.assertRaisesRegex(ValueError, 'empty'):
                pack.generate(m, base)


if __name__ == '__main__':
    unittest.main()

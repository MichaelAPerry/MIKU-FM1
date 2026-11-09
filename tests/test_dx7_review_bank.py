# SPDX-License-Identifier: GPL-3.0-only
"""Offline DX7 shortlist extraction: checksum, format and identity validation."""
from pathlib import Path
import importlib.util
import io
import unittest
import zipfile
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('review', ROOT/'tools/prepare_dx7_review_bank.py')
review = importlib.util.module_from_spec(spec);spec.loader.exec_module(review)

def bank():
    data = bytearray(4096)
    for i in range(32):
        data[i*128+110] = i
        data[i*128+118:(i+1)*128] = ('PATCH%05d' % i).encode()
    return b'\xf0\x43\x00\x09\x20\x00'+data+bytes([(-sum(data)) & 127,247])

class ReviewBank(unittest.TestCase):
    def test_selected_voice_identity_and_order(self):
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, 'w') as z:z.writestr('bank.syx', bank())
        plan = {'voices': [{'name': 'PATCH00031', 'bank': 'bank.syx'}, {'name': 'PATCH00000', 'bank': 'bank.syx'}]}
        data, report = review.prepare(archive, plan)
        self.assertEqual(len(data), 256)
        self.assertEqual(data[118:128], b'PATCH00031')
        self.assertEqual(report['voices'][0]['algorithm_1based'], 32)
        self.assertEqual(report['voices'][1]['source_program_1based'], 1)
        plan['voices'][0]['name'] = 'MISSING'
        with self.assertRaises(ValueError):review.prepare(archive, plan)
    def test_reject_corruption_and_nonstandard_packets(self):
        valid = bank()
        for bad in [valid[:-1], valid+b'\x00', valid[:3]+b'\x08'+valid[4:], valid[:-2]+b'\x00\xf7', valid[:20]+b'\x80'+valid[21:]]:
            with self.assertRaises(ValueError):review.voices(bad)

if __name__ == '__main__':unittest.main()

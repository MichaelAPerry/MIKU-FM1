#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('bench',Path(__file__).resolve().parents[1]/'tools/video_bench_report.py')
bench=importlib.util.module_from_spec(spec);spec.loader.exec_module(bench)
class ListingTests(unittest.TestCase):
    def test_actual_dump_and_instruction_byte_order(self):
        m=bench.memory('  01c0fe54  8391 ?\n  01c0fe56  00 00              nop\n  0x01c5ec00: e0ffd8ff 464a1000\n')
        self.assertEqual(bytes(m[0x01c0fe54+i] for i in range(4)),bytes.fromhex('91830000'))
        self.assertEqual(bytes(m[0x01c5ec00+i] for i in range(4)),bytes.fromhex('ffd8ffe0'))
        self.assertEqual(bench.memory('ignored\n'),{})
if __name__=='__main__':unittest.main()

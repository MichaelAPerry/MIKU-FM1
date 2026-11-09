# SPDX-License-Identifier: GPL-3.0-only
"""Address compatibility and fail-closed patching of the pinned storage map."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('slots',ROOT/'tools/patch_dinghy_slots.py')
slots=importlib.util.module_from_spec(spec);spec.loader.exec_module(slots)
class Slots(unittest.TestCase):
    def test_layout(self):
        old=[0x97000+i*0x4000 for i in range(8)]
        new=[0x97000+i*0x4000 for i in range(10)]
        self.assertEqual(old,new[:8]);self.assertEqual(new[-1]+0x4000,0xbf000)
        self.assertLessEqual(new[-1]+0x4000,0xe0000)
    def test_patch(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'firmware/src/store.h';p.parent.mkdir(parents=True)
            src='#define NSLOT 8u\n#define SLOT_SECT 4u\n#define SLOTS_AT 0x00097000u\n'
            p.write_text(src);slots.patch(tmp)
            self.assertEqual(p.read_text(),src.replace('NSLOT 8u','NSLOT 10u'))
            with self.assertRaises(ValueError):slots.patch(tmp)
if __name__=='__main__':unittest.main()

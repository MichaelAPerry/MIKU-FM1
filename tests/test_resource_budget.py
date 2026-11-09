#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import pathlib
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('resources', ROOT/'tools/check_resources.py')
resources = importlib.util.module_from_spec(spec)
spec.loader.exec_module(resources)

class BudgetTests(unittest.TestCase):
    def setUp(self):
        self.budget = {name: {'limit': 1000, 'minimum_free': 100} for name in ['image','static_ram','pool']}
    def symbols(self, ram=900, pool=900):
        return f'{0x01C08000+ram:08x} g       .bss 00000000 _bss_end\n{0x01C20000+pool:08x} g       .pool 00000000 _pool_end\n'
    def test_boundary_and_each_reserve(self):
        self.assertTrue(all(e['pass'] for e in resources.measure(900,self.symbols(),self.budget).values()))
        self.assertFalse(resources.measure(901,self.symbols(),self.budget)['image']['pass'])
        self.assertFalse(resources.measure(900,self.symbols(901,900),self.budget)['static_ram']['pass'])
        self.assertFalse(resources.measure(900,self.symbols(900,1001),self.budget)['pool']['pass'])
    def test_zero_flash_reserve_still_rejects_overflow(self):
        self.budget['image']['minimum_free']=0
        self.assertTrue(resources.measure(1000,self.symbols(),self.budget)['image']['pass'])
        self.assertFalse(resources.measure(1001,self.symbols(),self.budget)['image']['pass'])
        self.assertFalse(resources.measure(1000,self.symbols(1001,900),self.budget)['static_ram']['pass'])

    def test_fail_closed(self):
        for symbols in ['', self.symbols()+self.symbols(), self.symbols(-1,900)]:
            with self.assertRaises(ValueError): resources.measure(900,symbols,self.budget)
        with self.assertRaises(ValueError): resources.measure(0,self.symbols(),self.budget)

if __name__ == '__main__': unittest.main()

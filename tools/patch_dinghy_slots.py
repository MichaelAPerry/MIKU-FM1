#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Expand native slot count without moving any existing save address."""
from pathlib import Path
import sys

def patch(root):
    p=Path(root)/'firmware/src/store.h'
    s=p.read_text();old='#define NSLOT 8u';new='#define NSLOT 10u'
    if s.count(old)!=1:raise ValueError('pinned Dinghy slot declaration changed')
    # FM-1: 0x97000 + 10*4*4096 = 0xbf000. Existing slots retain
    # their 16KB stride. Guard against overlap with blobs/settings/protection.
    assert 0x97000+10*4*4096<=0xe0000
    assert 0x97000+10*4*4096<=0xf4000
    p.write_text(s.replace(old,new))

if __name__=='__main__':patch(sys.argv[1])

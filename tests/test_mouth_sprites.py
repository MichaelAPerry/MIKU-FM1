#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Decode the shipped atlas with the firmware codec and compare its indexed PNG."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from PIL import Image

root, cc = Path(sys.argv[1]), sys.argv[2]
source = r'''
#include <assert.h>
#include <stdio.h>
#include "firmware/audio/fvoice/fvoice.h"
#include "firmware/ui/mouth_sprites.h"
#include "firmware/ui/wardrobe_sprites.h"

int main(void) {
    assert(MOUTH_COUNT == 12 && MOUTH_COUNT == FVV_N);
    assert(MOUTH_ATLAS.frames == MOUTH_COUNT);
    assert(MOUTH_ATLAS.width == MOUTH_W && MOUTH_ATLAS.height == MOUTH_H);
    assert(MOUTH_OUTFIT_COUNT == WD_OUTFIT_N);
    assert(sizeof MOUTH_ANCHORS == 2 * WD_OUTFIT_N);
    assert(sizeof MOUTH_PAL / sizeof MOUTH_PAL[0] <= 16);
    assert(MOUTH_FLASH_BYTES == sizeof MOUTH_ATLAS_ROWS + sizeof MOUTH_ATLAS_OFFSETS
        + sizeof MOUTH_ATLAS_DATA + sizeof MOUTH_PAL + sizeof MOUTH_ANCHORS
        + sizeof MOUTH_CLEAN_OFFSETS + sizeof MOUTH_CLEAN_PIXELS + 24);
    assert(MOUTH_FLASH_BYTES <= 6000);
    assert(sizeof MOUTH_CLEAN_OFFSETS == WD_OUTFIT_N + 1);
    assert(MOUTH_CLEAN_OFFSETS[0] == 0);
    assert(2 * MOUTH_CLEAN_OFFSETS[WD_OUTFIT_N] == sizeof MOUTH_CLEAN_PIXELS);
    for (unsigned i = 0; i < WD_OUTFIT_N; ++i) {
        unsigned x = MOUTH_ANCHORS[2*i], y = MOUTH_ANCHORS[2*i+1];
        assert(x >= 25 && y >= 28 && x + MOUTH_W <= 67 && y + MOUTH_H <= 67);
        assert(MOUTH_CLEAN_OFFSETS[i] <= MOUTH_CLEAN_OFFSETS[i+1]);
        for (unsigned j = MOUTH_CLEAN_OFFSETS[i]; j < MOUTH_CLEAN_OFFSETS[i+1]; ++j) {
            assert(MOUTH_CLEAN_PIXELS[2*j] < MOUTH_W * MOUTH_H);
            assert(MOUTH_CLEAN_PIXELS[2*j+1] > 0 && MOUTH_CLEAN_PIXELS[2*j+1] < 16);
        }
    }
    assert(spr_validate(&MOUTH_ATLAS));
    for (unsigned f = 0; f < MOUTH_COUNT; ++f) {
        for (unsigned y = 0; y < MOUTH_H; ++y) {
            uint8_t row[MOUTH_W];
            assert(spr_decode_row(&MOUTH_ATLAS, f, y, row, sizeof row));
            for (unsigned x = 0; x < MOUTH_W; ++x)
                assert(row[x] < sizeof MOUTH_PAL / sizeof MOUTH_PAL[0]);
            assert(fwrite(row, 1, sizeof row, stdout) == sizeof row);
        }
    }
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'mouth.c').write_text(source)
    subprocess.run([cc, '-std=c11', '-Wall', '-Wextra', '-I' + str(root),
                    str(path / 'mouth.c'), str(root / 'firmware/ui/sprite_codec.c'),
                    '-o', str(path / 'mouth')], check=True)
    decoded = subprocess.check_output([str(path / 'mouth')])

header = (root / 'firmware/ui/mouth_sprites.h').read_text()
w, h = (int(re.search(fr'#define MOUTH_{axis} (\d+)u', header)[1]) for axis in ('W', 'H'))
palette = re.search(r'MOUTH_PAL\[\d+\] = \{([^}]+)\}', header)[1]
rgb565 = [int(color) for color in re.findall(r'\d+', palette)]
with Image.open(root / 'assets/singer/mouths.png') as sheet:
    assert sheet.mode == 'P' and sheet.info['transparency'] == 0
    assert 'Piapro Character License' in sheet.info['Attribution']
    channels = sheet.getpalette()
    assert rgb565 == [
        (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
        for r, g, b in zip(channels[::3], channels[1::3], channels[2::3])
    ][:len(rgb565)]
    margin = 28
    cell = (sheet.width - margin) // 12
    frames = []
    for i in range(12):
        x = margin + i*cell + (cell-w)//2
        small = sheet.crop((x, 19, x+w, 19+h))
        x = margin + i*cell + (cell-8*w)//2
        large = sheet.crop((x, 39, x+8*w, 39+8*h))
        assert small.resize(large.size, Image.Resampling.NEAREST).tobytes() == large.tobytes()
        frames.append(small.tobytes())
    assert len(set(frames)) == 12
    assert decoded == b''.join(frames)
print('12 mouth sprites round-trip through spr_decode_row at 1x and 8x')

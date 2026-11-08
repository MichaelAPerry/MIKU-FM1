#!/usr/bin/env python3
"""Check the actual native DANCE pixel decoder against packed linear 4bpp atlas."""
import pathlib
import subprocess
import sys
import tempfile
root, cc = pathlib.Path(sys.argv[1]), sys.argv[2]
s = (root/'native/dinghy.c').read_text()
s = s[s.index('static void dance_draw('):]
line = next(line.strip() for line in s.splitlines() if 'b=d[' in line)
source = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "firmware/ui/singer_sprites.h"
int main(void){
  uint32_t w=SINGER_STAGE_W, mismatches=0;
  for(unsigned f=0;f<8;f++){
    const uint8_t *d=SINGER_STAGE[f];
    for(uint32_t r0=0;r0<SINGER_STAGE_H;r0+=8){
      for(uint32_t r=0;r<8 && r0+r<SINGER_STAGE_H;r++)for(uint32_t c=0;c<w;c++){
        DECODE
        uint32_t pos=(r0+r)*w+c;
        uint32_t expected=pos&1?d[pos/2]&15:d[pos/2]>>4;
        mismatches+=i!=expected;
      }
    }
  }
  printf("native DANCE decoder: %u mismatches\n",mismatches);
  assert(!mismatches);
}
'''.replace('DECODE',line)
with tempfile.TemporaryDirectory() as td:
    path=pathlib.Path(td);(path/'sprite.c').write_text(source)
    subprocess.run([cc,'-std=c11','-O2','-I'+str(root),str(path/'sprite.c'),'-o',str(path/'sprite')],check=True)
    subprocess.run([str(path/'sprite')],check=True)

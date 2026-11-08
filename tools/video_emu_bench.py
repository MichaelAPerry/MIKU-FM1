#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run three target-side JPEG frames under vocal+band load; never access USB hardware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--emu',type=Path,default=ROOT/'build/fm1-emu/target/release/fm1-emu')
    p.add_argument('--package',type=Path,default=ROOT/'build/native/miku-fm-video-bench.fwsc')
    p.add_argument('--symbols',type=Path,default=ROOT/'build/native/miku-fm-video-bench-symbols.txt')
    p.add_argument('--out',type=Path,default=ROOT/'build/evidence/video-bench')
    a=p.parse_args();out=a.out.resolve();(out/'work').mkdir(parents=True,exist_ok=True)
    symbols=a.symbols.read_text();names=['video_jpeg','video_encoded','video_step_ticks','video_frame_ticks','video_last_bytes','video_failed']
    addresses={name:int(re.search(r'^([0-9a-f]+) .*\s'+name+r'$',symbols,re.M).group(1),16) for name in names}
    (out/'symbols.json').write_text(json.dumps(addresses,indent=2)+'\n')
    package=a.package.resolve();manifest={'package':str(package),'sha256':hashlib.sha256(package.read_bytes()).hexdigest(),
        'scope':'emulator-only offline JPEG benchmark, not USB camera throughput'}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    env=os.environ.copy();env['FM1_LIST_END']=','.join(f'{addresses[n]:x}:4' for n in names if n!='video_jpeg')
    cmd=[str(a.emu.resolve()),'--fwsc',str(package),'--ms','2000','--keys',
         '100:HOME,200:PLAY,250-1700:n7','--snap','1200','--wav','audio.wav',
         '--dump',f'{addresses["video_jpeg"]:x}','2048']
    with (out/'boot.log').open('w') as f: subprocess.run(cmd,cwd=out,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
    print(out/'boot.log')
    print('Decode/report: python3 tools/video_bench_report.py',out)

if __name__=='__main__':main()

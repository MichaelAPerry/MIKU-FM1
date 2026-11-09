#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Measure an actual package in fm1-emu under generated full-band DX7 playback.
No USB hardware is exercised. Missing counters, silence and overruns fail.
"""
import argparse
import array
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import wave
from video_bench_report import memory

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--emu',type=Path,required=True)
    p.add_argument('--package',type=Path,required=True)
    p.add_argument('--symbols',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    p.add_argument('--page',choices=['stage','dance'],required=True)
    p.add_argument('--ms',type=int,default=35000)
    p.add_argument('--allow-overruns',action='store_true',help='retain failing baseline measurements; never use for candidate acceptance')
    a=p.parse_args()
    if a.ms<8000:p.error('measure at least eight seconds')
    out=a.out.resolve();(out/'work').mkdir(parents=True,exist_ok=True)
    symbols=a.symbols.read_text()
    names=['audio_calls','audio_ticks','audio_frames','audio_hard_peak','audio_overruns','audio_dx_tracks','audio_playing_frames','stage_profile','dance_profile']
    addresses={}
    for name in names:
        m=re.search(r'^([0-9a-fA-F]+) .*\s'+name+r'$',symbols,re.M)
        if not m:raise SystemExit('missing measured symbol: '+name)
        addresses[name]=int(m[1],16)
    # Generate HYPERPOP (both DX7 tracks), keep, return to STAGE/DANCE, PLAY.
    events=[f'{100+200*i}:HOME' for i in range(5)]
    events+=['1200:REC','1500:EDIT']+[f'{1700+200*i}:HOME' for i in range(5)]
    if a.page=='dance':events+=['2700:HOME']
    events+=['3100:PLAY']
    env=os.environ.copy()
    env['FM1_LIST_END']=','.join(f'{addresses[n]:x}:{24 if n.endswith("profile") else 4:x}' for n in names)
    command=[str(a.emu.resolve()),'--fwsc',str(a.package.resolve()),'--ms',str(a.ms),'--keys',','.join(events),
             '--snap','5000,7000','--wav','audio.wav']
    with (out/'boot.log').open('w') as log:
        subprocess.run(command,cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
    log=(out/'boot.log').read_text();mem=memory(log)
    def word(name,offset=0):return int.from_bytes(bytes(mem[addresses[name]+offset+i] for i in range(4)),'little')
    fields=['frames','last_ticks','interval_min_ticks','interval_max_ticks','draw_max_ticks','draw_total_ticks']
    counters={name:word(name) for name in names if not name.endswith('profile')}
    profile={field:word(a.page+'_profile',i*4) for i,field in enumerate(fields)}
    with wave.open(str(out/'audio.wav'),'rb') as w:
        frames=w.getnframes();samples=array.array('h',w.readframes(frames))
    elapsed=a.ms*1000
    report={'scope':'CPU emulator; generated full-band DX7; not USB or hardware qualification','page':a.page,
        'package_sha256':hashlib.sha256(a.package.read_bytes()).hexdigest(),'run_ms':a.ms,
        'audio':counters,'draw':profile,'audio_peak':max(map(abs,samples),default=0),'wav_frames':frames,
        'audio_worst_us':counters['audio_hard_peak']/24,
        'audio_isr_share_percent':float(re.search(r'cpu0: ([0-9.]+)% in an ISR',log)[1]),
        'audio_render_share_percent':100*counters['audio_ticks']/(24*elapsed),
        'draw_mean_us':profile['draw_total_ticks']/24/max(profile['frames'],1),
        'draw_worst_us':profile['draw_max_ticks']/24,
        'interval_min_ms':profile['interval_min_ticks']/24000,
        'interval_max_ms':profile['interval_max_ticks']/24000}
    (out/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if not counters['audio_calls'] or profile['frames']<2 or frames<10000 or not report['audio_peak']:
        raise SystemExit('incomplete playback/draw measurement')
    if counters['audio_dx_tracks']!=3 or counters['audio_playing_frames']<44100:
        raise SystemExit('full-band DX7 song was not exercised')
    if counters['audio_overruns'] and not a.allow_overruns:raise SystemExit('audio callback deadline overruns')
if __name__=='__main__':main()

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Add the identical candidate timing probes to an isolated v0.7 source archive.
Does not backport optimizations or alter the selected checkout. The archive must
match v0.7 native/dinghy.c byte-for-byte before adding probes.
"""
from pathlib import Path
import argparse
import hashlib
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def function(s,name):
    start=s.index('static void '+name+'(');a=s.index('{',start);b=a+1;depth=1
    while depth:
        depth+=(s[b]=='{')-(s[b]=='}');b+=1
    return start,b,s[start:b]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('archive',type=Path);a=p.parse_args()
    target=a.archive/'native/dinghy.c';s=target.read_text()
    original=subprocess.check_output(['git','show','v0.7:native/dinghy.c'],cwd=ROOT,text=True)
    if s!=original:raise SystemExit('baseline must be an unmodified v0.7 archive')
    current=(ROOT/'native/dinghy.c').read_text()
    start=current.index('/* Debugger-readable cumulative telemetry.')
    end=current.index('/* ---- small helpers */',start)
    lo,hi,_=function(s,'app_audio');s=s[:lo]+current[start:end]+s[hi:]
    old='face_draw(f,x0,y0,x1-x0,y1-y0);'
    new='{uint32_t t=draw_start(&stage_profile);'+old+'draw_end(&stage_profile,t);}'
    if s.count(old)!=1:raise SystemExit('face timing anchor changed')
    s=s.replace(old,new)
    old='dance_cur=f;dance_draw(f);'
    new='dance_cur=f;{uint32_t t=draw_start(&dance_profile);dance_draw(f);draw_end(&dance_profile,t);}'
    if s.count(old)!=1:raise SystemExit('dance timing anchor changed')
    s=s.replace(old,new)
    needle='ppos=en.pos;playing=ppos>>16&1u;'
    reset=current[current.index(needle):current.index('        b=fm1_input_edges',current.index(needle))]
    if s.count(needle)!=1:raise SystemExit('playback transition anchor changed')
    s=s.replace(needle,reset.rstrip())
    target.write_text(s)
    print('instrumented v0.7 native source sha256='+hashlib.sha256(s.encode()).hexdigest())
if __name__=='__main__':main()

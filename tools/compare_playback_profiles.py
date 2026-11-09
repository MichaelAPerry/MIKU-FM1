#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare matched full-song emulator telemetry; never substitute host timings."""
import argparse
import json
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path)
    p.add_argument('--max-dance-jitter-ms',type=float,default=10.0,help='10%% of the 100 ms DANCE update period')
    a=p.parse_args();report={'scope':'CPU emulator only','passed':True,'pages':{}}
    for page in ('stage','dance'):
        before=json.loads((a.directory/(page+'-baseline')/'measurements.json').read_text())
        after=json.loads((a.directory/(page+'-default')/'measurements.json').read_text())
        spread=after['interval_max_ms']-after['interval_min_ms']
        result={'audio_worst_before_us':before['audio_worst_us'],'audio_worst_after_us':after['audio_worst_us'],
            'audio_overruns':after['audio']['audio_overruns'],'draw_before_us':before['draw_mean_us'],
            'draw_after_us':after['draw_mean_us'],'interval_spread_after_ms':spread,
            'audio_worst_unchanged_or_better':after['audio_worst_us']<=before['audio_worst_us']}
        result['passed']=result['audio_worst_unchanged_or_better'] and not result['audio_overruns']
        if page=='dance':
            result['steady_frame_interval']=spread<=a.max_dance_jitter_ms
            result['passed']=result['passed'] and result['steady_frame_interval']
        # STAGE mouths intentionally follow note articulation, not a periodic pose loop.
        report['pages'][page]=result;report['passed']=report['passed'] and result['passed']
    (a.directory/'playback-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if not report['passed']:raise SystemExit('playback acceptance failed; inspect measured evidence')
if __name__=='__main__':main()

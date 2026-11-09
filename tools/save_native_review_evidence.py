#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Retain only measured JSON and rendered image evidence; exclude binaries/audio; bounded logs only on failed runs."""
import argparse
import json
from pathlib import Path
import shutil

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('destination',type=Path);p.add_argument('commit');p.add_argument('--incomplete',action='store_true');a=p.parse_args()
    if len(a.commit)!=40 or any(c not in '0123456789abcdef' for c in a.commit):p.error('expected full verified commit SHA')
    required=['stage-baseline/measurements.json','dance-baseline/measurements.json','stage-default/measurements.json','dance-default/measurements.json','pages-before/captures.json','pages-after/captures.json','jpeg-bench/measurements.json']
    for name in required:
        if not a.incomplete and not (a.source/name).is_file():raise SystemExit('missing acceptance evidence: '+name)
    a.destination.mkdir(parents=True,exist_ok=True)
    manifest={'verified_code_commit':a.commit,'validation_complete':not a.incomplete,'scope':'Native builds and CPU emulator; physical USB/OS/hardware unverified','files':[]}
    for source in sorted(a.source.rglob('*')):
        if not source.is_file() or source.suffix not in ('.json','.png','.jpg','.log'):continue
        if source.suffix=='.log' and not a.incomplete:continue
        relative=source.relative_to(a.source);dest=a.destination/relative;dest.parent.mkdir(parents=True,exist_ok=True)
        if source.suffix=='.log':
            text=source.read_text(errors='replace')
            if len(text)>2000000:text=text[:1000000]+'\n[log middle omitted]\n'+text[-1000000:]
            dest.write_text(text)
        else:shutil.copyfile(source,dest)
        manifest['files'].append(relative.as_posix())
    (a.destination/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if a.incomplete:
        (a.destination/'README.md').write_text('# Incomplete native validation\n\nCommit `'+a.commit+'` did not complete acceptance checks. Retained JSON/images and compiler/emulator log tails are diagnostic evidence only. See manifest.json.\n')
        return
    rows=['# Native 0.8 review evidence' ,'',f'Generated from verified code `{a.commit}`. CPU emulator only; no physical USB or device qualification.','', '| Track / page | v0.7 baseline | 0.8 candidate |','| --- | --- | --- |']
    capture=json.loads((a.source/'pages-before/captures.json').read_text())
    for item in capture['captures']:
        name=item['file'];rows.append(f'| {item["track"]} / {item["page"]} | ![Before](pages-before/{name}) | ![After](pages-after/{name}) |')
    rows+=['','Measurements: [STAGE baseline](stage-baseline/measurements.json), [STAGE candidate](stage-default/measurements.json), [DANCE baseline](dance-baseline/measurements.json), [DANCE candidate](dance-default/measurements.json), [JPEG benchmark](jpeg-bench/measurements.json).','']
    (a.destination/'README.md').write_text('\n'.join(rows))
if __name__=='__main__':main()

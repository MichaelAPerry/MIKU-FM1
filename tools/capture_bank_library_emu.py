#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Capture real BANK pages for chords/bass and stopped/playing LIBRARY in fm1-emu."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from PIL import Image

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--emu',type=Path,required=True);p.add_argument('--package',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    manifest={'scope':'CPU emulator screenshots, not device screenshots','package_sha256':hashlib.sha256(a.package.read_bytes()).hexdigest(),'captures':[]}
    for track,button in [('chords','SCL'),('bass','ENV')]:
        run=out/track;(run/'work').mkdir(parents=True,exist_ok=True)
        events=[f'{100+200*i}:HOME' for i in range(8)]
        events+=['2200:PLAY','2800:HOME',f'3100:{button}','3600:REC','3900:EDIT']
        snapshots={2000:'library-stopped',2600:'library-playing',3400:'bank-00-browse',7000:'bank-01-global'}
        for page in range(2,11):
            when=7200+(page-2)*600;events.append(f'{when}:PRESET:+1');snapshots[when+400]=f'bank-{page:02d}'
        cmd=[str(a.emu.resolve()),'--fwsc',str(a.package.resolve()),'--ms','12800','--keys',','.join(events),
             '--snap',','.join(map(str,snapshots))]
        with (run/'boot.log').open('w') as log:subprocess.run(cmd,cwd=run,stdout=log,stderr=subprocess.STDOUT,check=True)
        for ms,label in snapshots.items():
            source=run/'work'/f'lcd_{ms}.bmp'
            with Image.open(source) as image:
                image.load()
                if image.size!=(240,240) or len(image.convert('RGB').getcolors(65536) or [])<3:
                    raise SystemExit('missing or blank screen: '+str(source))
                path=out/f'{track}-{label}.png';image.save(path)
            manifest['captures'].append({'track':track,'page':label,'time_ms':ms,'file':path.name})
    (out/'captures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(manifest["captures"])} real emulator screenshots: {out}')
if __name__=='__main__':main()

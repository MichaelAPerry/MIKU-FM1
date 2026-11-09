#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Recover native JPEG and measured counters from fm1-emu dump/listing evidence."""
import argparse
import json
from pathlib import Path
import re
import wave
import array


def memory(log):
    result={}
    for line in log.splitlines():
        dump=re.match(r'\s+0x([0-9a-f]{8}):((?: [0-9a-f]{8})+)\s*$',line)
        if dump:
            at=int(dump[1],16)
            data=b''.join(int(word,16).to_bytes(4,'little') for word in dump[2].split())
        else:
            unknown=re.match(r'\s+([0-9a-f]{8})\s+([0-9a-f]{4}) \?$',line)
            known=re.match(r'\s+([0-9a-f]{8})\s+((?:[0-9a-f]{2} ){1,3}[0-9a-f]{2})\s{2,}',line)
            if unknown:
                # Unknown instruction prints a numeric u16; recognized instructions print raw byte pairs.
                at=int(unknown[1],16);data=int(unknown[2],16).to_bytes(2,'little')
            elif known:
                at=int(known[1],16);data=bytes.fromhex(known[2])
            else: continue
        for i,value in enumerate(data): result[at+i]=value
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args=parser.parse_args();out=args.directory
    log=(out/'boot.log').read_text();mem=memory(log);symbols=json.loads((out/'symbols.json').read_text())
    def word(name): return int.from_bytes(bytes(mem[symbols[name]+i] for i in range(4)),'little')
    n=word('video_last_bytes')
    if not 4<=n<=8192 or word('video_encoded')!=3 or word('video_failed'):
        raise SystemExit('native benchmark incomplete or encoder failed')
    jpeg=bytes(mem[symbols['video_jpeg']+i] for i in range(n))
    if jpeg[:2]!=b'\xff\xd8' or jpeg[-2:]!=b'\xff\xd9': raise SystemExit('bad recovered JPEG')
    (out/'actual-native.jpg').write_bytes(jpeg)
    from PIL import Image
    with Image.open(out/'actual-native.jpg') as image:
        image.load()
        if image.size!=(160,120): raise SystemExit('wrong JPEG dimensions')
        image.save(out/'actual-native.png')
    with wave.open(str(out/'audio.wav'),'rb') as w:
        frames=w.getnframes();samples=array.array('h',w.readframes(frames))
    report={'encoded_frames':word('video_encoded'),'encoder_failures':word('video_failed'),
            'jpeg_bytes':n,'max_tile_us_emulator':word('video_step_ticks')/24,
            'max_frame_wall_us_emulator':word('video_frame_ticks')/24,
            'audio_frames':frames,'audio_peak':max(abs(x) for x in samples),
            'full_scale_samples':sum(x in (-32768,32767) for x in samples),
            'isr_percent_emulator':float(re.search(r'cpu0: ([0-9.]+)% in an ISR',log)[1]),
            'scope':'three JPEG frames during real native vocal+band playback; not real USB throughput or hardware timing'}
    if 'pub_peak' in symbols:
        report['audio_render_moving_peak_us_emulator']=word('pub_peak')/24
        report['audio_callback_deadline_us']=256/44100*1000000
    if 'audio_hard_peak' in symbols:
        report['audio_render_worst_us_emulator']=word('audio_hard_peak')/24
        report['audio_overruns']=word('audio_overruns')
        report['audio_init_us_emulator']=word('audio_init_ticks')/24
    (out/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__': main()

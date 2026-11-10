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
            known=re.match(r'\s+([0-9a-f]{8})\s+((?:[0-9a-f]{2} ){1,7}[0-9a-f]{2})\s{2,}',line)
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
    manifest=json.loads((out/'manifest.json').read_text()) if (out/'manifest.json').exists() else {}
    expected=manifest.get('expected_frames',3)
    counters={}
    for name in symbols:
        if name=='video_jpeg':continue
        try:counters[name]=word(name)
        except KeyError:counters[name]=None
    (out/'counters.json').write_text(json.dumps({'expected_frames':expected,'counters':counters},indent=2)+'\n')
    if 'halt reason:' in log:raise SystemExit('native video benchmark halted early')
    n=word('video_last_bytes');encoded=word('video_encoded')
    enough=encoded==expected if expected else encoded>=manifest.get('minimum_frames',150)
    if not 4<=n<=8192 or not enough or word('video_failed') or counters.get('video_encoding',0):
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
            'scope':'bounded JPEG frames during real native vocal+band playback; not real USB throughput or hardware timing'}
    if 'video_copy_ticks' in symbols:
        report['max_snapshot_copy_wall_us_emulator']=word('video_copy_ticks')/24
    if 'pub_peak' in symbols:
        report['audio_render_moving_peak_us_emulator']=word('pub_peak')/24
        report['audio_callback_deadline_us']=256/44100*1000000
    if 'audio_hard_peak' in symbols:
        report['audio_render_worst_us_emulator']=word('audio_hard_peak')/24
        report['audio_overruns']=word('audio_overruns')
        report['audio_init_us_emulator']=word('audio_init_ticks')/24
    if 'video_first_ticks' in symbols and encoded>1:
        elapsed=(word('video_last_ticks')-word('video_first_ticks'))&0xffffffff
        report['encoding_window_ms_emulator']=elapsed/24000
        report['encoder_frames_per_second_emulator']=(encoded-1)*24000000/elapsed if elapsed else 0
        report['mean_frame_wall_us_emulator']=word('video_total_ticks')/24/encoded
        report['minimum_jpeg_bytes']=word('video_min_bytes');report['maximum_jpeg_bytes']=word('video_max_bytes')
    if 'audio_dx_tracks' in symbols:
        report['dx7_band_tracks']=word('audio_dx_tracks')
        report['audio_playing_frames']=word('audio_playing_frames')
    (out/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    if manifest.get('run_ms') and frames<(manifest['run_ms']-1000)*44100//1000:
        raise SystemExit('short native benchmark audio: requested duration did not complete')
    if manifest.get('require_dx7') and (report.get('dx7_band_tracks')!=3 or report.get('audio_playing_frames',0)<44100):
        raise SystemExit('native video benchmark did not exercise full-band DX7 playback')
    if report.get('encoding_window_ms_emulator',0)<manifest.get('minimum_encoding_ms',0):
        raise SystemExit('native video encoding did not span the full-song measurement window')
    if report.get('audio_overruns',0):raise SystemExit('audio overruns during native JPEG encoding')

if __name__=='__main__': main()

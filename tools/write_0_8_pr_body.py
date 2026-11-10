#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare a review description from successful native acceptance evidence."""
import argparse
import json
from pathlib import Path
import re

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('commit')
a=p.parse_args()
if not re.fullmatch('[0-9a-f]{40}',a.commit):p.error('expected verified source SHA')
def read(name):return json.loads((a.source/name).read_text())
comparison=read('playback-comparison.json')
if not comparison['passed']:raise SystemExit('refusing a successful PR description for failed acceptance')
stage=read('stage-default/measurements.json');dance=read('dance-default/measurements.json')
video=read('jpeg-bench/measurements.json')
host=re.search(r'100% tests passed(?:, 0 tests failed)? out of (\d+)',(a.source/'host-tests.log').read_text())
if not host:raise SystemExit('missing successful host test summary')
rows=[]
for profile in ('miku-fm','miku-fm-video'):
 r=read(profile+'-resources.json')
 if not all(x['pass'] for x in r.values()):raise SystemExit('resource budget failed')
 rows.append(f"| {profile} | {r['image']['used']:,} | {r['static_ram']['free']:,} | {r['pool']['free']:,} |")
base='https://github.com/MichaelAPerry/MIKU-FM1/blob/feature/0.8-smooth-and-seen/'
body=f'''DX7 playback in 0.7 recomputes patch checksums in every audio block, and DANCE serializes rendering with LCD transfers. This draft moves patch invalidation to locked edits, overlaps rendering and transfer within the existing canvas, caches decoded body/effect rows, and protects musical-clock frame deadlines. Added outfits now keep consistent upright sprite heights.

USB video includes corrected UVC descriptors, generation-checked immutable JPEG ownership, suspend/resume and stalled-endpoint recovery, packet pacing, and completed-transfer counters. It stays a separate 160×120 MJPEG + MIDI profile because UVC and USB audio share EP4. Also included: BANK/LIBRARY layout fixes, checked user-bank name editing, and CC64 for classic chords/bass and the singer. MKS2 v4/v5 and both flash chips remain supported; no heap or libm was added to the audio path.

Verified source: `{a.commit}`. CPU-emulator measurements use timed 12 MHz LCD transfers and full-band DX7 playback:

- {host[1]}/{host[1]} host CTest checks pass, including 729 text-layout states and 720 cached/reference JPEG fixtures. Default/video/benchmark native builds, resource checks and default/video emulator smoke pass.
- DANCE draw mean {dance['draw_mean_us']/1000:.3f} ms; frame intervals {dance['interval_min_ms']:.3f}–{dance['interval_max_ms']:.3f} ms. The unchanged 10 ms interval-spread check passes.
- Worst audio callback: STAGE {stage['audio_worst_us']:.3f} µs and DANCE {dance['audio_worst_us']:.3f} µs, both no higher than the instrumented 0.7 baseline; zero overruns.
- Full-song JPEG run: {video['encoded_frames']} frames over {video['encoding_window_ms_emulator']/1000:.3f} s, {video['encoder_frames_per_second_emulator']:.3f} encoded fps, {video['minimum_jpeg_bytes']}–{video['maximum_jpeg_bytes']} bytes/JPEG. Mean/max frame time {video['mean_frame_wall_us_emulator']/1000:.3f}/{video['max_frame_wall_us_emulator']/1000:.3f} ms; zero encoder failures/audio overruns. These are encoder measurements, not physical USB throughput.
- The real USB driver separately reconstructs 150 fresh frames, then 150 frames during an encoder pause (1 fresh/149 repeated), with 66–67 ms intervals, exact payloads and recovery checks. A separate compressed snapshot keeps the camera cadence independent of encoding; fresh/repeated counters remain distinct. This is protocol evidence, not OS enumeration.
- The native benchmark also measures the bounded snapshot copy in main context: worst {video['max_snapshot_copy_wall_us_emulator']:.3f} µs including preemption. Physical USB-ISR cost remains unverified.

| Profile | Image bytes (≤581564) | Static free (≥16384) | Pool free (≥32768) |
| --- | ---: | ---: | ---: |
'''+ '\n'.join(rows)+f'''

[Before/after emulator screenshots, all BANK pages and both tracks]({base}docs/evidence/smooth-and-seen/native-ci/README.md) · [validation and limitations]({base}docs/VALIDATION.md) · [review handoff]({base}docs/0_8_REVIEW.md)

Remaining owner gates: macOS/Windows/Linux webcam enumeration and physical sustained USB/audio timing are unverified. CLUB STAB, DIGI GROWL and DIGI PLUCK trims await listening decisions; their bytes are unchanged. Claude review and main integration must precede experimental installation on the owner's FM-1. No main push, release tag, deployment or device access occurs in this work.
'''
a.output.write_text(body)

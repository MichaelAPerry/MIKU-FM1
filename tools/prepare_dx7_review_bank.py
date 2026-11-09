#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare an offline DX7 audition payload; this does not install a device bank."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile


def voices(packet):
    if len(packet) != 4104 or packet[:2] != b'\xf0\x43' or packet[2] > 15 or packet[3:6] != b'\x09\x20\x00' or packet[-1] != 247:
        raise ValueError('expected standard Yamaha 32-voice DX7 bank')
    data = packet[6:-2]
    if any(x > 127 for x in data) or packet[-2] != (-sum(data)) & 127:
        raise ValueError('invalid DX7 data or checksum')
    return [data[i:i+128] for i in range(0, 4096, 128)]


def prepare(archive, plan):
    payload = bytearray()
    report = []
    with zipfile.ZipFile(archive) as banks:
        for entry in plan['voices']:
            patches = voices(banks.read(entry['bank']))
            matches = [(i, p) for i, p in enumerate(patches) if p[118:128].decode('ascii').strip(' \x00') == entry['name']]
            if len(matches) != 1:
                raise ValueError('patch name absent or ambiguous: ' + entry['name'])
            index, patch = matches[0]
            payload.extend(patch)
            report.append(dict(entry, source_program_1based=index+1, algorithm_1based=(patch[110] & 31)+1,
                               sha256=hashlib.sha256(patch).hexdigest()))
    return bytes(payload), {'status': 'offline audition payload; renderer and device bank not integrated',
                            'packed_bytes': len(payload), 'voices': report}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path)
    p.add_argument('--plan', type=Path, default=Path(__file__).resolve().parents[1]/'config/dx7/review_bank.json')
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    data, report = prepare(a.archive, json.loads(a.plan.read_text()))
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out/'review-voices.bin').write_bytes(data)
    (a.out/'review-voices.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'{len(report["voices"])} selected DX7 voices, {len(data)} packed bytes; offline only')


if __name__ == '__main__':
    main()

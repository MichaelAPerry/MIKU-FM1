#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check linked native usage plus explicit development headroom, never package size."""
import argparse
import json
from pathlib import Path
import re


def measure(image_size, symbols, budget):
    def symbol(name):
        matches = re.findall(r'^([0-9a-fA-F]+) .*\s' + re.escape(name) + r'$', symbols, re.M)
        if len(matches) != 1:
            raise ValueError(f'expected one linker symbol {name}, got {len(matches)}')
        return int(matches[0], 16)
    if image_size <= 0:
        raise ValueError('native application image is empty')
    used = {'image': image_size, 'static_ram': symbol('_bss_end') - 0x01C08000,
            'pool': symbol('_pool_end') - 0x01C20000}
    result = {}
    for name, size in used.items():
        limit, minimum = budget[name]['limit'], budget[name]['minimum_free']
        if size < 0 or limit <= 0 or minimum < 0 or minimum > limit:
            raise ValueError(f'invalid usage/budget for {name}')
        free = limit - size
        result[name] = {'used': size, 'limit': limit, 'free': free,
                        'minimum_free': minimum, 'margin_above_reserve': free - minimum,
                        'pass': free >= minimum}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--symbols', type=Path, required=True)
    parser.add_argument('--budget', type=Path, default=Path(__file__).resolve().parents[1]/'config/resource_budget.json')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        report = measure(args.image.stat().st_size, args.symbols.read_text(), json.loads(args.budget.read_text()))
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, f'resource budget: {error}\n')
    if args.output:
        args.output.write_text(json.dumps(report, indent=2)+'\n')
    for name, entry in report.items():
        print(f"resource budget: {name}: used {entry['used']}, free {entry['free']}, reserve {entry['minimum_free']}: {'PASS' if entry['pass'] else 'FAIL'}")
    return 0 if all(entry['pass'] for entry in report.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())

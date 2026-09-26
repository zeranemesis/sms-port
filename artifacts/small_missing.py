"""Batch of small missing LIVE .text functions, grouped by unit.

Writing a missing function yields a full match for that function, whereas
chasing an 8-byte frame difference in an already-99% function yields nothing.
This ranks the cheap wins: short functions we simply never wrote.

Usage: python artifacts/small_missing.py [maxsize_hex] [limit]
"""
import json
import os
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from map_parse import load_map  # noqa: E402

SKIP = ('mario/JSystem', 'mario/dolphin', 'mario/PowerPC', 'mario/THPPlayer',
        'mario/Reaction')


def main():
    maxsize = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x140
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 12
    entries, _ = load_map()
    cfg = json.load(open('objdiff.json', encoding='utf-8', errors='replace'))

    by_unit = defaultdict(list)
    for u in cfg.get('units', []):
        name, base = u.get('name', ''), u.get('base_path', '')
        if not base or name.startswith(SKIP):
            continue
        if not os.path.exists(base):
            continue
        res = subprocess.run(['build/binutils/powerpc-eabi-nm.exe', '-S', base],
                             capture_output=True, text=True, errors='replace',
                             timeout=120)
        have = {ln.split()[-1] for ln in res.stdout.splitlines()
                if len(ln.split()) >= 3}
        fname = os.path.splitext(os.path.basename(base))[0] + '.cpp'
        for sym, e in entries.items():
            if e['file'] != fname or e.get('section') != '.text':
                continue
            if e['unused'] or sym in have or e['size'] > maxsize:
                continue
            if sym.startswith('.') or 'JSystem' in e['lib'] or 'MSL_' in e['lib']:
                continue
            by_unit[name].append((e['size'], sym))

    total = sum(len(v) for v in by_unit.values())
    bytes_total = sum(s for v in by_unit.values() for s, _ in v)
    print('missing LIVE .text <= 0x%x: %d functions, %d bytes (%.1f%% of .text)'
          % (maxsize, total, bytes_total, 100.0 * bytes_total / 4224332))
    order = sorted(by_unit.items(), key=lambda kv: -sum(s for s, _ in kv[1]))
    for unit, fns in order[:limit]:
        fns.sort()
        print('\n%s   (%d fn, %d B)' % (unit, len(fns), sum(s for s, _ in fns)))
        for s, sym in fns[:14]:
            print('    0x%-5x %s' % (s, sym[:64]))


if __name__ == '__main__':
    main()

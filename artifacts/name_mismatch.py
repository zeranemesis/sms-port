"""Detect missing map symbols that we actually *do* define under a different
class name.

The mangled name is  <method>__<class><signature>.  If our object contains
<method>__<OTHERCLASS><signature> for a symbol the map says belongs to
<CLASS>, then nothing is missing: our reconstruction simply named the class
differently.  Renaming the class fixes a whole batch of "missing" symbols at
once, which is far cheaper than decompiling them.

Usage: python artifacts/name_mismatch.py [limit]
"""
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from map_parse import load_map  # noqa: E402

SKIP = ('mario/JSystem', 'mario/dolphin', 'mario/PowerPC', 'mario/THPPlayer',
        'mario/Reaction')

SPLIT = re.compile(r'^([A-Za-z0-9_~$]+)__')


def main():
    limit = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    entries, _ = load_map()
    cfg = json.load(open('objdiff.json', encoding='utf-8', errors='replace'))

    hits = []
    for u in cfg.get('units', []):
        name, base = u.get('name', ''), u.get('base_path', '')
        if not base or name.startswith(SKIP) or not os.path.exists(base):
            continue
        res = subprocess.run(['build/binutils/powerpc-eabi-nm.exe', '-S', base],
                             capture_output=True, text=True, errors='replace',
                             timeout=120)
        ours = [ln.split()[-1] for ln in res.stdout.splitlines()
                if len(ln.split()) >= 3]
        have = set(ours)
        # index our symbols by "<method><signature>" with class erased
        idx = defaultdict(list)
        for s in ours:
            m = SPLIT.match(s)
            if m:
                idx[m.group(1) + s[m.end():]].append(s)
        fname = os.path.splitext(os.path.basename(base))[0] + '.cpp'
        for sym, e in entries.items():
            if e['file'] != fname or e.get('section') != '.text':
                continue
            if e['unused'] or sym in have or sym.startswith('.'):
                continue
            if 'JSystem' in e['lib'] or 'MSL_' in e['lib']:
                continue
            m = SPLIT.match(sym)
            if not m:
                continue
            key = m.group(1) + sym[m.end():]
            for alt in idx.get(key, []):
                hits.append((name, sym, alt))

    print('missing symbols defined by us under another class name: %d' % len(hits))
    by_unit = defaultdict(list)
    for n, want, got in hits:
        by_unit[n].append((want, got))
    for unit, lst in sorted(by_unit.items(), key=lambda kv: -len(kv[1]))[:limit]:
        print('\n%s  (%d)' % (unit, len(lst)))
        for want, got in lst[:10]:
            print('    map:  %s' % want[:70])
            print('    ours: %s' % got[:70])


if __name__ == '__main__':
    main()

"""Find units that are nearly finished: few missing LIVE .text functions,
high current match percentage.

Finishing one such unit takes it to 100%; starting a virgin one does not.

Usage: python artifacts/finishable.py [top N]
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
    top = int(sys.argv[1]) if len(sys.argv) > 1 else 40
    entries, _ = load_map()
    cfg = json.load(open('objdiff.json', encoding='utf-8', errors='replace'))
    rep = json.load(open('build/GMSP01/report.json',
                         encoding='utf-8', errors='replace'))
    pct = {}
    for u in rep.get('units', []):
        m = u.get('measures', {})
        pct[u.get('name')] = m.get('matched_code_percent')

    rows = []
    for u in cfg.get('units', []):
        name, base = u.get('name', ''), u.get('base_path', '')
        if not base or name.startswith(SKIP) or not os.path.exists(base):
            continue
        res = subprocess.run(['build/binutils/powerpc-eabi-nm.exe', '-S', base],
                             capture_output=True, text=True, errors='replace',
                             timeout=120)
        have = {ln.split()[-1] for ln in res.stdout.splitlines()
                if len(ln.split()) >= 3}
        fname = os.path.splitext(os.path.basename(base))[0] + '.cpp'
        miss = []
        for sym, e in entries.items():
            if e['file'] != fname or e.get('section') != '.text':
                continue
            if e['unused'] or sym in have or sym.startswith('.'):
                continue
            if 'JSystem' in e['lib'] or 'MSL_' in e['lib']:
                continue
            miss.append((e['size'], sym))
        if not miss:
            continue
        p = pct.get(name)
        rows.append((len(miss), sum(s for s, _ in miss),
                     p if isinstance(p, (int, float)) else -1, name,
                     sorted(miss)))

    # least missing bytes overall; percent shown for context
    good = [r for r in rows if r[1] <= 0x800]
    good.sort(key=lambda r: (r[1], r[0]))
    print('units with <=0x800 missing LIVE bytes: %d' % len(good))
    print('%5s %7s %7s  %s' % ('FN', 'BYTES', 'CODE%', 'UNIT'))
    for n, b, p, name, fns in good[:top]:
        print('%5d %7d %6.2f%%  %s' % (n, b, p, name))
        for s, sym in fns[:8]:
            print('           0x%-5x %s' % (s, sym[:60]))


if __name__ == '__main__':
    main()

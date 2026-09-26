"""Project the matched_code gain each unit would get if every missing LIVE
function were written.

gain% = missing_bytes / total_code, so this is literally "how much of the unit
is still just absent", independent of how wrong the existing bodies are.

Usage: python artifacts/missing_gain.py [top N]
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
    top = int(sys.argv[1]) if len(sys.argv) > 1 else 35
    entries, _ = load_map()
    cfg = json.load(open('objdiff.json', encoding='utf-8', errors='replace'))
    rep = json.load(open('build/GMSP01/report.json',
                         encoding='utf-8', errors='replace'))
    info = {}
    for u in rep.get('units', []):
        m = u.get('measures', {})
        try:
            info[u.get('name')] = (int(m.get('total_code', 0)),
                                   int(m.get('matched_code', 0)),
                                   float(m.get('matched_code_percent', 0)))
        except (TypeError, ValueError):
            pass

    rows = []
    for u in cfg.get('units', []):
        name, base = u.get('name', ''), u.get('base_path', '')
        if not base or name.startswith(SKIP) or not os.path.exists(base):
            continue
        if name not in info:
            continue
        total, matched, pctv = info[name]
        if total <= 0:
            continue
        res = subprocess.run(['build/binutils/powerpc-eabi-nm.exe', '-S', base],
                             capture_output=True, text=True, errors='replace',
                             timeout=120)
        have = {ln.split()[-1] for ln in res.stdout.splitlines()
                if len(ln.split()) >= 3}
        fname = os.path.splitext(os.path.basename(base))[0] + '.cpp'
        miss, n = 0, 0
        for sym, e in entries.items():
            if (e['file'] == fname and e.get('section') == '.text'
                    and not e['unused'] and sym not in have
                    and not sym.startswith('.')
                    and 'JSystem' not in e['lib'] and 'MSL_' not in e['lib']):
                miss += e['size']
                n += 1
        if miss <= 0:
            continue
        rows.append((100.0 * miss / total, miss, n, pctv, total, name))

    rows.sort(reverse=True)
    print('%7s %7s %4s %8s  %s'
          % ('GAIN%', 'MISS_B', 'FN', 'NOW%', 'UNIT'))
    for g, miss, n, pctv, total, name in rows[:top]:
        print('%6.2f%% %7d %4d %7.2f%%  %s' % (g, miss, n, pctv, name))


if __name__ == '__main__':
    main()

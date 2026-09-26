"""Per-section-type totals from build/GMSP01/report.json.

Usage: python artifacts/section_totals.py [report.json]
"""

import json
import os
import sys
from collections import defaultdict


def main():
    path = (sys.argv[1] if len(sys.argv) > 1
            else os.path.join('build', 'GMSP01', 'report.json'))
    rep = json.load(open(path, encoding='utf-8'))

    agg = defaultdict(lambda: [0, 0, 0])  # size, matched, full
    for u in rep['units']:
        for s in u.get('sections', []):
            name = s['name']
            sz = int(s.get('size', 0))
            fz = s.get('fuzzy_match_percent', 0.0)
            agg[name][0] += sz
            agg[name][1] += sz * fz / 100.0
            agg[name][2] += sz if fz >= 99.949 else 0

    order = sorted(agg.items(), key=lambda kv: -kv[1][0])
    print('%-10s %10s %10s %8s %10s %8s'
          % ('section', 'size', 'matched', '%', 'fuzzy', '%'))
    for name, (sz, fz, mt) in order:
        print('%-10s %10d %10d %7.2f%% %10d %7.2f%%'
              % (name, sz, mt, 100.0 * mt / sz if sz else 0,
                 int(fz + 0.5), 100.0 * fz / sz if sz else 0))

    sz = sum(v[0] for v in agg.values())
    mt = sum(v[2] for v in agg.values())
    fz = sum(v[1] for v in agg.values())
    print('%-10s %10d %10d %7.2f%% %10d %7.2f%%'
          % ('TOTAL', sz, mt, 100.0 * mt / sz, int(fz + 0.5),
             100.0 * fz / sz))


if __name__ == '__main__':
    main()

"""Global matching totals from build/GMSP01/report.json.

Usage: python artifacts/global_stats.py [report.json]

Prints, summed over every unit: total code size, matched code bytes (sections
that are 100% byte identical) and matched data bytes, plus fuzzy percentages.
"""

import json
import os
import sys


def main():
    path = (sys.argv[1] if len(sys.argv) > 1
            else os.path.join('build', 'GMSP01', 'report.json'))
    rep = json.load(open(path, encoding='utf-8'))

    code = mcode = data = mdata = 0
    fcode = fdata = 0.0
    units = 0
    for u in rep['units']:
        units += 1
        for s in u.get('sections', []):
            sz = int(s.get('size', 0))
            name = s['name']
            if name in ('.text', '.ctors'):
                code += sz
                fcode += sz * s.get('fuzzy_match_percent', 0.0) / 100.0
            else:
                data += sz
                fdata += sz * s.get('fuzzy_match_percent', 0.0) / 100.0
            if s.get('fuzzy_match_percent', 0.0) >= 99.949:
                if name in ('.text', '.ctors'):
                    mcode += sz
                else:
                    mdata += sz

    print('units            %d' % units)
    print('code   %8d   fuzzy %6.2f%%   matched %8d  (%5.2f%%)'
          % (code, 100.0 * fcode / code, mcode, 100.0 * mcode / code))
    print('data   %8d   fuzzy %6.2f%%   matched %8d  (%5.2f%%)'
          % (data, 100.0 * fdata / data, mdata, 100.0 * mdata / data))
    print('total  %8d   fuzzy %6.2f%%   matched %8d  (%5.2f%%)'
          % (code + data,
             100.0 * (fcode + fdata) / (code + data),
             mcode + mdata, 100.0 * (mcode + mdata) / (code + data)))


if __name__ == '__main__':
    main()

"""Rank units by *unmatched bytes*, not by percentage.

Percentage is misleading: a 99% unit with a 5000-byte function hides more work
than a 0% unit with three tiny ones.  This reads objdiff's report and prints
where the remaining unmatched bytes actually live.

Usage: python artifacts/rank_units.py [top N]
"""
import json
import sys

REPORT = 'build/GMSP01/report.json'
SKIP = ('mario/JSystem', 'mario/dolphin', 'mario/PowerPC', 'mario/THPPlayer')


def main():
    top = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    d = json.load(open(REPORT, encoding='utf-8', errors='replace'))
    rows = []
    grand_code = 0
    grand_miss = 0
    for u in d.get('units', []):
        if not isinstance(u, dict):
            continue
        name = u.get('name', '')
        if name.startswith(SKIP):
            continue
        m = u.get('measures', {})
        try:
            total_code = int(m.get('total_code', 0))
            matched_code = int(m.get('matched_code', 0))
            total_data = int(m.get('total_data', 0))
            matched_data = int(m.get('matched_data', 0))
        except (TypeError, ValueError):
            continue
        miss = (total_code - matched_code) + (total_data - matched_data)
        grand_code += total_code + total_data
        grand_miss += miss
        if miss <= 0:
            continue
        fns = []
        for f in u.get('functions') or []:
            size = f.get('size') or 0
            if not isinstance(size, int) or f.get('status') == 'match':
                continue
            pct = f.get('fuzzy_match_percent', f.get('fuzzy_match', 0)) or 0
            if isinstance(pct, float) and pct <= 1.0:
                pct *= 100
            fns.append((size * (100.0 - pct) / 100.0, pct, size,
                        f.get('name', '')))
        fns.sort(reverse=True)
        rows.append((miss, total_code + total_data, name, fns))
    rows.sort(reverse=True)
    print('game-code unmatched bytes: %d / %d  (%.2f%% to go)\n'
          % (grand_miss, grand_code, 100.0 * grand_miss / max(grand_code, 1)))
    for miss, tot, name, fns in rows[:top]:
        print('%7dB miss  %-44s %.1f%% of unit left'
              % (miss, name, 100.0 * miss / max(tot, 1)))
        for m2, p, sz, nm in fns[:2]:
            print('               %5.1f%% of %5dB  %s' % (p, sz, nm[:60]))


if __name__ == '__main__':
    main()

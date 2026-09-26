import json, sys, io, os

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

d = json.load(open(os.path.join('build', 'GMSP01', 'report_changes.json'),
                   encoding='utf-8'))

METRICS = [
    ('fuzzy_match_percent', 'fuzzy_match'),
    ('matched_code_percent', 'matched_code'),
    ('matched_data_percent', 'matched_data'),
    ('matched_functions_percent', 'matched_fns'),
]


def delta(unit, metric, section=None):
    src = unit
    if section is not None:
        for s in unit.get('sections', []):
            if s['name'] == section:
                src = s
                break
        else:
            return None
    b = src.get('from', {}).get(metric)
    a = src.get('to', {}).get(metric)
    if b is None or a is None:
        return None
    return float(b), float(a)


print('=== UNITS CHANGED (%d) ===' % len(d['units']))
rows = []
for u in d['units']:
    for metric, label in METRICS:
        r = delta(u, metric)
        if r and abs(r[1] - r[0]) > 1e-9:
            rows.append((r[1] - r[0], u['name'], label, r[0], r[1]))
    for s in u.get('sections', []):
        for metric, label in METRICS:
            r = delta(u, metric, s['name'])
            if r and abs(r[1] - r[0]) > 1e-9:
                rows.append((r[1] - r[0], u['name'] + ' ' + s['name'],
                             label, r[0], r[1]))

rows.sort()
print()
print('=== REGRESSIONS (%d) ===' % sum(1 for r in rows if r[0] < 0))
for dd, name, label, b, a in rows:
    if dd < 0:
        print('  %-46s %-13s %8.2f -> %8.2f  (%+.2f)'
              % (name[:46], label, b, a, dd))

print()
print('=== IMPROVEMENTS (%d) ===' % sum(1 for r in rows if r[0] > 0))
for dd, name, label, b, a in sorted(rows, key=lambda r: -r[0]):
    if dd > 0:
        print('  %-46s %-13s %8.2f -> %8.2f  (%+.2f)'
              % (name[:46], label, b, a, dd))

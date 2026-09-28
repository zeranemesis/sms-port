import json
r = json.load(open('build/GMSP01/report.json'))
rows = []
for u in r['units']:
    if u.get('complete'):
        continue
    fns = u.get('functions') or []
    if not fns:
        continue
    bad = [f for f in fns if f.get('fuzzy_match_percent') != 100.0]
    if not bad:
        continue
    m = u.get('measures', {})
    rows.append((len(bad), -m.get('matched_code_percent', 0), u['name'], m.get('matched_code_percent', 0)))
rows.sort()
print(f"non-complete units with fns: {len(rows)}")
print("sorted by (fewest bad, then highest %):")
for n, negpct, name, pct in rows[:45]:
    print(f"  {n:3d} bad  {pct:6.2f}%  {name}")

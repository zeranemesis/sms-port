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
    if len(bad) != 1:
        continue
    m = u.get('measures', {})
    rows.append((m.get('matched_code_percent', 0), u['name'], bad[0]))
rows.sort(reverse=True)
print(f"units with exactly 1 bad fn: {len(rows)}")
for pct, name, f in rows:
    print(f"  {pct:6.2f}%  {name.split('/',1)[1]:30s} {f.get('fuzzy_match_percent',0):6.2f}%  {f.get('name')}")

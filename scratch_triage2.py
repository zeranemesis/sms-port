import json
r = json.load(open('build/GMSP01/report.json'))
rows = []
for u in r['units']:
    mc = u['measures'].get('matched_code_percent', 0)
    if mc < 60.0:  # only partially-done units
        continue
    fns = [f for f in u.get('functions', []) if f.get('fuzzy_match_percent') is not None and f['fuzzy_match_percent'] < 100.0]
    if not fns:
        continue
    total_bytes = int(u['measures'].get('total_code', 0))
    matched = int(u['measures'].get('matched_code', 0))
    rows.append((len(fns), total_bytes - matched, u['name'], mc,
                 [(f['name'], round(f['fuzzy_match_percent'], 2)) for f in fns]))
rows.sort()
print(f"partial units with remaining fns: {len(rows)}")
for n, ub, name, mc, fns in rows[:45]:
    print(f"\n{n:>2} fns {ub:>6}B  {name}  ({mc:.1f}%)")
    for fn, fp in fns[:6]:
        print(f"     {fp:>7}%  {fn}")
print("\nTOTAL units <=2 fns:", sum(1 for x in rows if x[0] <= 2),
      "bytes:", sum(x[1] for x in rows if x[0] <= 2))

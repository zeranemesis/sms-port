import json
r = json.load(open('build/GMSP01/report.json'))
rows = []
for u in r['units']:
    mc = u['measures'].get('matched_code_percent', 0)
    if mc >= 100.0:
        continue
    fns = [f for f in u.get('functions', []) if f.get('fuzzy_match_percent') is not None and f['fuzzy_match_percent'] < 100.0]
    total_bytes = int(u['measures'].get('total_code', 0))
    matched = int(u['measures'].get('matched_code', 0))
    rows.append((len(fns), sum(int(f['size']) for f in fns), total_bytes - matched, u['name'], mc))
rows.sort()
print(f"incomplete units: {len(rows)}")
print(f"{'nfns':>5} {'fnB':>6} {'unmB':>6}  unit")
for n, fb, ub, name, mc in rows[:40]:
    print(f"{n:>5} {fb:>6} {ub:>6}  {name}  ({mc:.1f}%)")
print("---")
print("units with <=2 nonmatch fns:", sum(1 for x in rows if x[0] <= 2))
print("bytes to match in those:", sum(x[2] for x in rows if x[0] <= 2))

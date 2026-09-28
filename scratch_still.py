import json
r = json.load(open('build/GMSP01/report.json'))
units = r.get('units', [])
byunit = {u['name']: u for u in units}
names = json.load(open('scratch_names.json'))
neg = [(u, f, n, nm) for u, f, n, nm in names if nm and not str(nm).startswith('ERR') and (n is not None)]
# recompute: only negative-delta (target smaller)
pads = json.load(open('scratch_pads.json'))
negset = set()
for d, lst in pads.items():
    d = int(d)
    if d >= 0:
        continue
    for unit, fn in lst:
        negset.add((unit, fn))
still = []
for u, f, n, nm in names:
    if (u, f) not in negset:
        continue
    uu = byunit.get(u)
    ff = next((x for x in uu.get('functions', []) if x['name'] == f), None) if uu else None
    if ff and ff.get('fuzzy_match_percent') != 100.0:
        still.append((u, f, ff.get('fuzzy_match_percent'), nm))
print(f"negative-delta fns still not 100%: {len(still)}")
still.sort(key=lambda x: x[2])
for u, f, p, nm in still:
    print(f"  {p:6.2f}%  {u.split('/',1)[1]:28s} {nm[:60]}")

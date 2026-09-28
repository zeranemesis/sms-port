import json
r = json.load(open('build/GMSP01/report.json'))
units = r.get('units', [])
def g(u, k):
    v = u['measures'].get(k)
    return int(v) if v is not None else 0
tc = sum(g(u, 'total_code') for u in units)
mc = sum(g(u, 'matched_code') for u in units)
fu = sum(g(u, 'total_functions') for u in units)
mf = sum(g(u, 'matched_functions') for u in units)
completes = sum(1 for u in units if u['measures'].get('matched_code_percent') == 100.0)
print(f"units: {len(units)}, complete: {completes}")
print(f"code: {mc}/{tc} = {100*mc/tc:.4f}%")
print(f"functions: {mf}/{fu} = {100*mf/fu:.2f}%")
worst = sorted(units, key=lambda u: (g(u, 'total_code') - g(u, 'matched_code')))
for u in worst[:12]:
    miss = g(u, 'total_code') - g(u, 'matched_code')
    if miss <= 0:
        break
    print(f"  {miss:5d}B missing  {u['name']}")

import json
r = json.load(open('build/GMSP01/report.json'))
units = r.get('units', [])
def g(u, k):
    v = u['measures'].get(k)
    return int(v) if v is not None else 0
worst = sorted(units, key=lambda u: (g(u, 'total_code') - g(u, 'matched_code')), reverse=True)
for u in worst[:12]:
    m = u['measures']
    print(f"  {g(u,'total_code')-g(u,'matched_code'):6d}B missing  {u['name']}  keys={list(m.keys())[:6]}")

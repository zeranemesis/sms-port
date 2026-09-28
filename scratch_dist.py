import json
r = json.load(open('build/GMSP01/report.json'))
units = r.get('units', [])
buckets = {0:0, 1:0, 50:0, 90:0, 99:0, 100:0}
tot = 0
nearfns = []
for u in units:
    for f in u.get('functions', []):
        p = f.get('fuzzy_match_percent')
        if p is None:
            continue
        tot += 1
        if p == 100.0:
            buckets[100] += 1
        elif p >= 99.0:
            buckets[99] += 1
            nearfns.append((p, u['name'], f['name'], f.get('size')))
        elif p >= 90.0:
            buckets[90] += 1
            nearfns.append((p, u['name'], f['name'], f.get('size')))
        elif p >= 50.0:
            buckets[50] += 1
        elif p > 0.0:
            buckets[1] += 1
        else:
            buckets[0] += 1
print(f"total fns: {tot}")
print(f"  100%: {buckets[100]}")
print(f"  99-99.9%: {buckets[99]}")
print(f"  90-98.9%: {buckets[90]}")
print(f"  50-89.9%: {buckets[50]}")
print(f"  1-49.9%: {buckets[1]}")
print(f"  0%: {buckets[0]}")
print(f"\nnear-complete (>=90) fns: {len(nearfns)}")
nearfns.sort(key=lambda x: -x[3] if x[3] else 0)
for p, u, f, s in nearfns[:25]:
    print(f"  {p:6.2f}%  {s:>5}B  {u.split('/',1)[1]}  {f[:45]}")

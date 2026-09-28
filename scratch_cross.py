import json, re
r99 = json.load(open('scratch_99.json'))
pads = json.load(open('scratch_pads.json'))
padded = set()
for d, lst in pads.items():
    for unit, fn in lst:
        padded.add((unit, fn))
in99 = [x for x in r99 if x[3] in ('frame+other', 'other')]
overlap = [x for x in in99 if (x[0], x[1]) in padded]
print(f"99% fns with real diffs: {len(in99)}, already in pad list: {len(overlap)}")
# for overlap: what's the current state?
names = json.load(open('scratch_names.json'))
bymangled = {(u, f): n for u, f, n, nm in names}
# check which overlap fns now 100%
rep = json.load(open('build/GMSP01/report.json'))
byunit = {u['name']: u for u in rep.get('units', [])}
still = 0
for unit, fn, p, cat, nb in overlap:
    u = byunit.get(unit)
    f = next((f for f in u.get('functions', []) if f['name'] == fn), None) if u else None
    if f and f.get('fuzzy_match_percent') != 100.0:
        still += 1
print(f"overlap still not 100%: {still}")

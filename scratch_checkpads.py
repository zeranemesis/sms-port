import json
r = json.load(open('build/GMSP01/report.json'))
byunit = {u['name']: u for u in r.get('units', [])}
applied = [(u, f) for u, f, n, nm in json.load(open('scratch_applied3.json'))]
applied += [(u, f) for u, f, n, cls in json.load(open('scratch_applied4.json'))]
seen = set()
applied = [a for a in applied if not (a in seen or seen.add(a))]
print(f"checked {len(applied)} newly-padded fns:")
ok = 0
for u, f in applied:
    uu = byunit.get(u)
    ff = next((x for x in uu.get('functions', []) if x['name'] == f), None) if uu else None
    p = ff.get('fuzzy_match_percent') if ff else None
    if p == 100.0:
        ok += 1
    else:
        print(f"  MISS {p:7.2f}%  {u.split('/',1)[1]:28s} {f[:55]}")
print(f"at 100%: {ok}/{len(applied)}")
print(f"complete units: {r['measures']['complete_units']}")
# units where ALL fns 100 and code 100 but not flagged complete
nearly = [u for u in r['units'] if not u.get('complete')
          and u.get('functions')
          and u.get('matched_code_percent', 0) >= 99.9
          and u.get('matched_data_percent', 0) >= 99.9]
print(f"near-complete (>=99.9% code+data) not flagged: {len(nearly)}")
for u in nearly:
    print("  ", u['name'], round(u.get('matched_code_percent',0),2), round(u.get('matched_data_percent',0),2))

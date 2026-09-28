import json, re
r = json.load(open('build/GMSP01/report.json'))
cfg = open('configure.py', encoding='utf-8', errors='replace').read()
# map unit name -> matching flag in configure.py
# configure lines: Object(Matching|NonMatching, "path.cpp")  and PCHObject(...)
def flag_for(unit):
    # unit like "marioEU/Enemy/gatekeeper" -> path "src/Enemy/gatekeeper.cpp"
    tail = unit.split('/', 1)[1] + '.cpp'
    # search for the last occurrence of the filename in a Matching/NonMatching line
    pat = re.compile(r'(Object|PCHObject)\(\s*(Matching|NonMatching)\s*,\s*["\'][^"\']*' + re.escape(tail) + r'["\']')
    hits = pat.findall(cfg)
    return hits[-1] if hits else None
cand = []
for u in r['units']:
    if u.get('complete'):
        continue
    fns = u.get('functions') or []
    if not fns:
        continue
    if all(f.get('fuzzy_match_percent') == 100.0 for f in fns) \
       and u.get('matched_code_percent', 0) >= 99.99 \
       and u.get('matched_data_percent', 0) >= 99.99:
        cand.append(u)
print(f"units all-fns-100 & code/data>=99.99 but not flagged complete: {len(cand)}")
for u in sorted(cand, key=lambda x: x['name']):
    fl = flag_for(u['name'])
    print(f"  {u['name']:40s} code={u.get('matched_code_percent',0):.2f} data={u.get('matched_data_percent',0):.2f} cfg={fl}")

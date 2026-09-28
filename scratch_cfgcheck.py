import json, re
fixable = json.load(open('scratch_fixable.json'))
cfg = open('configure.py', encoding='utf-8', errors='replace').read()
ok = 0
notok = []
for unit, fn, p, d, bad in fixable:
    m = re.search(r'Object\((Matching|NonMatching),\s*"[^"]*' + re.escape(unit.split('/', 1)[1]) + r'\.cpp"', cfg)
    if m and m.group(1) == 'Matching':
        ok += 1
    else:
        notok.append((unit, fn, d, m.group(1) if m else None))
print(f"fixable in Matching units: {ok}/{len(fixable)}")
for n in notok[:10]:
    print("  ", n)

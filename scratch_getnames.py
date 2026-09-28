import json, re, subprocess, os
from concurrent.futures import ThreadPoolExecutor

pads = json.load(open('scratch_pads.json'))
todo = []
for d, lst in pads.items():
    d = int(d)
    if d >= 0:
        continue
    for unit, fn in lst:
        todo.append((unit, fn, -d))
print(f"functions to resolve: {len(todo)}")

def one(unit, fn, n):
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=120).stdout
        first = out.splitlines()[0] if out.strip() else ''
        m = re.match(r'^(.+?):\s+\d', first)
        name = m.group(1) if m else None
        return (unit, fn, n, name)
    except Exception as e:
        return (unit, fn, n, f'ERR:{e}')

res = []
with ThreadPoolExecutor(max_workers=10) as ex:
    for i, r in enumerate(ex.map(one, [t[0] for t in todo], [t[1] for t in todo], [t[2] for t in todo])):
        res.append(r)
        if (i + 1) % 50 == 0:
            print(f"  {i+1}/{len(todo)}")

json.dump(res, open('scratch_names.json', 'w'), indent=1)
ok = [r for r in res if r[3] and not str(r[3]).startswith('ERR')]
print(f"resolved: {len(ok)} / {len(res)}")
for r in res:
    if not r[3] or str(r[3]).startswith('ERR'):
        print("  FAIL:", r[0].split('/', 1)[1], r[1], r[3])

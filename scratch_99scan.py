import json, re, subprocess
from concurrent.futures import ThreadPoolExecutor

r = json.load(open('build/GMSP01/report.json'))
units = r.get('units', [])
todo = []
for u in units:
    for f in u.get('functions', []):
        p = f.get('fuzzy_match_percent')
        if p is not None and 99.0 <= p < 100.0:
            todo.append((u['name'], f['name'], p))
print(f"99%+ fns: {len(todo)}")

LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
ADDI = re.compile(r'addi\s+r1,\s*r1,\s*\{?(-?0x[0-9a-f]+)\}?$')

def hv(s):
    s = s.strip()
    if s.lstrip('-').lower().startswith('0x'):
        return int(s, 16)
    return int(s)

def one(unit, fn, p):
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=120).stdout
    except Exception:
        return (unit, fn, p, 'ERR', [])
    rows = []
    for ln in out.splitlines():
        m = LINE.match(ln)
        if m:
            rows.append((m.group(1), m.group(2).strip(), m.group(3).strip()))
    if not rows:
        return (unit, fn, p, 'NODIFF', [])
    # frame delta
    fl = fr = None
    for off, l, rt in rows:
        if fl is None:
            mm = STWU.search(l)
            if mm: fl = hv(mm.group(1))
        if fr is None:
            mm = STWU.search(rt)
            if mm: fr = hv(mm.group(1))
    # classify diffs
    kinds = []
    for off, l, rt in rows:
        if l == rt:
            continue
        if STWU.search(l) or STWU.search(rt) or ADDI.search(l) or ADDI.search(rt):
            kinds.append('frame')
        elif re.match(r'b\w*\s+0x[0-9a-f]+$', l) and re.match(r'b\w*\s+0x[0-9a-f]+$', rt):
            kinds.append('branch')
        elif re.search(r'\(r1\)', l) and re.search(r'\(r1\)', rt):
            kinds.append('r1off')
        else:
            kinds.append('other')
    cat = 'frame-only' if set(kinds) <= {'frame', 'branch'} else '+'.join(sorted(set(kinds)))
    nbad = len([k for k in kinds if k not in ('frame', 'branch')])
    return (unit, fn, p, cat, nbad)

res = []
with ThreadPoolExecutor(max_workers=10) as ex:
    for i, r_ in enumerate(ex.map(one, [t[0] for t in todo], [t[1] for t in todo], [t[2] for t in todo])):
        res.append(r_)
        if (i + 1) % 100 == 0:
            print(f"  {i+1}/{len(todo)}")

from collections import Counter
c = Counter(r_[3] for r_ in res)
print(c.most_common())
json.dump(res, open('scratch_99.json', 'w'), indent=1)

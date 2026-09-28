import json, re, subprocess
from concurrent.futures import ThreadPoolExecutor

pads = json.load(open('scratch_pads.json'))
negset = set()
for d, lst in pads.items():
    d = int(d)
    if d >= 0:
        continue
    for unit, fn in lst:
        negset.add((unit, fn))
r = json.load(open('build/GMSP01/report.json'))
byunit = {u['name']: u for u in r.get('units', [])}
todo = []
for u, f, n, nm in json.load(open('scratch_names.json')):
    if (u, f) not in negset:
        continue
    uu = byunit.get(u)
    ff = next((x for x in uu.get('functions', []) if x['name'] == f), None) if uu else None
    if ff and ff.get('fuzzy_match_percent') != 100.0:
        todo.append((u, f))
print(f"to classify: {len(todo)}")

LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
ADDI_R1 = re.compile(r'addi\s+r1,\s*r1,\s*\{?(-?0x[0-9a-f]+)\}?$')
def hv(s):
    s = s.strip()
    if s.lstrip('-').lower().startswith('0x'):
        return int(s, 16)
    return int(s)

def one(unit, fn):
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=120).stdout
    except Exception:
        return (unit, fn, 'ERR', 0, 0, 0)
    fl = fr = None
    rows = []
    for ln in out.splitlines():
        m = LINE.match(ln)
        if not m:
            continue
        mark, off, l, rt = m.groups()
        if fl is None:
            mm = STWU.search(l)
            if mm: fl = hv(mm.group(1))
        if fr is None:
            mm = STWU.search(rt)
            if mm: fr = hv(mm.group(1))
        if mark:
            rows.append((off, l.strip(), rt.strip()))
    if fl is None or fr is None:
        return (unit, fn, 'NOFRAME', 0, len(rows), 0)
    d = fl - fr
    def norm(insn, frame):
        insn = re.sub(r'\{(-?0x[0-9a-f]+)\}\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
        insn = re.sub(r'\((-?0x[0-9a-f]+)\)\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
        insn = ADDI_R1.sub(lambda mm: f"addi r1,r1,{hv(mm.group(1)) + frame}", insn)
        return insn
    bad = 0
    for off, l, rt in rows:
        if re.match(r'b\w*\s+0x[0-9a-f]+$', l) or re.match(r'b\w*\s+0x[0-9a-f]+$', rt):
            continue
        if STWU.search(l) or STWU.search(rt) or ADDI_R1.search(l) or ADDI_R1.search(rt):
            continue
        if norm(l, fl) != norm(rt, fr):
            bad += 1
    return (unit, fn, 'OK', d, len(rows), bad)

res = []
with ThreadPoolExecutor(max_workers=10) as ex:
    for i, r_ in enumerate(ex.map(one, [t[0] for t in todo], [t[1] for t in todo])):
        res.append(r_)
        if (i + 1) % 20 == 0:
            print(f"  {i+1}/{len(todo)}")
json.dump(res, open('scratch_still_detail.json', 'w'), indent=1)
fix = [x for x in res if x[2] == 'OK' and x[3] < 0 and x[5] == 0]
print(f"pure-padding: {len(fix)}")
for x in fix:
    print("  ", x[3], x[0].split('/', 1)[1], x[1][:40])

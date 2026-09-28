import json, re, subprocess
from concurrent.futures import ThreadPoolExecutor

r99 = json.load(open('scratch_99.json'))
todo = [(u, f, p) for u, f, p, cat, nb in r99 if cat in ('frame+other', 'other')]
print(f"to re-scan: {len(todo)}")

LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
ADDI_R1 = re.compile(r'addi\s+r1,\s*r1,\s*\{?(-?0x[0-9a-f]+)\}?$')
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
        return (unit, fn, p, None, 0)
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
        return (unit, fn, p, None, 0)
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
    return (unit, fn, p, d, bad)

res = []
with ThreadPoolExecutor(max_workers=10) as ex:
    for i, r_ in enumerate(ex.map(one, [t[0] for t in todo], [t[1] for t in todo], [t[2] for t in todo])):
        res.append(r_)
        if (i + 1) % 100 == 0:
            print(f"  {i+1}/{len(todo)}")

fixable = [x for x in res if x[3] is not None and x[3] < 0 and x[4] == 0]
print(f"pure-padding fixable (d<0, bad=0): {len(fixable)}")
json.dump(fixable, open('scratch_fixable.json', 'w'), indent=1)
for x in fixable[:10]:
    print("  ", x[3], x[0].split('/', 1)[1], x[1][:40])

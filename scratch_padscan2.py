import json, re, subprocess
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor

r = json.load(open('build/GMSP01/report.json'))

cands = []
for u in r['units']:
    if u['measures'].get('matched_code_percent', 0) >= 100.0:
        continue
    for f in u.get('functions', []):
        fp = f.get('fuzzy_match_percent')
        if fp is not None and 99.5 <= fp < 100.0:
            cands.append((u['name'], f['name']))

print(f"candidates: {len(cands)}", flush=True)

LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
R1OFF = re.compile(r'\{?(-?0x[0-9a-f]+|\d+)\}?\(r1\)')
ADDI_R1 = re.compile(r'addi\s+r1,\s*r1,\s*\{?(-?0x[0-9a-f]+|\d+)\}?$')

def hv(s):
    s = s.strip()
    neg = s.startswith('-')
    if s.lstrip('-').lower().startswith('0x'):
        v = int(s, 16)
    else:
        v = int(s)
    return v

def one(arg):
    unit, fn = arg
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=120).stdout
    except Exception:
        return None
    fl = fr = None
    rows = []
    for ln in out.splitlines():
        m = LINE.match(ln)
        if not m:
            continue
        mark, off, l, rt = m.groups()
        if fl is None:
            mm = STWU.search(l)
            if mm:
                fl = hv(mm.group(1))
        if fr is None:
            mm = STWU.search(rt)
            if mm:
                fr = hv(mm.group(1))
        rows.append((off, l.strip(), rt.strip(), mark))
    if fl is None or fr is None or fl == fr:
        return None
    d = fl - fr
    def norm(insn, frame):
        insn = re.sub(r'\{(-?0x[0-9a-f]+)\}\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
        insn = re.sub(r'\((-?0x[0-9a-f]+)\)\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
        insn = ADDI_R1.sub(lambda mm: f"addi r1,r1,{hv(mm.group(1)) + frame}", insn)
        return insn
    bad = 0
    for off, l, rt, mark in rows:
        if mark is None:
            continue  # identical lines: ignore
        if re.match(r'b\w*\s+0x[0-9a-f]+$', l) or re.match(r'b\w*\s+0x[0-9a-f]+$', rt):
            continue  # branch targets: same relative target iff bodies identical
        if STWU.search(l) or STWU.search(rt) or ADDI_R1.search(l) or ADDI_R1.search(rt):
            continue  # the frame allocation lines are the diff itself
        if norm(l, fl) != norm(rt, fr):
            bad += 1
    return (unit, fn, d, bad)

res = defaultdict(list)
nf = 0
with ThreadPoolExecutor(max_workers=16) as ex:
    for i, x in enumerate(ex.map(one, cands, chunksize=8)):
        if x is None:
            nf += 1
        elif x[3] == 0:
            res[x[2]].append((x[0], x[1]))
        if (i + 1) % 150 == 0:
            print(f"  {i+1}/{len(cands)}", flush=True)

print(f"frame-equal/none: {nf}")
total = 0
for d in sorted(res):
    print(f"  delta={d}: {len(res[d])}")
    total += len(res[d])
print(f"TOTAL pure-padding: {total}")
json.dump({str(k): v for k, v in res.items()}, open('scratch_pads.json', 'w'), indent=1)

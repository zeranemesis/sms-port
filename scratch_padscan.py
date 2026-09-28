import json, re, subprocess, os
from collections import defaultdict

OBJDUMP = r'build\binutils\powerpc-eabi-objdump.exe'
r = json.load(open('build/GMSP01/report.json'))

cands = set()
for u in r['units']:
    if u['measures'].get('matched_code_percent', 0) >= 100.0:
        continue
    for f in u.get('functions', []):
        fp = f.get('fuzzy_match_percent')
        if fp is not None and 99.5 <= fp < 100.0:
            cands.add((u['name'], f['name']))

print(f"candidates: {len(cands)}", flush=True)
units = sorted({k[0] for k in cands})

def objpath(unit, side):
    rel = unit.split('/', 1)[1]
    return os.path.join('build', 'GMSP01', side, rel + '.o')

HDR = re.compile(r'^\s*([0-9a-f]+) <(.+)>:$')
INS = re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s+(.+)$')

def parse_asm(path):
    out = subprocess.run([OBJDUMP, '-d', '--disassemble-all', path],
                         capture_output=True, text=True).stdout
    funcs = {}
    cur = None
    name = None
    for ln in out.splitlines():
        m = HDR.match(ln)
        if m:
            if cur is not None:
                funcs[name] = cur
            name = m.group(2)
            cur = []
            continue
        m = INS.match(ln)
        if m and cur is not None:
            insn = m.group(3).strip()
            insn = re.sub(r'(-?\d+)\(r1\)', lambda mm: f"({int(mm.group(1))})", insn)
            cur.append(insn)
    if cur is not None:
        funcs[name] = cur
    return funcs

def frame_of(insns):
    for i in insns:
        m = re.search(r'stwu\s+r1,\s*(-?\d+)\(r1\)', i)
        if m:
            return int(m.group(1))
    return None

def norm(insns, frame):
    return [re.sub(r'\((-?\d+)\)', lambda mm: f"({int(mm.group(1)) + frame})", i) for i in insns]

res = defaultdict(list)
nframe = nskip = 0
for unit in units:
    try:
        L = parse_asm(objpath(unit, 'src'))
        R = parse_asm(objpath(unit, 'obj'))
    except Exception as e:
        print("ERR", unit, e, flush=True)
        continue
    for fn in [f for (u, f) in cands if u == unit]:
        li = L.get(fn)
        ri = R.get(fn)
        if li is None or ri is None:
            nskip += 1
            continue
        lf, rf = frame_of(li), frame_of(ri)
        if lf is None or rf is None or lf == rf:
            nframe += 1
            continue
        if norm(li, lf) == norm(ri, rf):
            res[lf - rf].append((unit, fn))

print(f"frame-equal/none: {nframe}, missing: {nskip}")
total = 0
for d in sorted(res):
    print(f"  delta={d}: {len(res[d])}")
    total += len(res[d])
print(f"TOTAL pure-padding: {total}")
json.dump({str(k): v for k, v in res.items()}, open('scratch_pads.json', 'w'), indent=1)

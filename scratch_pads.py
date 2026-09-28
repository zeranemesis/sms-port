import json, re, subprocess
from collections import defaultdict

r = json.load(open('build/GMSP01/report.json'))

cands = []
for u in r['units']:
    if u['measures'].get('matched_code_percent', 0) >= 100.0:
        continue
    for f in u.get('functions', []):
        fp = f.get('fuzzy_match_percent')
        if fp is not None and 99.5 <= fp < 100.0:
            cands.append((u['name'], f['name']))

print(f"candidates (99.5-100): {len(cands)}", flush=True)

STWU = re.compile(r'stwu\s+r1,\s*-0x([0-9a-f]+)\(r1\)')

def frames(out):
    fl = fr = None
    for ln in out.splitlines():
        m = re.match(r'^\s*(~|<|>)?\s*[0-9a-f]+\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$', ln)
        if not m:
            continue
        l, rt = m.group(2), m.group(3)
        if STWU.search(l):
            fl = int(STWU.search(l).group(1), 16)
        if STWU.search(rt):
            fr = int(STWU.search(rt).group(1), 16)
    return fl, fr

results = []
for i, (unit, fn) in enumerate(cands):
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=90).stdout
    except Exception:
        continue
    fl, fr = frames(out)
    if fl is None or fr is None:
        continue
    delta = fl - fr
    real = []
    for ln in out.splitlines():
        m = re.match(r'^\s*(~|<|>)\s*[0-9a-f]+\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$', ln)
        if not m:
            continue
        mark, l, rt = m.groups()
        l2 = re.sub(r'@sda\d+\d?', '', l).strip()
        r2 = re.sub(r'@sda\d+\d?', '', rt).strip()
        if STWU.search(l2) or STWU.search(r2) or re.match(r'addi\s+r1,\s*r1,\s*0x[0-9a-f]+$', l2) or re.match(r'addi\s+r1,\s*r1,\s*0x[0-9a-f]+$', r2):
            continue
        ml = re.match(r'(\S+)\s+(r\d+)\s*,\s*(0x[0-9a-f]+)\(r1\)$', l2)
        mr = re.match(r'(\S+)\s+(r\d+)\s*,\s*(0x[0-9a-f]+)\(r1\)$', r2)
        if ml and mr and ml.group(1) == mr.group(1) and ml.group(2) == mr.group(2) and abs(int(ml.group(3), 16) - int(mr.group(3), 16)) == abs(delta):
            continue
        if re.match(r'b\w*\s+0x[0-9a-f]+$', l2) and re.match(r'b\w*\s+0x[0-9a-f]+$', r2):
            continue
        real.append((mark, l2, r2))
    results.append((unit, fn, fl, fr, delta, real))
    if i % 100 == 0:
        print(f"  {i}/{len(cands)}", flush=True)

pure = [x for x in results if not x[5] and x[4] > 0]
print(f"pure-padding candidates: {len(pure)}")
bydelta = defaultdict(list)
for x in pure:
    bydelta[x[4]].append(x)
for d in sorted(bydelta):
    print(f"  delta={d}: {len(bydelta[d])} fns")
json.dump([{'unit': x[0], 'fn': x[1], 'delta': x[4]} for x in pure], open('scratch_pads.json', 'w'), indent=1)
# sample of non-pure for debugging
samp = [x for x in results if x[5] and x[4] > 0][:5]
for x in samp:
    print("SAMPLE", x[0], x[1], "delta", x[4], "reals", len(x[5]))
    for rr in x[5][:6]:
        print("   ", rr)

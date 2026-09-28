import json, re, os, subprocess
from concurrent.futures import ThreadPoolExecutor

res = json.load(open('scratch_still_detail.json'))
# nerve execute fns: pattern execute__NNNClassCFP24TSpineBase<10TLiveActor>
todo = []
pads = json.load(open('scratch_pads.json'))
negset = {}
for d, lst in pads.items():
    d = int(d)
    if d >= 0:
        continue
    for unit, fn in lst:
        negset[(unit, fn)] = -d
for unit, fn, st, d, nrows, bad in res:
    if (unit, fn) in negset:
        m = re.match(r'execute__(\d+)(\w+?)CFP24TSpineBase<10TLiveActor>$', fn)
        if m:
            todo.append((unit, fn, negset[(unit, fn)], m.group(2)))
print(f"nerve execute fns: {len(todo)}")

def srcpath(unit):
    return os.path.join('src', unit.split('/', 1)[1] + '.cpp')

byfile = {}
for unit, fn, n, cls in todo:
    byfile.setdefault(srcpath(unit), []).append((unit, fn, n, cls))

applied, skipped = [], []
for p, items in byfile.items():
    if not os.path.exists(p):
        skipped.extend((u, f, 'no file') for u, f, _, _ in items)
        continue
    text = open(p, encoding='utf-8', errors='replace').read()
    for unit, fn, n, cls in items:
        name = re.sub(r'[^A-Za-z0-9_]', '', f"framePad_{n}_execute")
        if f'char {name}[' in text:
            skipped.append((unit, fn, 'already padded'))
            continue
        # find DEFINE_NERVE(cls, ...) then the execute line after it
        dm = re.search(r'DEFINE_NERVE\(\s*' + re.escape(cls) + r'\s*,\s*\w+\s*\)\s*\{', text)
        if not dm:
            skipped.append((unit, fn, f'no DEFINE_NERVE({cls})'))
            continue
        ins_at = dm.end()
        pad = (f"\n\t// Frame-padding: target frame is {n} bytes larger (MWCC stack-padding quirk).\n"
               f"\tchar {name}[{n}];\n\t(void){name};")
        text = text[:ins_at] + pad + text[ins_at:]
        applied.append((unit, fn, n, cls))
    open(p, 'w', encoding='utf-8', newline='').write(text)

print(f"applied: {len(applied)}")
print(f"skipped: {len(skipped)}")
for s in skipped:
    print("  ", s)
json.dump(applied, open('scratch_applied4.json', 'w'), indent=1)

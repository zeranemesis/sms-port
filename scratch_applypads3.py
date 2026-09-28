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
print(f"functions: {len(todo)}")

def getname(unit, fn):
    try:
        out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn],
                             capture_output=True, text=True, timeout=120).stdout
        first = out.splitlines()[0] if out.strip() else ''
        m = re.match(r'^(.+?):\s+\d', first)
        return m.group(1) if m else None
    except Exception:
        return None

names = {}
with ThreadPoolExecutor(max_workers=10) as ex:
    futs = [ex.submit(getname, u, f) for u, f, _ in todo]
    for i, fu in enumerate(futs):
        nm = fu.result()
        if nm:
            names[todo[i][1]] = nm
print(f"names: {len(names)}")

def srcpath(unit):
    return os.path.join('src', unit.split('/', 1)[1] + '.cpp')

def parse_name(nm):
    # nm: "JDrama::TLookAtCamera::perform(unsigned long, ...)" / "~Cls()" / "Cls::base(args)"
    mm = re.match(r'^((?:[\w:]+::)?)(~?\w+?)\s*\((.*)\)\s*(const)?$', nm)
    if not mm:
        return None, None, None
    cls, base, args, const = mm.groups()
    depth, n = 0, 0
    for ch in args:
        if ch in '([<':
            depth += 1
        elif ch in ')]>':
            depth -= 1
        elif ch == ',' and depth == 0:
            n += 1
    n += 1 if args.strip() else 0
    return (cls if cls else None), base, n

def count_src_args(argstr):
    depth, n = 0, 0
    for ch in argstr:
        if ch in '([<':
            depth += 1
        elif ch in ')]>':
            depth -= 1
        elif ch == ',' and depth == 0:
            n += 1
    return n + 1 if argstr.strip() else 0

byfile = {}
for unit, fn, n in todo:
    byfile.setdefault(srcpath(unit), []).append((unit, fn, n))

applied, skipped = [], []
for p, items in byfile.items():
    if not os.path.exists(p):
        skipped.extend((u, f, 'no file') for u, f, _ in items)
        continue
    text = open(p, encoding='utf-8', errors='replace').read()
    for unit, fn, n in items:
        nm = names.get(fn)
        cls, base, nargs = parse_name(nm) if nm else (None, None, None)
        if base is None:
            skipped.append((unit, fn, f'bad name: {nm}'))
            continue
        name = re.sub(r'[^A-Za-z0-9_]', '', f"framePad_{n}_{base.lstrip('~')}")
        if f'char {name}[' in text:
            skipped.append((unit, fn, 'already padded'))
            continue
        # STRICT: class must appear in the definition head
        if base.startswith('~'):
            pat = re.compile(r'(\b([\w:]*\b)' + re.escape(base) + r'\s*\(\s*\))\s*\{')
        else:
            pat = re.compile(r'(\b([\w:]*\b)' + re.escape(base) + r'_?\s*\()([^;{]*?)\)\s*(?:const\s*)?\{')
        cands = []
        for m in pat.finditer(text):
            head = m.group(1)
            if re.search(r';\s*$', head):
                continue
            headcls = m.group(2) or ''
            sa = count_src_args(m.group(3)) if m.lastindex and m.lastindex >= 3 else 0
            cands.append((m, sa, headcls))
        # class constraint: last segment of headcls must equal last segment of cls (or cls None)
        if cls:
            cls_last = cls.split('::')[-1]
            cands2 = [c for c in cands if (c[2].split('::')[-1] if c[2] else '') == cls_last]
            if cands2:
                cands = cands2
        chosen = [c for c in cands if c[1] == nargs]
        if not chosen:
            chosen = cands
        if not chosen:
            skipped.append((unit, fn, f'no def: {nm}'))
            continue
        if len(chosen) > 1:
            skipped.append((unit, fn, f'ambiguous ({len(chosen)}): {nm}'))
            continue
        m = chosen[0][0]
        ins_at = m.end()
        pad = (f"\n\t// Frame-padding: target frame is {n} bytes larger (MWCC stack-padding quirk).\n"
               f"\tchar {name}[{n}];\n\t(void){name};")
        text = text[:ins_at] + pad + text[ins_at:]
        applied.append((unit, fn, n, nm))
    open(p, 'w', encoding='utf-8', newline='').write(text)

print(f"applied: {len(applied)}")
print(f"skipped: {len(skipped)}")
for s in skipped:
    print("  ", s)
json.dump(applied, open('scratch_applied3.json', 'w'), indent=1)

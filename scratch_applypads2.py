import json, re, subprocess, os
from concurrent.futures import ThreadPoolExecutor

fixable = json.load(open('scratch_fixable.json'))
todo = [(u, f, -d) for u, f, p, d, bad in fixable]
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

def argcount(m):
    """top-level arg count from mangled name"""
    i = m.find('__')
    if i < 0:
        return None
    rest = m[i + 2:]
    mm = re.match(r'\d+[A-Za-z_][A-Za-z0-9_]*(?:<.*>)?', rest)
    t = rest[mm.end():] if mm else rest
    t = t.lstrip('F')
    if t == 'v' or t == '':
        return 0
    n, depth, i = 0, 0, 0
    while i < len(t):
        c = t[i]
        if c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
        elif depth == 0 and c in 'bhctijlmfdr':
            n += 1
        elif depth == 0 and c == 'v':
            return n
        elif depth == 0 and c in 'PQCF':
            pass
        elif depth == 0 and c.isupper():
            n += 1
        i += 1
    return n

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
    # returns (cls, base, nargs) — base may be '~Cls' for dtors
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
    return (cls[:-2] if cls else None), base, n

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
        mangled_args = argcount(fn)
        cands = []
        if base.startswith('~'):
            # destructor:  ~Cls()  or  ::~Cls()
            pat = re.compile(r'(\b[\w:<>, ]*' + re.escape(base) + r'\s*\(\s*\))\s*\{')
        else:
            pat = re.compile(r'(\b[\w:<>, ]*' + re.escape(base) + r'_?\s*\()([^;{]*?)\)\s*(?:const\s*)?\{')
        for m in pat.finditer(text):
            head = m.group(1)
            if re.search(r';\s*$', head):
                continue
            if cls and cls not in head:
                continue
            sa = count_src_args(m.group(2)) if m.lastindex and m.lastindex >= 2 else 0
            cands.append((m, sa))
        chosen = [c for c in cands if c[1] == (mangled_args if mangled_args is not None else nargs)]
        if not chosen:
            chosen = [c for c in cands if c[1] == nargs]
        if not chosen:
            chosen = cands
        if not chosen:
            skipped.append((unit, fn, f'no def: {nm}'))
            continue
        if len(chosen) > 1:
            # try to disambiguate: prefer candidate whose enclosing scope mentions a type unique to this mangled name
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
json.dump(applied, open('scratch_applied2.json', 'w'), indent=1)

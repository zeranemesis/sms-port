import json, re, os

names = json.load(open('scratch_names.json'))
todo = [(u, f, n, nm) for u, f, n, nm in names if nm and not str(nm).startswith('ERR')]
print(f"functions: {len(todo)}")

def srcpath(unit):
    return os.path.join('src', unit.split('/', 1)[1] + '.cpp')

def parse_name(nm):
    # "Cls::base(args) const" or "base(args)"
    mm = re.match(r'^((?:[\w:]+::)?)(\w+?)\s*\((.*)\)\s*(const)?$', nm)
    if not mm:
        return None, None, None
    cls, base, args, const = mm.groups()
    # count top-level args
    depth, n, i = 0, 0, 0
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
for unit, fn, n, nm in todo:
    byfile.setdefault(srcpath(unit), []).append((unit, fn, n, nm))

applied, skipped = [], []
for p, items in byfile.items():
    if not os.path.exists(p):
        skipped.extend((u, f, 'no file') for u, f, _, _ in items)
        continue
    text = open(p, encoding='utf-8', errors='replace').read()
    for unit, fn, n, nm in items:
        cls, base, nargs = parse_name(nm)
        if base is None:
            skipped.append((unit, fn, f'bad name: {nm}'))
            continue
        name = re.sub(r'[^A-Za-z0-9_]', '', f"framePad_{n}_{base}")
        if f'char {name}[' in text:
            skipped.append((unit, fn, 'already padded'))
            continue
        cands = []
        pat = re.compile(r'(\b[\w:<>, ]*' + re.escape(base) + r'_?\s*\()([^;{]*?)\)\s*(?:const\s*)?\{')
        for m in pat.finditer(text):
            head = m.group(1)
            if re.search(r';\s*$', head):
                continue
            if cls and cls not in head:
                continue
            sa = count_src_args(m.group(2))
            cands.append((m, sa))
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
json.dump(applied, open('scratch_applied.json', 'w'), indent=1)

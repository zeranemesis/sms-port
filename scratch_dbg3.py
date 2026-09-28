import re, subprocess
OBJDUMP = r'build\binutils\powerpc-eabi-objdump.exe'
HDR = re.compile(r'^\s*([0-9a-f]+) <(.+)>:$')
INS = re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s+(.+)$')
for side in ('src', 'obj'):
    out = subprocess.run([OBJDUMP, '-d', '--disassemble-all', f'build/GMSP01/{side}/JSystem/JAudio/JAInterface/JAIGlobalParameter.o'],
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
    print(side, "nfns:", len(funcs))
    for k, v in funcs.items():
        if 'setParam' in k:
            print("  ", k, "ninsn:", len(v))
            for i in v[:8]:
                print("    ", i)

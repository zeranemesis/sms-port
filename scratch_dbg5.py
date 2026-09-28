import re, subprocess
LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
ADDI_R1 = re.compile(r'addi\s+r1,\s*r1,\s*\{?(-?0x[0-9a-f]+)\}?$')
def hv(s):
    s = s.strip()
    if s.lstrip('-').lower().startswith('0x'):
        return int(s, 16)
    return int(s)
def norm(insn, frame):
    # normalize {0x..}(r1) -> (signed+frame)(r1)
    insn = re.sub(r'\{(-?0x[0-9a-f]+)\}\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
    # plain (0x..)(r1) -> (signed+frame)(r1)
    insn = re.sub(r'\((-?0x[0-9a-f]+)\)\(r1\)', lambda mm: f"({hv(mm.group(1)) + frame})(r1)", insn)
    # addi r1,r1,{0x..} -> addi r1,r1,frame
    insn = ADDI_R1.sub(lambda mm: f"addi r1,r1,{hv(mm.group(1)) + frame}", insn)
    return insn
def btarget(off, insn):
    m = re.match(r'(b\w*)\s+0x([0-9a-f]+)$', insn)
    if m:
        return f"{m.group(1)} 0x{int(off,16)+int(m.group(2),16):x}"
    return insn
out = subprocess.run(['python','tools/decomp-diff.py','-u','marioEU/JSystem/JAudio/JAInterface/JAIGlobalParameter','-d','setParamSoundOutputMode__18JAIGlobalParameterFUl'],capture_output=True,text=True).stdout
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
    rows.append((off, l.strip(), rt.strip(), mark))
bad = 0
for off, l, rt, mark in rows:
    l = btarget(off, l); rt = btarget(off, rt)
    nl = norm(l, fl); nr = norm(rt, fr)
    if nl != nr:
        bad += 1
        if bad <= 12:
            print("BAD:", repr(nl), "|", repr(nr))
print("bad=", bad, "fl=", fl, "fr=", fr)

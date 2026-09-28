import re, subprocess
LINE = re.compile(r'^\s*(~|<|>)?\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*\{?(-?0x[0-9a-f]+)\}?\(r1\)')
def hv(s):
    s = s.strip()
    if s.lstrip('-').lower().startswith('0x'):
        return int(s, 16)
    return int(s)
out = subprocess.run(['python','tools/decomp-diff.py','-u','marioEU/JSystem/JAudio/JAInterface/JAIGlobalParameter','-d','setParamSoundOutputMode__18JAIGlobalParameterFUl'],capture_output=True,text=True).stdout
fl = fr = None
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
print("fl=", fl, "fr=", fr)
print("delta=", (fl or 0)-(fr or 0))

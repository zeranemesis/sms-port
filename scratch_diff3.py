import re, subprocess, sys
LINE = re.compile(r'^\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
STWU = re.compile(r'stwu\s+r1,\s*(-?0x[0-9a-f]+)\(r1\)')
def diff(unit, fn):
    out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', unit, '-d', fn, '--no-collapse'],
                         capture_output=True, text=True, timeout=120).stdout
    fl = fr = None
    rows = []
    for ln in out.splitlines():
        m = LINE.match(ln)
        if not m:
            continue
        off, l, rt = m.groups()
        if fl is None:
            mm = STWU.search(l)
            if mm: fl = int(mm.group(1), 16)
        if fr is None:
            mm = STWU.search(rt)
            if mm: fr = int(mm.group(1), 16)
        if l.strip() != rt.strip():
            rows.append((off, l.strip(), rt.strip()))
    print(f"== {unit.split('/',1)[1]} :: {fn}  (frame L={fl and hex(fl)} R={fr and hex(fr)}, {len(rows)} diff lines)")
    for off, l, rt in rows[:25]:
        print(f"  {off}: L={l!r}  R={rt!r}")
    if len(rows) > 25:
        print(f"  ... {len(rows)-25} more")

diff('marioEU/JSystem/JParticle/JPADraw', 'initialize__7JPADrawFP14JPABaseEmitterP18JPATextureResource')
diff('marioEU/dolphin/os/__start', '__init_registers')
diff('marioEU/MoveBG/WoodBarrel', 'kill__11TWoodBarrelFv')

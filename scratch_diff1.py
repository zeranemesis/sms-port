import re, subprocess
LINE = re.compile(r'^\s*([0-9a-f]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*$')
out = subprocess.run(['python', 'tools/decomp-diff.py', '-u', 'marioEU/Enemy/gatekeeper',
                      '-d', 'execute__14TNerveBGKWait2CFP24TSpineBase<10TLiveActor>', '--no-collapse'],
                     capture_output=True, text=True, timeout=120).stdout
n = 0
for ln in out.splitlines():
    m = LINE.match(ln)
    if not m:
        continue
    off, l, rt = m.groups()
    if l.strip() != rt.strip():
        n += 1
        print(f"{off}: L={l.strip()!r}  R={rt.strip()!r}")
print(f"total diff lines: {n}")

import json, re, subprocess
OBJDUMP = r'build\binutils\powerpc-eabi-objdump.exe'
r = json.load(open('build/GMSP01/report.json'))
unit = 'marioEU/JSystem/JKernel/JKRExpHeap'
u = [x for x in r['units'] if x['name'] == unit][0]
for side in ('src', 'obj'):
    out = subprocess.run([OBJDUMP, '-d', '--disassemble-all', f'build/GMSP01/{side}/JSystem/JKernel/JKRExpHeap.o'],
                         capture_output=True, text=True).stdout
    starts = [int(m.group(1), 16) for m in re.finditer(r'^([0-9a-f]+) <(.+)>:$', out, re.M)]
    print(side, "starts:", [hex(s) for s in starts][:12])
    for f in u['functions']:
        off = int(f['address'])
        ok = off in starts
        print(f"  {f['name'][:40]:42} addr={off} ({hex(off)}) match={ok} fuzzy={f.get('fuzzy_match_percent')}")

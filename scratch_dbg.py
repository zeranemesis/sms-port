import re, subprocess
OBJDUMP = r'build\binutils\powerpc-eabi-objdump.exe'
out = subprocess.run([OBJDUMP, '-d', '--disassemble-all', r'build\GMSP01\src\JSystem\JKernel\JKRExpHeap.o'],
                     capture_output=True, text=True).stdout
hdr = re.compile(r'^([0-9a-f]+) <(.+)>:$')
ins = re.compile(r'^\s*([0-9a-f]+):\s+([0-9a-f ]+?)\s+(.+)$')
nh = ni = 0
for ln in out.splitlines():
    if hdr.match(ln):
        nh += 1
    elif ins.match(ln):
        ni += 1
print("headers:", nh, "insn:", ni)
for ln in out.splitlines():
    if '53c:' in ln or '540:' in ln:
        print(repr(ln))

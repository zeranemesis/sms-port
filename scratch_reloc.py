import subprocess
for tag, path in [('ORIG', r'build\GMSP01\obj\JSystem\JParticle\JPADraw.o'),
                  ('MINE', r'build\GMSP01\src\JSystem\JParticle\JPADraw.o')]:
    out = subprocess.run([r'build\binutils\powerpc-eabi-objdump.exe', '-r', path],
                         capture_output=True, text=True).stdout
    print(f"=== {tag} relocations in .text (first 30) ===")
    n = 0
    in_text = False
    for ln in out.splitlines():
        if 'RELOCATION' in ln:
            in_text = 'R_POWERPC' in ln
            continue
        if in_text:
            m = ln.strip()
            if m:
                print('  ', m)
                n += 1
            if n >= 30:
                break

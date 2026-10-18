"""Make the symbols a module object defines outright local (COFF hosts).

    python localize_module.py [--weaken] NM OBJCOPY MODULE.o

On ELF the modules are built with -fvisibility=hidden and objcopy
--localize-hidden keeps each module's names to itself, as Kuribo does
(Eclipse and the moveset both define gSettingsGroup). COFF has no hidden
visibility, so this localizes what the module defines, except in COMDAT
sections (inline functions, template instances): the linker keeps one copy of
each COMDAT section by name, and a local reference to a discarded copy would
resolve to nothing. Those copies are the same code in every module.

--weaken makes the object's global definitions weak instead (COMDAT or not),
as the inline definitions SunshineHeaderInterface's headers give for functions
the game defines outright are on ELF: the game's wins. Undefined references
stay strong, so a missing symbol fails the link.
"""
import os, re, subprocess, sys

weaken = sys.argv[1] == "--weaken"
if weaken:
    del sys.argv[1]
nm, objcopy, obj = sys.argv[1:4]
objdump = os.path.join(os.path.dirname(objcopy), "objdump" + os.path.splitext(objcopy)[1])
heads = subprocess.run([objdump, "-h", obj], capture_output=True, text=True, check=True).stdout.splitlines()
comdat = set()
for i, line in enumerate(heads):
    m = re.match(r"\s*(\d+)\s+(\S+)", line)
    if m and i + 1 < len(heads) and "LINK_ONCE" in heads[i + 1]:
        comdat.add(int(m.group(1)) + 1)  # objdump -t numbers sections from 1
names = set()
for line in subprocess.run([objdump, "-t", obj], capture_output=True, text=True, check=True).stdout.splitlines():
    # [  12](sec  3)(fl 0x00)(ty  20)(scl   2) (nx 0) 0x... name
    m = re.match(r"\[\s*\d+\]\(sec\s+(-?\d+)\).*\(scl\s+(\d+)\)\s+\(nx \d+\)\s+0x[0-9a-f]+\s+(\S+)", line)
    if m and int(m.group(2)) == 2 and int(m.group(1)) > 0 and (weaken or int(m.group(1)) not in comdat):
        names.add(m.group(3))
lst = obj + (".weaken" if weaken else ".localize")
with open(lst, "w") as f:
    f.write("\n".join(sorted(names)) + "\n")
subprocess.run([objcopy, ("--weaken-symbols=" if weaken else "--localize-symbols=") + lst, obj], check=True)
if weaken:
    print(f"{os.path.basename(obj)}: {len(names)} definitions made weak")
else:
    print(f"{os.path.basename(obj)}: {len(names)} symbols made local, {len(comdat)} COMDAT sections kept shared")

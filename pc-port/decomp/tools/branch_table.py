import re
import subprocess
import sys

asm = sys.argv[1]
obj = sys.argv[2] if len(sys.argv) > 2 else None
lines = open(asm, encoding="utf-8", errors="replace").read().split("\n")

INSN = re.compile(r"^/\*\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f ]+?\*/\s*(.*)$")

# ---- rodata offset -> string
objs = []
cur = None
in_rodata = False
for line in lines:
    s = line.strip()
    if s == ".rodata":
        in_rodata = True
        continue
    if s in (".data", ".sdata2", ".bss", ".sdata", ".sbss", ".text"):
        in_rodata = False
    if not in_rodata:
        continue
    m = re.match(r"^# \.rodata:(0x[0-9a-fA-F]+) \| .* size:", s)
    if m:
        cur = {"off": int(m.group(1), 16), "b": bytearray()}
        objs.append(cur)
        continue
    if cur is None:
        continue
    m = re.match(r"^\.4byte 0x([0-9A-Fa-f]{8})$", s)
    if m:
        cur["b"] += int(m.group(1), 16).to_bytes(4, "big")
        continue
    m = re.match(r"^\.2byte 0x([0-9A-Fa-f]{4})$", s)
    if m:
        cur["b"] += int(m.group(1), 16).to_bytes(2, "big")
        continue
    m = re.match(r"^\.byte (.*)$", s)
    if m:
        for tok in m.group(1).split(","):
            mm = re.match(r"^0x([0-9a-fA-F]{2})$", tok.strip())
            if mm:
                cur["b"].append(int(mm.group(1), 16))
        continue
    m = re.match(r'^\.string "(.*)"$', s)
    if m:
        cur["b"] += m.group(1).encode("latin-1") + b"\0"


def at(off):
    for o in objs:
        d = bytes(o["b"])
        if o["off"] <= off < o["off"] + max(len(d), 1):
            try:
                return d[off - o["off"] :].split(b"\0")[0].decode("shift_jis")
            except Exception:
                return None
    return None


# dtk does not dump .sdata2 in the .s file, yet short strings ("EMario",
# "Koopa", ...) live there and are loaded with `li r4, "@NNNN"@sda21`.
# Read them straight from the extracted object instead.
sdata2 = {}
if obj:
    TOOL = "build/binutils/powerpc-eabi-objdump.exe"
    dump = subprocess.run(
        [TOOL, "-s", "-j", ".sdata2", obj], capture_output=True, text=True,
        encoding="utf-8", errors="replace",
    ).stdout
    blob = bytearray()
    for line in dump.split("\n"):
        m = re.match(r"^\s+([0-9a-f]{4,8})\s+((?:[0-9a-f]{2,8}\s+){1,4})", line)
        if m:
            for w in m.group(2).split():
                blob += bytes.fromhex(w)
    syms = subprocess.run(
        [TOOL, "-t", obj], capture_output=True, text=True, encoding="utf-8",
        errors="replace",
    ).stdout
    for line in syms.split("\n"):
        m = re.match(r"^([0-9a-f]{8})\s+l\s+O\s+\.sdata2\s+([0-9a-f]{8})\s+(@\d+)", line)
        if m:
            off = int(m.group(1), 16)
            try:
                sdata2[m.group(3)] = bytes(blob[off:]).split(b"\0")[0].decode("shift_jis")
            except Exception:
                pass


def sdata_at(sym):
    return sdata2.get(sym)


ins = []
for line in lines:
    m = INSN.match(line.strip())
    ins.append(m.group(1).strip() if m else None)

SKIP = ("strcmp", "__nw__", "operator new", "calcKeyCode", "strlen", "strcpy", "memcpy")

print("%-36s %-8s %s" % ("STRING", "SIZE", "CONSTRUCTED BY"))
print("-" * 92)
for i, s in enumerate(ins):
    if not (s and s.startswith("bl strcmp")):
        continue
    key = None
    for j in range(i - 1, max(i - 7, -1), -1):
        t = ins[j]
        if t:
            m = re.match(r"addi r\d+, r\d+, (0x[0-9a-fA-F]+)$", t)
            if m:
                key = at(int(m.group(1), 16))
                break
            m = re.match(r'li r\d+, "(@\d+)"@sda21$', t)
            if m:
                key = sdata_at(m.group(1))
                break
    ctor, size, arg = None, None, None
    for j in range(i, min(i + 24, len(ins))):
        t = ins[j]
        if not t:
            continue
        m = re.match(r"li r3, (0x[0-9a-fA-F]+)$", t)
        if m and int(m.group(1), 16) > 0x20 and size is None:
            size = m.group(1)
        if t.startswith("addi r4, ") and ctor is None:
            m = re.match(r"addi r4, (r\d+|r31|@?\w*), (0x[0-9a-fA-F]+)$", t)
            if m:
                a = at(int(m.group(2), 16))
                if a:
                    arg = a
                elif m.group(1) == "r31":
                    arg = "r31+" + m.group(2)
                else:
                    arg = m.group(1)
        m = re.match(r'li r4, "(@\d+)"@sda21$', t)
        if m and ctor is None:
            v = sdata_at(m.group(1))
            if v:
                arg = v
        if t.startswith("bl ") and not any(k in t for k in SKIP):
            ctor = t[3:].strip()
            break
    print(
        "%-30s %-7s %-34s %s"
        % (ascii(key) if key else "?", size or "?", ctor or "?", ascii(arg) if arg else "")
    )

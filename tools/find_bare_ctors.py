import re
import os

# Find classes whose header declares `X(const char* name = "...");` with no
# body. For each, report whether the target binary has a __ct__ symbol for it:
#   no  symbol -> ctor must be inline in the header (MWCC inlines it everywhere)
#   has symbol -> ctor is genuinely out-of-line, leave it alone
syms = open("config/GMSP01/symbols.txt", encoding="utf-8", errors="replace").read()

cands = []
for root, dirs, files in os.walk("include"):
    for fn in sorted(files):
        if not fn.endswith((".hpp", ".h")):
            continue
        p = os.path.join(root, fn)
        txt = open(p, encoding="utf-8", errors="replace").read()
        lines = txt.split("\n")
        for i, line in enumerate(lines):
            m = re.match(r"\s*class\s+(\w+)\s*:\s*public\s+([\w:]+)\s*\{", line)
            if not m:
                continue
            cls, base = m.group(1), m.group(2).split("::")[-1]
            # bare declaration on one of the following lines, no body after it
            for j in range(i + 1, min(i + 40, len(lines))):
                d = re.match(r"\s*" + re.escape(cls) + r"\(const char\* name = .*\);\s*$", lines[j])
                if d:
                    nxt = lines[j + 1] if j + 1 < len(lines) else ""
                    if re.match(r"\s*:\s*\w+\(name\)", nxt):
                        break  # already has a body
                    has_ctor_sym = (
                        re.search(r"__ct__\d+" + re.escape(cls) + r"F", syms) is not None
                    )
                    cands.append((p, cls, base, has_ctor_sym))
                    break

no_sym = [c for c in cands if not c[3]]
has_sym = [c for c in cands if c[3]]

# which of the "should be inline" classes are actually instantiated in src/?
inst = {}
for root, dirs, files in os.walk("src"):
    for fn in sorted(files):
        if not fn.endswith(".cpp"):
            continue
        p = os.path.join(root, fn)
        txt = open(p, encoding="utf-8", errors="replace").read()
        for c in no_sym:
            if re.search(r"new\s+" + re.escape(c[1]) + r"\b", txt):
                inst.setdefault(c[1], []).append(p)

print("candidates with bare ctor declaration + default arg: %d" % len(cands))
print("  -> NO __ct__ symbol in target (should be inline in header): %d" % len(no_sym))
print("  -> HAS __ct__ symbol in target (keep out-of-line)         : %d" % len(has_sym))
print("  -> of the inline ones, instantiated in src/              : %d" % len(inst))
print()
for p, cls, base, hs in sorted(no_sym, key=lambda x: x[0]):
    where = inst.get(cls)
    tag = ("USED in " + ", ".join(where)) if where else "never instantiated"
    print("  INLINE  %-28s base=%-22s %-34s %s" % (cls, base, p, tag))

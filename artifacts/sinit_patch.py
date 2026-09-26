"""Insert the rogue MSound include pair that reproduces __sinit_<TU>_cpp.

Every TU whose __sinit is missing registers the same 15 JSUList<T>::smList
template statics from JALList.hpp, and pulling <MSound/MSSetSound.hpp> +
<MSound/MSoundBGM.hpp> in is what makes MWCC emit them (see the reference
block in src/Enemy/effectObj.cpp).  The block goes right after the last
top-level #include so it cannot disturb the rest of the include ordering
(which drives .bss / sinit layout).

Skipped when the pair is already present or when --exclude names the unit.
"""

import io
import os
import sys

PAIR = (
    "\n// rogue include: pulls in JALList.hpp's JSUList<T>::smList template\n"
    "// statics, which is what marioEU.dol registers from __sinit_<TU>_cpp\n"
    "// (see the same block in src/Enemy/effectObj.cpp).\n"
    "#include <MSound/MSSetSound.hpp>\n"
    "#include <MSound/MSoundBGM.hpp>\n"
)


def patch(path):
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        lines = f.readlines()

    if any("MSSetSound.hpp" in ln for ln in lines[:60]):
        return "already"

    last = -1
    for i, ln in enumerate(lines):
        if ln.startswith("#include"):
            last = i
    if last < 0:
        return "no include"

    lines.insert(last + 1, PAIR)
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.writelines(lines)
    return "patched"


def main():
    targets = [a for a in sys.argv[1:] if not a.startswith("-")]
    for unit in targets:
        rel = unit.split("/", 1)[1]
        path = os.path.join("src", rel + ".cpp")
        if not os.path.exists(path):
            print("%-40s MISSING SOURCE" % path)
            continue
        print("%-40s %s" % (path, patch(path)))


if __name__ == "__main__":
    main()

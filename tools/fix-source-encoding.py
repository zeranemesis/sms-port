#!/usr/bin/env python3
"""Repair / verify the encoding of the game sources.

MWCC's `sjiswrap` wrapper rejects a source file whose first byte is a UTF-8 BOM with a
confusing "declaration syntax error" on line 1.  The BOM is almost always introduced
accidentally by a PowerShell `Set-Content` / `Out-File` round-trip, which is easy to do
because the sources are UTF-8 with Japanese Shift-JIS-ish literals.

This script:
  * finds every .cpp/.hpp under src/ and include/ that starts with a BOM,
  * strips it in place (--fix), or just lists the offenders (default),
  * additionally reports any file that is NOT valid UTF-8, since a lossy round-trip
    (e.g. `Get-Content | Set-Content` under a Shift-JIS code page) silently mangles the
    Japanese string literals and will quietly break .rodata offsets.

usage:  python tools\\fix-source-encoding.py          # report only
        python tools\\fix-source-encoding.py --fix    # strip the BOMs
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOM = b"\xef\xbb\xbf"
DIRS = ("src", "include")


def sources():
    for d in DIRS:
        base = os.path.join(ROOT, d)
        for dirpath, _dirs, files in os.walk(base):
            for name in files:
                if name.endswith((".cpp", ".hpp", ".h")):
                    yield os.path.join(dirpath, name)


def main():
    fix = "--fix" in sys.argv
    bad_bom = []
    bad_utf8 = []
    for path in sources():
        with open(path, "rb") as fh:
            data = fh.read()
        rel = os.path.relpath(path, ROOT)
        if data.startswith(BOM):
            bad_bom.append(rel)
            if fix:
                with open(path, "wb") as fh:
                    fh.write(data[len(BOM):])
        else:
            try:
                data.decode("utf-8")
            except UnicodeDecodeError as exc:
                bad_utf8.append((rel, exc.start))

    for rel in bad_bom:
        print(("stripped BOM  " if fix else "BOM present   ") + rel)
    for rel, off in bad_utf8:
        print("NOT UTF-8      %s (byte %d) - a lossy round-trip mangled it" % (rel, off))

    if not bad_bom and not bad_utf8:
        print("all %s sources are BOM-free and valid UTF-8" % " + ".join(DIRS))
        return 0
    if bad_utf8:
        # Never auto-fix this: it needs a human to decide what the bytes should be.
        print("\n%d file(s) are not valid UTF-8 and were NOT touched." % len(bad_utf8))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

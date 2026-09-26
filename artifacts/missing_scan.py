"""List map symbols our objects are missing, split LIVE vs UNUSED.

LIVE missing  == real code present in the ROM that we have not written; this
                 costs matched_code directly and is the top priority.
UNUSED missing == compiled but dead-stripped; useful mainly as a size
                 constraint on a reconstructed inline body.

Usage: python artifacts/missing_scan.py [unit-substring ...]
"""
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from map_parse import load_map  # noqa: E402

SKIP_PREFIX = ('mario/JSystem', 'mario/dolphin', 'mario/PowerPC',
               'mario/THPPlayer', 'mario/Reaction')


def own_units():
    with open('objdiff.json', encoding='utf-8', errors='replace') as f:
        cfg = json.load(f)
    return {u['name']: (u['base_path'], u['target_path'])
            for u in cfg.get('units', []) if u.get('base_path')}


def main():
    entries, stats = load_map()
    units = own_units()
    print('map symbols %d (%s)   objdiff units %d'
          % (len(entries), stats, len(units)))

    want = sys.argv[1:]
    rows = []
    for unit, (base, tgt) in sorted(units.items()):
        if unit.startswith(SKIP_PREFIX):
            continue
        if want and not any(w in unit for w in want):
            continue
        fname = os.path.splitext(os.path.basename(base))[0] + '.cpp'
        have = set()
        if os.path.exists(base):
            try:
                res = subprocess.run(
                    ['build/binutils/powerpc-eabi-nm.exe', '-S', base],
                    capture_output=True, text=True, errors='replace',
                    timeout=120)
                for ln in res.stdout.splitlines():
                    parts = ln.split()
                    if len(parts) >= 3:
                        have.add(parts[-1])
            except Exception:
                pass
        for sym, e in entries.items():
            if e['file'] != fname:
                continue
            if e.get('section') != '.text':
                continue          # .rodata/.sdata2/... are data, not functions
            if sym in have:
                continue
            if 'JSystem' in e['lib'] or 'MSL_' in e['lib']:
                continue
            rows.append((unit, 'UNUSED' if e['unused'] else 'LIVE',
                         e['size'], sym))

    for kind in ('LIVE', 'UNUSED'):
        sel = [r for r in rows if r[1] == kind]
        sel.sort(key=lambda r: -r[2])
        print('\n===== %s missing: %d  (total 0x%x = %d B) ====='
              % (kind, len(sel), sum(r[2] for r in sel),
                 sum(r[2] for r in sel)))
        for unit, _, size, sym in sel[:45]:
            print('  0x%-6x %-56s %s' % (size, sym[:56], unit))


if __name__ == '__main__':
    main()

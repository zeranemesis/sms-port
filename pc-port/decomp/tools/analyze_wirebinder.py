import json, subprocess, os, sys

root = os.getcwd()
objdiff = os.path.join(root, 'build', 'tools', 'objdiff-cli.exe')
result = subprocess.run([objdiff, 'diff', '-c', 'functionRelocDiffs=data_value', '-u', 'mario/Enemy/wireBinder', '-o', '-', '--format', 'json'], capture_output=True, cwd=root)
data = json.loads(result.stdout)

left_syms = data.get('left', {}).get('symbols', [])
right_syms = data.get('right', {}).get('symbols', [])

print('LEFT (original) symbols:')
for i, s in enumerate(left_syms):
    nm = s.get('demangled_name', s.get('name', '?'))
    mp = s.get('match_percent')
    kind = s.get('kind')
    size = s.get('size')
    target = s.get('target_symbol')
    print(f'  [{i}] {nm} mp={mp} kind={kind} size={size} target={target}')

print()
print('RIGHT (decomp) symbols:')
for i, s in enumerate(right_syms):
    nm = s.get('demangled_name', s.get('name', '?'))
    mp = s.get('match_percent')
    kind = s.get('kind')
    size = s.get('size')
    target = s.get('target_symbol')
    print(f'  [{i}] {nm} mp={mp} kind={kind} size={size} target={target}')

print()
print('LEFT sections:')
for s in data.get('left', {}).get('sections', []):
    print(f'  {s.get("name")} kind={s.get("kind")} size={s.get("size")}')
print()
print('RIGHT sections:')
for s in data.get('right', {}).get('sections', []):
    print(f'  {s.get("name")} kind={s.get("kind")} size={s.get("size")}')

# Now focus on bind function specifically
print()
print('=== BIND FUNCTION DETAILED DIFF ===')
for sym in left_syms:
    nm = sym.get('demangled_name', sym.get('name', ''))
    if 'bind' in nm.lower() and 'TWireBinder' in nm:
        target_idx = sym.get('target_symbol')
        if target_idx is not None and target_idx < len(right_syms):
            right_sym = right_syms[target_idx]
            # Compare instruction by instruction with data_value diffing
            left_insts = sym.get('instructions', [])
            right_insts = right_sym.get('instructions', [])
            for i in range(max(len(left_insts), len(right_insts))):
                l = left_insts[i] if i < len(left_insts) else {}
                r = right_insts[i] if i < len(right_insts) else {}
                l_fmt = l.get('formatted', '')
                r_fmt = r.get('formatted', '')
                l_diff = l.get('diff_kind', '')
                r_diff = r.get('diff_kind', '')
                if l_diff or r_diff:
                    print(f'  diff at inst {i}: L={l_fmt} R={r_fmt} L_kind={l_diff} R_kind={r_diff}')
        else:
            print(f'bind has no target: target={target_idx}')
        break

"""Generate the Android-only SPC adaptation against the existing PC mirror.

Original console sources are never written. Apply this patch after earlier
decomp-patches; the existing Windows mirror supplies that exact base.
"""
from pathlib import Path
import difflib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
MIRROR = ROOT / 'build/windows-64-pal-aurora/patched'
OUT = ROOT / 'artifacts/android-spc'
OUT.mkdir(parents=True, exist_ok=True)
paths = ['include/Strategic/spcinterp.hpp', 'src/Strategic/spcinterp.cpp',
         'src/Strategic/liveinterp.cpp', 'src/Enemy/enemyinterp.cpp',
         'src/System/EventWatcher.cpp', 'src/NPC/NpcEvent.cpp']
changes = []
counts = {}
for name in paths:
    source = MIRROR / name
    if not source.exists():
        source = ROOT / 'decomp' / name
    before = source.read_bytes().decode('latin1')
    # Normalize CRLF only for patch syntax; source bytes otherwise stay intact.
    before = before.replace('\r\n', '\n')
    base = OUT / 'base' / name
    base.parent.mkdir(parents=True, exist_ok=True)
    base.write_bytes(before.encode('latin1'))
    after = before
    if name.endswith('.cpp'):
        pattern = r'(bindSystemDataToSymbol\([^,\n]+,\s*)\(u32\)(&[A-Za-z_][A-Za-z_0-9]*)(\))'
        after, count = re.subn(pattern, r'\1PORT_SPC_NATIVE(\2)\3', after)
        counts[name] = count
        assert count, name
        remaining = re.findall(r'bindSystemDataToSymbol\([^;]+;', after)
        assert all('(u32)&' not in call for call in remaining), name
    if name.endswith('.hpp'):
        guard = '#define STRATEGIC_SPCINTERP_HPP\n'
        assert after.count(guard) == 1
        addition = '''
#if defined(TARGET_PC) && defined(__ANDROID__)
#include "spc_native_registry.h"
#define PORT_SPC_NATIVE(callback) port_spc_register(callback)
#else
#define PORT_SPC_NATIVE(callback) ((u32)(callback))
#endif
'''
        after = after.replace(guard, guard + addition)
        marker = '\tvirtual void dispatchBuiltin(u32 sym_index, u32 arg_count)\n\t{'
        assert after.count(marker) == 1
        start = after.index(marker) + len(marker)
        end = after.index('\n\t}\n', start)
        original = after[start:end]
        mobile = '''
#if defined(TARGET_PC) && defined(__ANDROID__)
        TSpcSymbol* sym = mBinary->getSymbol(sym_index);
        if (sym != nullptr && sym->mNativeCall != 0) {
            mCurrentlyExecutingBuiltinName = mBinary->getSymbolName(sym);
            port_spc_call_checked(sym->mNativeCall, this, arg_count);
            return;
        }
        TSpcInterp::dispatchBuiltin(sym_index, arg_count);
#else'''
        after = after[:start] + mobile + original + '\n#endif' + after[end:]
    if name == 'src/Strategic/spcinterp.cpp':
        old = '\ttypedef void (*Call)(TSpcInterp*, u32);\n\tCall call = (Call)sym->mNativeCall;\n\tif (call == nullptr) {'
        new = '''#if !defined(TARGET_PC) || !defined(__ANDROID__)
\ttypedef void (*Call)(TSpcInterp*, u32);
\tCall call = (Call)sym->mNativeCall;
#endif
\tif (sym->mNativeCall == 0) {'''
        assert after.count(old) == 1
        after = after.replace(old, new)
        call = '\tcall(this, arg_count);'
        assert after.count(call) == 1
        after = after.replace(call, '''#if defined(TARGET_PC) && defined(__ANDROID__)
\tport_spc_call_checked(sym->mNativeCall, this, arg_count);
#else
\tcall(this, arg_count);
#endif''')
    changes.append(''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                              fromfile='a/' + name, tofile='b/' + name)))
    target = OUT / 'patched' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(after.encode('latin1'))
patch = ROOT / 'decomp-patches/zzzzz-mobile-spc-native-handles.patch'
patch.write_bytes(''.join(changes).encode('latin1'))
(OUT / 'bindings.json').write_text(json.dumps(counts, indent=2) + '\n')
print(json.dumps(counts, indent=2))
print(patch)

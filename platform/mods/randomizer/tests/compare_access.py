"""Compare native access masks against v0.3.6 PowerPC code.

Usage: compare_access.py Randomizer.kxe access_probe.exe
Requires Unicorn for the test only.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_LR, UC_PPC_REG_PC


def main():
    module = Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(module).hexdigest() != 'a6033a3efb81a931c18f8bfb421898ba62665f681124faf1326d004079b118e7':
        raise ValueError('Expected the original v0.3.6 module')
    offset, size = struct.unpack_from('>II', module, 16)
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(base, 0x10000)
    cpu.mem_map(state, 0x10000)
    cpu.mem_write(base, module[offset:offset + size])
    shines = [[], [6, 16, 26, 36, 46, 56, 66], [6, 16, 26, 36, 46, 56],
              [6, 16, 26, 36, 46, 56, 66, 6], [6] * 7,
              [5, 7, 15, 17, 25, 27, 35], [6, 16, 26, 36, 46, 56, 66, 0, 119]]
    for pattern, ids in enumerate(shines):
        vector = state + 0x3000 + 24 * pattern
        data = state + 0x4000 + 128 * pattern
        cpu.mem_write(vector, struct.pack('>6I', 0, data, data + 8 * len(ids), data + 8 * len(ids), 0x40000000, 0))
        for i, shine in enumerate(ids):
            cpu.mem_write(data + 8 * i, bytes([shine]) + bytes(7))
    result = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    count = 0
    for line in result.stdout.splitlines():
        unlocks, pattern, native = map(int, line.split())
        if not 0 <= pattern < len(shines) or not 0 <= unlocks <= 0x7fffff:
            raise ValueError('Invalid probe input')
        cpu.mem_write(state + 0x20f4, struct.pack('>I', unlocks))
        cpu.reg_write(UC_PPC_REG_3, state)
        cpu.reg_write(UC_PPC_REG_4, state + 0x3000 + 24 * pattern)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        cpu.emu_start(base + 0x3840, stop, count=1000)
        original = cpu.reg_read(UC_PPC_REG_3)
        if cpu.reg_read(UC_PPC_REG_PC) != stop or original != native:
            raise AssertionError(f'Access mismatch: unlocks={unlocks:x}, pattern={pattern}, native={native:x}, original={original:x}')
        count += 1
    if count != 30779:
        raise AssertionError(f'Incomplete access probe: {count} cases')
    print(json.dumps(dict(cases=count, matched=True)))


if __name__ == '__main__':
    main()

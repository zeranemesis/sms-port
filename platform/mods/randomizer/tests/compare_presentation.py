"""Compare native rainbow RGB with original v0.3.6 HSL PowerPC routines.

Usage: compare_presentation.py Randomizer.kxe presentation_probe.exe
"""
import json
from pathlib import Path
import struct
import subprocess
import sys

from compare_rules import original_code
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN
from unicorn.ppc_const import (UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_LR, UC_PPC_REG_PC,
                               UC_PPC_REG_MSR, UC_PPC_REG_FPR1, UC_PPC_REG_FPR2, UC_PPC_REG_FPR3)


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address in (base, state, 0x82000000): cpu.mem_map(address, 0x10000)
    cpu.mem_write(base, original_code(Path(sys.argv[1]).read_bytes()))
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)
    output = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    count = 0
    for line in output.stdout.splitlines():
        bits, *native = map(int, line.split())
        hue, = struct.unpack('>f', struct.pack('>I', bits))
        cpu.reg_write(UC_PPC_REG_1, 0x8200ff00)
        cpu.reg_write(UC_PPC_REG_3, state)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        lightness, = struct.unpack('>f', struct.pack('>f', .55))
        for reg, value in ((UC_PPC_REG_FPR1, hue), (UC_PPC_REG_FPR2, 1), (UC_PPC_REG_FPR3, lightness)):
            cpu.reg_write(reg, struct.unpack('>Q', struct.pack('>d', value))[0])
        cpu.emu_start(base + 0x5f6c, stop, count=1000)
        original = list(map(int, struct.unpack('>3f', cpu.mem_read(state, 12))))
        if cpu.reg_read(UC_PPC_REG_PC) != stop or original != native:
            raise AssertionError(f'hue={hue}: original={original}, native={native}')
        count += 1
    if count != 1025: raise AssertionError('Incomplete rainbow comparison')
    print(json.dumps(dict(rainbow_colors=count, matched_original=True)))


if __name__ == '__main__':
    main()

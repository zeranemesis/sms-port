"""Compare movement restrictions to the original v0.3.6 PowerPC callback.

Usage: compare_actions.py Randomizer.kxe actions_probe.exe
Retail transition/physics calls are stubbed; this checks unlock policy only.
"""
import json
from pathlib import Path
import struct
import subprocess
import sys

from compare_rules import original_code
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import (UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4,
                               UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_LR,
                               UC_PPC_REG_PC, UC_PPC_REG_MSR)


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (state, 0x10000),
                          (0x82000000, 0x10000), (0x80000000, 0x410000)):
        cpu.mem_map(address, size)
    cpu.mem_write(base, original_code(Path(sys.argv[1]).read_bytes()))
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)

    def retail(machine, address, size, user):
        if address == 0x802543a0:
            machine.reg_write(UC_PPC_REG_3, machine.reg_read(UC_PPC_REG_4))
        elif address == 0x80254034:
            machine.mem_write(state + 0x7c, struct.pack('>I', machine.reg_read(UC_PPC_REG_4)))
            machine.reg_write(UC_PPC_REG_3, 1)
        elif address == 0x802a8afc:
            machine.reg_write(UC_PPC_REG_3, 0)
        elif address != 0x80018864:
            raise AssertionError(f'Unexpected retail call {address:x}')
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    cpu.hook_add(UC_HOOK_CODE, retail, begin=0x80000000, end=0x8040ffff)
    output = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    count = 0
    for line in output.stdout.splitlines():
        mask, status, player, target, allowed, clear_holder = map(int, line.split())
        cpu.mem_write(base + 0x9ed1, bytes(bool(mask & (1 << bit)) for bit in range(23)))
        cpu.mem_write(0x8040e0e8, struct.pack('>I', state if player else state + 0x1000))
        cpu.mem_write(state, bytes(0x200))
        cpu.mem_write(state + 0x7c, struct.pack('>I', 0x0c400201))
        cpu.mem_write(state + 0x68, struct.pack('>I', state + 0x1000))
        for register, value in ((UC_PPC_REG_1, 0x8200ff00), (UC_PPC_REG_3, state),
                                (UC_PPC_REG_4, status), (UC_PPC_REG_5, 0),
                                (UC_PPC_REG_6, 0), (UC_PPC_REG_LR, stop)):
            cpu.reg_write(register, value)
        cpu.emu_start(base + 0x5704, stop, count=1000)
        actual = (cpu.reg_read(UC_PPC_REG_3),
                  struct.unpack('>I', cpu.mem_read(state + 0x7c, 4))[0],
                  bytes(cpu.mem_read(state + 0x68, 4)) == bytes(4))
        expected = (allowed, target if allowed else 0x0c400201, bool(clear_holder))
        if cpu.reg_read(UC_PPC_REG_PC) != stop or actual != expected:
            raise AssertionError(f'mask={mask:x}, status={status:x}, player={player}: '
                                 f'original={actual}, native={expected}')
        count += 1
    if count != 45066:
        raise AssertionError(f'Incomplete movement probe: {count}')
    print(json.dumps(dict(cases=count, matched=True, scope='unlock policy; retail calls stubbed')))


if __name__ == '__main__':
    main()

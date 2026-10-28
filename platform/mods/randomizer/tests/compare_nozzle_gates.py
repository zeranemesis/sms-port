"""Compare nozzle/backup eligibility with the v0.3.6 PowerPC callbacks.

Usage: compare_nozzle_gates.py Randomizer.kxe nozzle_gate_probe.exe
Stops before the game's ordinary nozzle physics and model updates.
"""
import json
from pathlib import Path
import struct
import subprocess
import sys

from compare_rules import original_code
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import (UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4,
                               UC_PPC_REG_5, UC_PPC_REG_LR, UC_PPC_REG_PC,
                               UC_PPC_REG_MSR)


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (state, 0x10000), (0x82000000, 0x10000),
                          (0x80000000, 0x410000)):
        cpu.mem_map(address, size)
    cpu.mem_write(base, original_code(Path(sys.argv[1]).read_bytes()))
    cpu.mem_write(0x8040e0e8, struct.pack('>I', state))
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)

    def finish_gate(machine, address, size, user):
        machine.emu_stop()

    # Nozzle acceptance/rejection, then backup acceptance/rejection respectively.
    for address in (base + 0x5cc4, base + 0x5dbc, base + 0x5b34, base + 0x5b3c):
        cpu.hook_add(UC_HOOK_CODE, finish_gate, begin=address, end=address)
    output = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    count = 0
    for line in output.stdout.splitlines():
        mask, nozzle, allowed, backup = map(int, line.split())
        cpu.mem_write(base + 0x9ed1, bytes(bool(mask & (1 << bit)) for bit in range(23)))
        for force in (0, 1):
            for offset, expected, accepted, rejected in (
                    (0x5b60, allowed, 0x5cc4, 0x5dbc),
                    (0x5af4, backup, 0x5b34, 0x5b3c)):
                for register, value in ((UC_PPC_REG_1, 0x8200ff00), (UC_PPC_REG_3, state),
                                        (UC_PPC_REG_4, nozzle), (UC_PPC_REG_5, force),
                                        (UC_PPC_REG_LR, stop)):
                    cpu.reg_write(register, value)
                cpu.emu_start(base + offset, stop, count=1000)
                pc = cpu.reg_read(UC_PPC_REG_PC)
                if pc != base + (accepted if expected else rejected):
                    raise AssertionError(f'mask={mask:x}, nozzle={nozzle}, callback={offset:x}, '
                                         f'force={force}: original PC={pc:x}, native={expected}')
                count += 1
    if count != 768:
        raise AssertionError(f'Incomplete nozzle probe: {count}')
    print(json.dumps(dict(cases=count, matched=True, scope='nozzle and backup eligibility')))


if __name__ == '__main__':
    main()

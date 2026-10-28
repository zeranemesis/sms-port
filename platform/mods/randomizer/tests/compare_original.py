"""Compare native transitions with original v0.3.6 PowerPC instructions.

Usage: compare_original.py Randomizer.kxe transitions_probe.exe rewards_probe.exe
Requires Unicorn for this test only; no module or emulator is bundled in the port.
"""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_LR, UC_PPC_REG_PC


def main():
    module = Path(sys.argv[1]).read_bytes()
    expected = 'a6033a3efb81a931c18f8bfb421898ba62665f681124faf1326d004079b118e7'
    if hashlib.sha256(module).hexdigest() != expected:
        raise ValueError('This comparison requires the original v0.3.6 module')
    offset, size = struct.unpack_from('>II', module, 16)
    base, stack, state, stop = 0x81000000, 0x82000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address in (base, stack, state):
        cpu.mem_map(address, 0x10000)
    cpu.mem_write(base, module[offset:offset + size])
    result = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    comparisons = 0
    for line in result.stdout.splitlines():
        count, scene, native_exit, native_index = map(int, line.split())
        if count not in (0, 1, 128):
            raise ValueError('Unexpected probe table size')
        data = bytearray(0x1000)
        struct.pack_into('>I', data, 0x304, count)
        struct.pack_into('>H', data, 0x508, count)
        for i in range(count):
            key = (i % 31) * 17
            struct.pack_into('>H', data, 4 + 2 * i, key)
            struct.pack_into('>I', data, 0x104 + 4 * i, i)
            struct.pack_into('>H', data, 0x308 + 2 * i, key)
            struct.pack_into('>H', data, 0x408 + 2 * i, 0x200 + i * 13)
        cpu.mem_write(state, bytes(data))
        for function, native in ((0x1ec4, native_exit), (0x1f14, native_index)):
            cpu.reg_write(UC_PPC_REG_1, stack + 0xff00)
            cpu.reg_write(UC_PPC_REG_3, state)
            cpu.reg_write(UC_PPC_REG_4, scene)
            cpu.reg_write(UC_PPC_REG_LR, stop)
            cpu.emu_start(base + function, stop, count=20000)
            if cpu.reg_read(UC_PPC_REG_PC) != stop:
                raise AssertionError('Original function did not return within the instruction limit')
            original = cpu.reg_read(UC_PPC_REG_3)
            if original != native:
                raise AssertionError(f'{function:x}, table={count}, scene={scene:x}: native={native}, original={original}')
            comparisons += 1
    if comparisons != 384:
        raise AssertionError(f'Incomplete probe output: {comparisons} comparisons')
    # The original restoration function calls the retail Shine flag accessor.
    # Only this accessor is supplied by the harness; the loop and OR run as PPC.
    cpu.mem_map(0x80400000, 0x20000)
    stub = base + 0xffe0
    branch = 0x48000001 | ((stub - (base + 0x1d44)) & 0x03fffffc)
    cpu.mem_write(base + 0x1d44, struct.pack('>I', branch))
    cpu.mem_write(0x8040e160, struct.pack('>I', state + 0x8000))
    flags = []

    def shine_flag(machine, address, size, user):
        index = machine.reg_read(UC_PPC_REG_4)
        if index >= 120:
            raise AssertionError('Shine accessor called outside the retail range')
        machine.reg_write(UC_PPC_REG_3, int(flags[index]))
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    cpu.hook_add(UC_HOOK_CODE, shine_flag, begin=stub, end=stub)
    rewards_output = subprocess.run([sys.argv[3]], capture_output=True, text=True, check=True)
    reward_cases = 0
    for line in rewards_output.stdout.splitlines():
        scenario, native = map(int, line.split())
        flags = [i == scenario if scenario < 120 else (i + scenario) % 3 == 0 for i in range(120)]
        cpu.mem_write(state + 0x50c, struct.pack('>120I', *(1 << (i % 23) for i in range(120))))
        cpu.mem_write(state + 0x20f8, struct.pack('>I', 0 if scenario % 2 else 0x400))
        cpu.reg_write(UC_PPC_REG_1, stack + 0xff00)
        cpu.reg_write(UC_PPC_REG_3, state)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        cpu.emu_start(base + 0x1d04, stop, count=20000)
        original, = struct.unpack('>I', cpu.mem_read(state + 0x20f8, 4))
        if cpu.reg_read(UC_PPC_REG_PC) != stop or original != native:
            raise AssertionError(f'Unlock restoration mismatch for scenario {scenario}')
        reward_cases += 1
    if reward_cases != 128:
        raise AssertionError('Incomplete rewards probe output')
    comparisons += reward_cases
    print(json.dumps(dict(module_sha256=expected, comparisons=comparisons, matched=True)))


if __name__ == '__main__':
    main()

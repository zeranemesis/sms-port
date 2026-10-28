"""Compare native constraints with every stage/episode input of v0.3.6.

Usage: compare_rules.py Randomizer.kxe rules_probe.exe
Requires Unicorn for the test only. The user's original module stays local.
"""
from collections import defaultdict
import json
from pathlib import Path
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'tools/mods'))
from inspect_kxe import inspect
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_LR, UC_PPC_REG_PC, UC_PPC_REG_MSR


def original_code(module, base=0x81000000):
    report = inspect(module, {})
    if report['sha256'] != 'a6033a3efb81a931c18f8bfb421898ba62665f681124faf1326d004079b118e7':
        raise ValueError('Expected the original v0.3.6 module')
    code_offset, code_size = struct.unpack_from('>II', module, 16)
    code = bytearray(module[code_offset:code_offset + code_size])
    reloc_offset, reloc_size = struct.unpack_from('>II', module, 64)
    for pos in range(reloc_offset, reloc_offset + reloc_size, 16):
        kind, affected, source, pad, target, origin, addend = struct.unpack_from('>BBBBIII', module, pos)
        value = ((base + origin + addend) if source == 0 else
                 (origin + addend) if source == 0xfd else 0x81800000 + 4 * origin) & 0xffffffff
        if kind == 1:
            struct.pack_into('>I', code, target, value)
        elif kind == 4:
            struct.pack_into('>H', code, target, value & 0xffff)
        elif kind == 6:
            struct.pack_into('>H', code, target, ((value >> 16) + bool(value & 0x8000)) & 0xffff)
        elif kind == 10:
            old, = struct.unpack_from('>I', code, target)
            struct.pack_into('>I', code, target, (old & ~0x03fffffc) | ((value - base - target) & 0x03fffffc))
        else:
            raise ValueError('Unexpected relocation type')
    return bytes(code)


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    code = original_code(Path(sys.argv[1]).read_bytes())
    native = defaultdict(list)
    result = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    for line in result.stdout.splitlines():
        scene, kind, target, flags, mask = map(int, line.split())
        native[scene].append((kind, target, flags, mask))
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (0x82000000, 0x10000), (state, 0x10000),
                          (0x84000000, 0x100000), (0x80200000, 0x200000)):
        cpu.mem_map(address, size)
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)
    cpu.mem_write(base, bytes(code))
    next_heap = 0x84000000

    def allocation(machine, address, size, user):
        nonlocal next_heap
        if address == 0x802c3ba4:
            length = machine.reg_read(UC_PPC_REG_3)
            pointer = next_heap
            next_heap = (next_heap + length + 31) & ~31
            if next_heap >= 0x84100000:
                raise AssertionError('Original function exhausted the oracle heap')
            machine.reg_write(UC_PPC_REG_3, pointer)
        elif address != 0x802c3da4:
            raise AssertionError(f'Unexpected retail call {address:x}')
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    cpu.hook_add(UC_HOOK_CODE, allocation, begin=0x80200000, end=0x803fffff)
    populated = rules = 0
    for scene in range(65536):
        next_heap = 0x84000000
        data = bytearray(0x34)
        struct.pack_into('>H', data, 0, scene)
        for pos in (0x14, 0x2c):
            struct.pack_into('>I', data, pos, 0x40000000)
        cpu.mem_write(state, bytes(data))
        cpu.reg_write(UC_PPC_REG_1, 0x8200ff00)
        cpu.reg_write(UC_PPC_REG_3, state)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        cpu.emu_start(base + 0x1fd4, stop, count=200000)
        if cpu.reg_read(UC_PPC_REG_PC) != stop:
            raise AssertionError(f'Original function did not return for scene {scene:x}')
        original = []
        for kind, pos in enumerate((8, 0x20)):
            begin, end = struct.unpack('>II', cpu.mem_read(state + pos, 8))
            if begin != end and not (0x84000000 <= begin < end <= next_heap and (end - begin) % 8 == 0):
                raise AssertionError('Invalid original output vector')
            for pointer in range(begin, end, 8):
                target, flags, pad, mask = struct.unpack('>HBBI', cpu.mem_read(pointer, 8))
                original.append((kind, target, flags, mask))
        if original != native.pop(scene, []):
            raise AssertionError(f'Constraint mismatch for scene {scene:x}')
        populated += bool(original)
        rules += len(original)
    if native or populated != 101 or rules != 207:
        raise AssertionError('Incomplete native constraints')
    print(json.dumps(dict(scenes_compared=65536, populated_scenes=populated, rules=rules, matched=True)))


if __name__ == '__main__':
    main()

"""Run the original generator as a local comparison oracle, never in the game.

Usage: original_seed.py Randomizer.kxe Sunshine-US.iso rules_probe.exe SEED
The archive enumeration is supplied as an ordered fixture. All generation,
constraints, vector operations and random draws execute the original PPC code.
"""
import json
from pathlib import Path
import struct
import subprocess
import sys

from compare_rules import original_code
from compare_start import retail_rng
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import (UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4,
                               UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_13,
                               UC_PPC_REG_LR, UC_PPC_REG_PC, UC_PPC_REG_MSR)


def generate(module, iso, scenes, seed, trace=None, start_with_all_unlocks=False):
    if not 0 <= seed <= 0xffffffff:
        raise ValueError('Seed outside uint32 range')
    if len(scenes) != len(set(scenes)) or any(not 0 <= x <= 0xffff for x in scenes):
        raise ValueError('Invalid archive fixture')
    base, state, stop, heap, heap_end = 0x81000000, 0x83000000, 0x8100fff0, 0x85000000, 0x86000000
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (state, 0x10000), (0x82000000, 0x10000),
                          (0x84000000, 0x10000), (heap, heap_end - heap), (0x80000000, 0x500000)):
        cpu.mem_map(address, size)
    cpu.mem_write(base, original_code(Path(module).read_bytes()))
    cpu.mem_write(0x8033b19c, retail_rng(iso))
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)
    cpu.reg_write(UC_PPC_REG_13, 0x80400000)
    cpu.mem_write(state, struct.pack('>I', seed))
    for index in range(128):
        for offset in (0x6f0 + index * 0x34 + 0x14, 0x6f0 + index * 0x34 + 0x2c):
            cpu.mem_write(state + offset, struct.pack('>I', 0x40000000))
    initial = 0x7fffff if start_with_all_unlocks else 0x400
    cpu.mem_write(state + 0x20f4, struct.pack('>II', initial, initial))
    archive_data = struct.pack(f'>{len(scenes)}H', *scenes)
    cpu.mem_write(0x84000000, archive_data)
    symbols = {0x80003100: 'memset', 0x800031f4: 'memcpy', 0x802c3da4: '__dl__FPv',
               0x802c2fb0: 'getGlbResource__13JKRFileLoaderFPCc',
               0x80344644: 'OSReport', 0x803446c4: 'OSPanic'}
    calls = {}

    def retail(machine, address, size, user):
        nonlocal heap
        if 0x8033b19c <= address < 0x8033b1c8:
            if address == 0x8033b1a4 and trace is not None:
                caller = machine.reg_read(UC_PPC_REG_LR) - base
                pools = {0x8fc: (0xdc, 2), 0x1018: (0x1b4, 2),
                         0x1528: (0x74, 4), 0x1550: (0x5c, 8),
                         0x1970: (0xbc, 4), 0x1998: (0x184, 8)}
                offset, stride = pools[caller]
                start, end = struct.unpack('>II', machine.mem_read(machine.reg_read(UC_PPC_REG_1) + offset, 8))
                if not 0 <= end - start <= 0x10000 or (end - start) % stride:
                    raise AssertionError('Invalid original draw pool')
                data = bytes(machine.mem_read(start, end - start))
                values = [data[i] if stride == 8 else int.from_bytes(data[i:i + stride], 'big')
                          for i in range(0, len(data), stride)]
                entry = dict(caller=hex(caller), values=values)
                if caller == 0x1528:
                    stack = machine.reg_read(UC_PPC_REG_1)
                    first, last = struct.unpack('>II', machine.mem_read(stack + 0x13c, 8))
                    entry['locked_exits'] = [struct.unpack('>HBBI', machine.mem_read(pos, 8))
                                             for pos in range(first, last, 8)]
                    entry['unlocks'], = struct.unpack('>I', machine.mem_read(state + 0x20f4, 4))
                    first, last = struct.unpack('>II', machine.mem_read(stack + 0x184, 8))
                    entry['shines'] = [machine.mem_read(pos, 1)[0] for pos in range(first, last, 8)]
                trace.append(entry)
            return
        name = symbols.get(address, f'unknown_{address:x}')
        calls[name] = calls.get(name, 0) + 1
        r3, r4, r5 = (machine.reg_read(reg) for reg in (UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_5))
        if address == 0x802c3ba4:
            pointer = heap
            heap = (heap + max(r3, 1) + 31) & ~31
            if heap > heap_end:
                raise AssertionError('Oracle allocation limit exceeded')
            machine.reg_write(UC_PPC_REG_3, pointer)
        elif name == '__dl__FPv' or name == 'OSReport':
            pass
        elif name == 'memset':
            if r5 > 0x100000:
                raise AssertionError('Invalid memset size')
            machine.mem_write(r3, bytes([r4 & 255]) * r5)
        elif name in ('memcpy', 'memmove'):
            if r5 > 0x100000:
                raise AssertionError('Invalid copy size')
            machine.mem_write(r3, bytes(machine.mem_read(r4, r5)))
        elif name == 'getGlbResource__13JKRFileLoaderFPCc':
            machine.reg_write(UC_PPC_REG_3, 0x84008000)
        elif name == 'OSPanic':
            raise AssertionError(f'Original generator panicked at source line {r4}')
        else:
            raise AssertionError(f'Unexpected original-generator call: {name} ({address:x})')
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    def archives(machine, address, size, user):
        stack = machine.reg_read(UC_PPC_REG_1)
        end = 0x84000000 + len(archive_data)
        machine.mem_write(stack + 0x19c, struct.pack('>III', 0x84000000, end, end))
        machine.reg_write(UC_PPC_REG_PC, base + 0x364)

    cpu.hook_add(UC_HOOK_CODE, retail, begin=0x80000000, end=0x804fffff)
    cpu.hook_add(UC_HOOK_CODE, archives, begin=base + 0x27c, end=base + 0x27c)
    cpu.reg_write(UC_PPC_REG_1, 0x8200ff00)
    cpu.reg_write(UC_PPC_REG_3, state)
    cpu.reg_write(UC_PPC_REG_LR, stop)
    cpu.emu_start(base + 0xcc, stop, count=50000000)
    if cpu.reg_read(UC_PPC_REG_PC) != stop:
        raise AssertionError(f'Original generator did not return: {cpu.reg_read(UC_PPC_REG_PC):x}')
    count, = struct.unpack('>H', cpu.mem_read(state + 0x508, 2))
    if count != 110:
        raise AssertionError(f'Original generated {count} routes, expected 110')
    sources = struct.unpack(f'>{count}H', cpu.mem_read(state + 0x308, count * 2))
    destinations = struct.unpack(f'>{count}H', cpu.mem_read(state + 0x408, count * 2))
    rewards = struct.unpack('>120I', cpu.mem_read(state + 0x50c, 480))
    return dict(seed=seed, scenes=scenes, routes=list(zip(sources, destinations)),
                rewards=list(rewards), retail_calls=calls)


def reference_scenes(probe):
    output = subprocess.run([probe], capture_output=True, text=True, check=True)
    scenes = {0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108, 0x109, 0xf00, 0x3400, 0x3c00}
    for line in output.stdout.splitlines():
        scene, kind, target, flags, mask = map(int, line.split())
        scenes.add(scene)
        if kind == 0:
            scenes.add(target)
    return sorted(scenes)


def main():
    module, iso, probe, seed = sys.argv[1:]
    print(json.dumps(generate(module, iso, reference_scenes(probe), int(seed)), indent=2))


if __name__ == '__main__':
    main()

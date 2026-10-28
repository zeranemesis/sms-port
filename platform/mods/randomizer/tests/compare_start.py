"""Usage: compare_start.py Randomizer.kxe start_probe.exe rules_probe.exe Sunshine-US.iso

Compare starting pools to the original module, and first draws to the retail RNG.
Unicorn and the user's unmodified GMSE01 revision 0 disc are test inputs only.
"""
from collections import defaultdict
import json
from pathlib import Path
import struct
import subprocess
import sys

from compare_rules import original_code
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7, UC_PPC_REG_8, UC_PPC_REG_13, UC_PPC_REG_LR, UC_PPC_REG_PC, UC_PPC_REG_MSR


def retail_rng(path):
    with open(path, 'rb') as disc:
        boot = disc.read(0x440)
        if boot[:6] != b'GMSE01' or boot[7] != 0:
            raise ValueError('Expected an unmodified GMSE01 revision 0 ISO')
        dol, = struct.unpack_from('>I', boot, 0x420)
        disc.seek(dol)
        header = disc.read(0x100)
        offsets = struct.unpack_from('>18I', header, 0)
        addresses = struct.unpack_from('>18I', header, 0x48)
        sizes = struct.unpack_from('>18I', header, 0x90)
        for offset, address, size in zip(offsets, addresses, sizes):
            if address <= 0x8033b19c and 0x8033b1c8 <= address + size:
                disc.seek(dol + offset + 0x8033b19c - address)
                code = disc.read(0x2c)
                if len(code) != 0x2c:
                    raise ValueError('Truncated retail RNG')
                return code
    raise ValueError('Retail RNG not found')


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (0x82000000, 0x10000), (state, 0x10000),
                          (0x84000000, 0x100000), (0x80200000, 0x210000)):
        cpu.mem_map(address, size)
    cpu.mem_write(base, original_code(Path(sys.argv[1]).read_bytes()))
    cpu.mem_write(0x8033b19c, retail_rng(sys.argv[4]))
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)
    cpu.reg_write(UC_PPC_REG_13, 0x80400000)
    next_heap = 0x84000000

    def allocation(machine, address, size, user):
        nonlocal next_heap
        if address == 0x802c3ba4:
            pointer = next_heap
            next_heap = (next_heap + machine.reg_read(UC_PPC_REG_3) + 31) & ~31
            if next_heap >= 0x84100000:
                raise AssertionError('Oracle heap exhausted')
            machine.reg_write(UC_PPC_REG_3, pointer)
        elif address != 0x802c3da4:
            raise AssertionError('Unexpected retail call')
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    for address in (0x802c3ba4, 0x802c3da4):
        cpu.hook_add(UC_HOOK_CODE, allocation, begin=address, end=address)

    def run(function, arguments):
        cpu.reg_write(UC_PPC_REG_1, 0x8200ff00)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        for reg, value in arguments:
            cpu.reg_write(reg, value)
        cpu.emu_start(function, stop, count=1000000)
        if cpu.reg_read(UC_PPC_REG_PC) != stop:
            raise AssertionError('Original function did not return')
        return cpu.reg_read(UC_PPC_REG_3)

    fixtures = defaultdict(lambda: [[], []])
    probe = subprocess.run([sys.argv[3]], capture_output=True, text=True, check=True)
    for line in probe.stdout.splitlines():
        scene, kind, target, flags, mask = map(int, line.split())
        fixtures[scene][kind].append(struct.pack('>HBBI', target, flags, 0, mask))
    pointer = state + 0xa000
    for index, (scene, vectors) in enumerate(sorted(fixtures.items())):
        cpu.mem_write(state + 4 + index * 2, struct.pack('>H', scene))
        cpu.mem_write(state + 0x104 + index * 4, struct.pack('>I', index))
        record = bytearray(0x34)
        struct.pack_into('>H', record, 0, scene)
        record[2] = int(scene in (0x1400, 0x3400))
        for kind, pos in enumerate((4, 0x1c)):
            data = b''.join(vectors[kind])
            if data:
                cpu.mem_write(pointer, data)
            struct.pack_into('>6I', record, pos, 0, pointer, pointer + len(data), pointer + len(data), 0x40000000, 0)
            pointer += len(data)
        cpu.mem_write(state + 0x6f0 + index * 0x34, bytes(record))
    cpu.mem_write(state + 0x304, struct.pack('>I', len(fixtures)))
    cpu.mem_write(state + 0x20f0, struct.pack('>I', len(fixtures)))
    fixed = (0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108, 0x109, 0x3400, 0x3c00)
    cpu.mem_write(state + 0x4000, struct.pack('>10H', *fixed))
    cpu.mem_write(state + 0x3000, struct.pack('>6I', 0, 0, 0, 0, 0x40000000, 0))
    cpu.mem_write(state + 0x3100, struct.pack('>6I', 0, state + 0x4000, state + 0x4014, state + 0x4014, 0x40000000, 0))
    pools = {}
    probe = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    seed_cases = 0
    for line in probe.stdout.splitlines():
        tag, *values = line.split()
        unlocks, *values = map(int, values)
        if tag == 'pool':
            next_heap = 0x84000000
            cpu.mem_write(state + 0x20f4, struct.pack('>I', unlocks))
            output = state + 0x5000
            run(base + 0x3960, ((UC_PPC_REG_3, output), (UC_PPC_REG_4, state),
                (UC_PPC_REG_5, state + 0x3000), (UC_PPC_REG_6, state + 0x3100),
                (UC_PPC_REG_7, 1), (UC_PPC_REG_8, 0)))
            begin, end = struct.unpack('>II', cpu.mem_read(output + 4, 8))
            if begin != end and not (0x84000000 <= begin < end <= next_heap and (end - begin) % 2 == 0):
                raise AssertionError('Invalid starting pool')
            original = list(struct.unpack(f'>{(end - begin) // 2}H', cpu.mem_read(begin, end - begin))) if begin != end else []
            if original != values:
                raise AssertionError(f'Starting pool mismatch for unlocks {unlocks:x}: native={values}, original={original}')
            pools[unlocks] = original
        elif tag == 'seed':
            seed, native = values
            run(0x8033b19c, ((UC_PPC_REG_3, seed),))
            draw = run(0x8033b1a4, ())
            pool = pools[unlocks]
            original = pool[draw % len(pool)] if pool else -1
            if original != native:
                raise AssertionError(f'First draw mismatch for seed {seed:x}')
            seed_cases += 1
        else:
            raise ValueError('Unexpected probe output')
    if len(pools) != 3 or seed_cases != 768:
        raise AssertionError('Incomplete starting-destination comparison')
    print(json.dumps(dict(pools=3, seed_cases=seed_cases, matched=True)))


if __name__ == '__main__':
    main()

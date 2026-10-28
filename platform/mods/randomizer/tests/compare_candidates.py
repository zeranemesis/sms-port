"""Usage: compare_candidates.py Randomizer.kxe candidates_probe.exe eligibility_probe.exe rules_probe.exe

Runs the original ability-placement candidate selection with an isolated heap.
Requires Unicorn for testing only.
"""
import json
from pathlib import Path
from collections import defaultdict
import struct
import subprocess
import sys

from compare_rules import original_code
from unicorn import Uc, UC_ARCH_PPC, UC_MODE_32, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
from unicorn.ppc_const import UC_PPC_REG_1, UC_PPC_REG_3, UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6, UC_PPC_REG_7, UC_PPC_REG_8, UC_PPC_REG_9, UC_PPC_REG_LR, UC_PPC_REG_PC, UC_PPC_REG_MSR


def main():
    base, state, stop = 0x81000000, 0x83000000, 0x8100fff0
    code = original_code(Path(sys.argv[1]).read_bytes())
    cpu = Uc(UC_ARCH_PPC, UC_MODE_32 | UC_MODE_BIG_ENDIAN)
    for address, size in ((base, 0x10000), (0x82000000, 0x10000), (state, 0x10000),
                          (0x84000000, 0x100000), (0x80200000, 0x200000)):
        cpu.mem_map(address, size)
    cpu.mem_write(base, code)
    cpu.reg_write(UC_PPC_REG_MSR, cpu.reg_read(UC_PPC_REG_MSR) | 0x2000)
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
            raise AssertionError(f'Unexpected retail call {address:x}')
        machine.reg_write(UC_PPC_REG_PC, machine.reg_read(UC_PPC_REG_LR))

    cpu.hook_add(UC_HOOK_CODE, allocation, begin=0x80200000, end=0x803fffff)
    vector, data, output = state + 0x3000, state + 0x4000, state + 0x5000
    cpu.mem_write(vector, struct.pack('>6I', 0, data, data + 56, data + 56, 0x40000000, 0))
    for i, shine in enumerate((6, 16, 26, 36, 46, 56, 66)):
        cpu.mem_write(data + i * 8, bytes([shine]) + bytes(7))
    probe = subprocess.run([sys.argv[2]], capture_output=True, text=True, check=True)
    count = 0
    for line in probe.stdout.splitlines():
        required, unlocks, *native = map(int, line.split())
        next_heap = 0x84000000
        cpu.mem_write(state + 0x20f4, struct.pack('>I', unlocks))
        cpu.reg_write(UC_PPC_REG_1, 0x8200ff00)
        cpu.reg_write(UC_PPC_REG_3, output)
        cpu.reg_write(UC_PPC_REG_4, state)
        cpu.reg_write(UC_PPC_REG_5, vector)
        cpu.reg_write(UC_PPC_REG_6, required)
        cpu.reg_write(UC_PPC_REG_LR, stop)
        cpu.emu_start(base + 0x3f48, stop, count=50000)
        if cpu.reg_read(UC_PPC_REG_PC) != stop:
            raise AssertionError('Original candidate selection did not return')
        begin, end = struct.unpack('>II', cpu.mem_read(output + 4, 8))
        if begin != end and not (0x84000000 <= begin < end <= next_heap and (end - begin) % 4 == 0):
            raise AssertionError('Invalid output vector')
        original = list(struct.unpack(f'>{(end - begin) // 4}I', cpu.mem_read(begin, end - begin))) if begin != end else []
        if original != native:
            raise AssertionError(f'Candidate mismatch: required={required:x}, unlocks={unlocks:x}, native={native}, original={original}')
        count += 1
    if count != 8264:
        raise AssertionError(f'Incomplete candidate probe: {count}')
    rules = defaultdict(lambda: [[], []])
    fixture = subprocess.run([sys.argv[4]], capture_output=True, text=True, check=True)
    for line in fixture.stdout.splitlines():
        scene, kind, target, flags, mask = map(int, line.split())
        if kind not in (0, 1):
            raise ValueError('Unexpected constraint kind')
        rules[scene][kind].append(struct.pack('>HBBI', target, flags, 0, mask))
    eligibility = subprocess.run([sys.argv[3]], capture_output=True, text=True, check=True)
    level, output = state + 0x6000, state + 0x7000
    eligibility_count = 0
    for line in eligibility.stdout.splitlines():
        scene, mask, shines, flags, native = map(int, line.split())
        next_heap = 0x84000000
        record = bytearray(0x34)
        struct.pack_into('>H', record, 0, scene)
        for kind, start in enumerate((4, 0x1c)):
            pointer = state + 0x8000 + kind * 0x1000
            entries = b''.join(rules[scene][kind])
            if entries:
                cpu.mem_write(pointer, entries)
            struct.pack_into('>6I', record, start, 0, pointer, pointer + len(entries), pointer + len(entries), 0x40000000, 0)
        cpu.mem_write(level, bytes(record))
        cpu.mem_write(output, struct.pack('>6I', 0, 0, 0, 0, 0x40000000, 0))
        for reg, value in ((UC_PPC_REG_1, 0x8200ff00), (UC_PPC_REG_4, level),
                           (UC_PPC_REG_5, mask), (UC_PPC_REG_6, shines),
                           (UC_PPC_REG_7, output), (UC_PPC_REG_8, flags & 1),
                           (UC_PPC_REG_9, (flags >> 1) & 1), (UC_PPC_REG_LR, stop)):
            cpu.reg_write(reg, value)
        cpu.emu_start(base + 0x450c, stop, count=10000)
        original = cpu.reg_read(UC_PPC_REG_3)
        if cpu.reg_read(UC_PPC_REG_PC) != stop or original != native:
            raise AssertionError(f'Eligibility mismatch: scene={scene:x}, mask={mask:x}, shines={shines}, flags={flags}, native={native}, original={original}')
        eligibility_count += 1
    if eligibility_count != 9792:
        raise AssertionError(f'Incomplete eligibility probe: {eligibility_count}')
    print(json.dumps(dict(candidate_cases=count, eligibility_cases=eligibility_count, matched=True)))


if __name__ == '__main__':
    main()

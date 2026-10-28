#!/usr/bin/env python3
"""Inventory a Kuribo KXER v0 module before adapting its PowerPC code.

Usage: inspect_kxe.py MODULE.kxe RETAIL.map
The imports section also contains strings: only relocation-referenced entries
are descriptors. No game code or resources are written by this command.
Format: DotKuribo/Kuribo, source/LibKuribo/modules/kxer/Binary.hxx and Loader.cxx.
"""
import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path

from gen_rawfn import retail_map


def inspect(data, symbols):
    def unpack(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise ValueError('Truncated KXER structure')
        return struct.unpack_from(fmt, data, offset)

    magic, version, kernel, declared_size, flags = unpack('>4sHHII', 0)
    if magic != b'KXER' or version != 0:
        raise ValueError('Expected KXER version 0')
    if declared_size not in (0, len(data)):
        raise ValueError('KXER file size mismatch')
    sections = {}
    for index, name in enumerate(('code', 'data', 'bss', 'relocations', 'imports', 'exports')):
        offset, size, crc, alignment, compressed, reserved = unpack('>IIIBBH', 16 + index * 16)
        if offset == 0xffffffff and size == 0xffffffff:
            continue
        if compressed:
            raise ValueError('Compressed KXER sections require decompression first')
        if offset + size > len(data):
            raise ValueError('KXER section outside file')
        sections[name] = (offset, size)
    if 'code' not in sections or 'relocations' not in sections:
        raise ValueError('Missing code or relocations')
    code_start, code_size = sections['code']
    entry, = unpack('>I', 112)
    if entry >= code_size:
        raise ValueError('Entry point outside code')
    offset, size = sections['relocations']
    if size % 16:
        raise ValueError('Truncated relocation table')
    calls, imports, kinds = Counter(), {}, Counter()
    for pos in range(offset, offset + size, 16):
        kind, affected, source, pad, target, origin, addend = unpack('>BBBBIII', pos)
        width = 2 if kind in (3, 4, 5, 6) else 4
        if affected != 0 or target + width > code_size:
            raise ValueError('Relocation target outside code')
        kinds[str(kind)] += 1
        if source == 0xff:
            if 'imports' not in sections:
                raise ValueError('Missing imports section')
            start, length = sections['imports']
            if origin * 12 + 12 > length:
                raise ValueError('Import descriptor outside section')
            text_offset, text_length, crc = unpack('>III', start + origin * 12)
            if not start <= text_offset <= text_offset + text_length <= start + length:
                raise ValueError('Import name outside section')
            imports[str(origin)] = data[text_offset:text_offset + text_length].decode('ascii').rstrip('\0')
        elif source == 0xfd:
            if kind == 10:
                calls[(origin + addend) & 0xffffffff] += 1
        elif source != 0 or origin + addend >= code_size:
            raise ValueError('Unsupported relocation source')
    return {
        'sha256': hashlib.sha256(data).hexdigest(),
        'code_bytes': code_size,
        'entry_offset': entry,
        'relocations': dict(kinds),
        'imports': imports,
        'retail_calls': [dict(address=f'0x{addr:08x}', symbol=symbols.get(addr), sites=count)
                         for addr, count in sorted(calls.items())],
    }


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    try:
        print(json.dumps(inspect(Path(sys.argv[1]).read_bytes(), retail_map(sys.argv[2])), indent=2))
    except (ValueError, OSError, struct.error) as error:
        sys.exit(str(error))

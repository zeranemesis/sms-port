import struct
import re

def decompress_yaz0(data):
    if data[:4] != b'Yaz0':
        return data
    uncompressed_size = struct.unpack('>I', data[4:8])[0]
    src, dst, valid_bits, curr_code_byte = 16, bytearray(), 0, 0
    while len(dst) < uncompressed_size:
        if valid_bits == 0:
            curr_code_byte, src, valid_bits = data[src], src + 1, 8
        if curr_code_byte & 0x80:
            dst.append(data[src])
            src += 1
        else:
            b1, b2 = data[src], data[src+1]
            src += 2
            dist = ((b1 & 0xF) << 8) | b2
            num_bytes = b1 >> 4
            if num_bytes == 0:
                num_bytes = data[src] + 0x12
                src += 1
            else:
                num_bytes += 2
            copy_src = len(dst) - (dist + 1)
            for _ in range(num_bytes):
                dst.append(dst[copy_src])
                copy_src += 1
        curr_code_byte = (curr_code_byte << 1) & 0xFF
        valid_bits -= 1
    return bytes(dst)

with open('rom/GMSP01-extracted/files/data/scene/monte2.szs', 'rb') as f:
    dec = decompress_yaz0(f.read())

print('Dec size:', len(dec))
# Parse RARC to find file entries
# RARC format:
# 0x00: 'RARC'
# 0x04: file size
# 0x08: header size (0x20)
# 0x0C: data offset relative to header (usually 0x20 + offset)
# 0x20: Node count
# 0x28: File entry offset
# 0x2C: File entry count
# 0x30: String table offset
pos = 0x62e0
while True:
    idx = dec.find(b'J3D2bmd3', pos)
    if idx == -1:
        break
    size = struct.unpack('>I', dec[idx+8:idx+12])[0]
    bmd = dec[idx:idx+size]
    # Check if this BMD has MAT3
    sec_count = struct.unpack('>I', bmd[12:16])[0]
    spos = 32
    mat_sec = None
    tex_sec = None
    for s in range(sec_count):
        stag = bmd[spos:spos+4]
        slen = struct.unpack('>I', bmd[spos+4:spos+8])[0]
        if stag == b'MAT3': mat_sec = bmd[spos:spos+slen]
        if stag == b'TEX1': tex_sec = bmd[spos:spos+slen]
        spos += slen
    
    # Check texture names in TEX1
    tex_names = []
    if tex_sec:
        # TEX1 string table
        str_tab_off = struct.unpack('>I', tex_sec[24:28])[0]
        str_tab = tex_sec[str_tab_off:]
        tex_names = re.findall(rb'[a-zA-Z0-9_\-\.]{3,}', str_tab)
        tex_names = [t.decode('latin1') for t in tex_names]
    
    # Filter for interesting BMDs
    is_interesting = any('pollut' in t.lower() or 'puddle' in t.lower() or 'ind' in t.lower() or 'fire' in t.lower() for t in tex_names)
    if is_interesting or b'pollution' in bmd:
        print(f'BMD at {idx:#x}, size={size}, textures={tex_names}')
        if mat_sec:
            mat_count = struct.unpack('>H', mat_sec[8:10])[0]
            print(f'  Material count: {mat_count}')
            # Parse indirect stages in MAT3!
            # J3D Material MAT3 layout:
            # 8: mat_count, 12: mat_entries, 16: mat_remap, 20: string_table
            # offsets at 24+:
            # 24: indInitData, 28: indTexOrder, 32: indTexFormat, 36: indTexMtx, 40: indTexCoordScale, 44: indTexBias
            ind_init_off = struct.unpack('>I', mat_sec[24:28])[0]
            ind_order_off = struct.unpack('>I', mat_sec[28:32])[0]
            ind_format_off = struct.unpack('>I', mat_sec[32:36])[0]
            ind_mtx_off = struct.unpack('>I', mat_sec[36:40])[0]
            print(f'  Indirect offsets: init={ind_init_off:#x}, order={ind_order_off:#x}, mtx={ind_mtx_off:#x}')
    pos = idx + size




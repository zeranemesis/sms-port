"""Parse the 0x10CC byte `tmp_data` blob out of marioEU.o for ModelWaterManager.

TModelWaterManager::drawShineShadowVolume takes the address of this object, uses
the first 0x760 bytes as a GX display list (GXCallDisplayList(..., 0x760)) and
the remaining bytes as the GX_VA_POS vertex array (GXSetArray(..., 6)), i.e.
3 s16 per vertex.  This script confirms that reading.
"""

import re
import subprocess

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"
TARGET = "build/GMSP01/obj/Player/ModelWaterManager.o"

OUT = "artifacts/tmp_data.bin"


def dump():
    out = subprocess.run([OBJDUMP, "-s", "-j", ".data", TARGET],
                         capture_output=True, text=True).stdout
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]{4,8})\s((?:[0-9a-f]{8}\s?){1,4})", line)
        if not m:
            continue
        for word in re.findall(r"[0-9a-f]{8}", m.group(2)):
            data += bytes.fromhex(word)
    open(OUT, "wb").write(bytes(data[0xc0:0xc0 + 0x10cc]))
    return bytes(data[0xc0:0xc0 + 0x10cc])


def parse_list(buf):
    """Walk a GX display list: opcode byte, then opcode specific payload."""
    i = 0
    cmds = []
    draws = 0
    while i < len(buf):
        op = buf[i]
        if 0x80 <= op <= 0x87:      # GX_DRAW_QUADS
            n = (buf[i + 1] << 8) | buf[i + 2]
            cmds.append(("QUADS", n, i))
            i += 3 + 2 * n
            draws += 1
        elif 0x90 <= op <= 0x97:    # GX_DRAW_TRIANGLES
            n = (buf[i + 1] << 8) | buf[i + 2]
            cmds.append(("TRIS", n, i))
            i += 3 + 2 * n
            draws += 1
        elif 0x98 <= op <= 0x9F:    # GX_DRAW_TRIANGLE_STRIP
            n = (buf[i + 1] << 8) | buf[i + 2]
            cmds.append(("STRIP", n, i))
            i += 3 + 2 * n
            draws += 1
        elif op == 0xA0:            # GX_DRAW_TRIANGLE_FAN
            n = (buf[i + 1] << 8) | buf[i + 2]
            cmds.append(("FAN", n, i))
            i += 3 + 2 * n
            draws += 1
        elif op in (0x61,):         # GX_LOAD_BP_REG + 1 param
            i += 5
            cmds.append(("BP", None, i))
        elif op in (0x50,):         # GX_LOAD_CP_REG + 1 param
            i += 5
            cmds.append(("CP", None, i))
        elif op == 0x10:            # GX_LOAD_XF_REG + n + data
            n = (buf[i + 1] << 24) | (buf[i + 2] << 16) | (buf[i + 3] << 8) | buf[i + 4]
            i += 5 + 4 * n
            cmds.append(("XF", n, i))
        elif op == 0x00:            # padding
            cmds.append(("PAD", None, i))
            i += 1
        else:
            return cmds, i, "unknown opcode 0x%02x at 0x%x" % (op, i)
    return cmds, i, None


def main():
    blob = dump()
    dl, end, err = parse_list(blob[:0x760])
    print("display list: %d bytes, parsed to 0x%x of 0x760" % (len(dl), end))
    if err:
        print("  STOP:", err)
    for kind, n, off in dl[:12]:
        print("  +0x%04x %-5s %s" % (off, kind, n if n is not None else ""))
    print("  ... %d commands total, %d draw calls" % (
        len(dl), sum(1 for k, _, _ in dl if k in
                     ("QUADS", "TRIS", "STRIP", "FAN"))))

    verts = blob[0x760:]
    print("\nvertex data: %d bytes -> %d verts of 6 bytes" % (
        len(verts), len(verts) // 6))
    for i in range(6):
        x, y, z = (int.from_bytes(verts[i * 6 + k * 2:i * 6 + k * 2 + 2], "big",
                                  signed=True) for k in range(3))
        print("  v%-3d %6d %6d %6d" % (i, x, y, z))
    maxc = max(abs(int.from_bytes(verts[i:i + 2], "big", signed=True))
               for i in range(0, len(verts) - 1, 2))
    print("  max component magnitude: %d (0x%04x)" % (maxc, maxc & 0xFFFF))


if __name__ == "__main__":
    main()

"""Rewrite ModelWaterManager.cpp's `tmp_data` initializer with the real bytes.

`tmp_data` is a 0x10CC byte object that lives in .data of marioEU.o, i.e. it is
pre-initialised: a 0x760 byte GX display list (40 GX_DRAW_TRIANGLE_STRIP
commands) followed by 402 s16[3] vertices of a radius-32767 sphere.  The old
source had `static u8 tmp_data;` - one byte - so none of this was present.

The bytes are taken verbatim from build/GMSP01/obj/Player/ModelWaterManager.o
and re-emitted as a u8 initializer; the region comments keep the two halves
recognisable to a human reader.
"""

import re
import subprocess

OBJDUMP = "build/binutils/powerpc-eabi-objdump.exe"
TARGET = "build/GMSP01/obj/Player/ModelWaterManager.o"
SRC = "src/Player/ModelWaterManager.cpp"

DL_SIZE = 0x760
TOTAL = 0x10CC

HEADER = """\
// The sphere that drawShineShadowVolume renders. marioEU.o has this as a
// pre-built .data object (symbols.txt: size 0x10CC align 32), not a scratch
// buffer: the first 0x760 bytes are a GX display list handed straight to
// GXCallDisplayList below, and the remaining 402 * 6 bytes are the s16[3]
// vertex array handed to GXSetArray. 40 triangle strips, radius 32767.
static u8 tmp_data[4300] __attribute__((aligned(32))) = {
"""

REGION_COMMENTS = {
    DL_SIZE: "\t// --- sphere vertices: 402 x s16[3], radius 32767 ---\n",
}


def target_data():
    out = subprocess.run([OBJDUMP, "-s", "-j", ".data", TARGET],
                         capture_output=True, text=True).stdout
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"\s*([0-9a-f]{4,8})\s((?:[0-9a-f]{8}\s?){1,4})", line)
        if not m:
            continue
        for word in re.findall(r"[0-9a-f]{8}", m.group(2)):
            data += bytes.fromhex(word)
    blob = bytes(data[0xc0:0xc0 + TOTAL])
    if len(blob) != TOTAL:
        raise SystemExit("short read: %d" % len(blob))
    return blob


def emit(blob):
    # start with the display list, note the boundary, then the vertices
    lines = [HEADER]
    lines.append("\t// --- GX display list, 40 GX_DRAW_TRIANGLE_STRIP commands ---\n")
    for off in range(0, TOTAL, 12):
        if off in REGION_COMMENTS:
            lines.append(REGION_COMMENTS[off])
        chunk = blob[off:off + 12]
        body = ", ".join("0x%02x" % b for b in chunk)
        lines.append("\t%s,\n" % body)
    lines.append("};\n")
    return "".join(lines)


def splice(text, blob):
    pat = re.compile(
        r"static u8 tmp_data\[4300\] __attribute__\(\(aligned\(32\)\)\) = \{ 0 \};")
    if not pat.search(text):
        raise SystemExit("initializer line not found")
    return pat.sub(emit(blob), text, count=1)


def main():
    blob = target_data()
    with open(SRC, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    with open(SRC, "w", encoding="utf-8", newline="") as f:
        f.write(splice(text, blob))
    print("wrote %d bytes of initializer to %s" % (TOTAL, SRC))


if __name__ == "__main__":
    main()

// Exercises the real GX texture API and FIFO without creating a GPU surface.
#include "gx_internal.h"
#include "sms_gx/gx_pc.h"
#include "gx_static_memory.h"
#include <dolphin/gx.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned char image[64], vertices[64];
static unsigned char* memory;
static size_t used = 256;
static void* allocate(size_t bytes, size_t alignment, void*) {
    used = (used + alignment - 1) & ~(alignment - 1);
    if (bytes > 65536 - used) return nullptr;
    void* result = memory + used;
    used += bytes;
    return result;
}
extern "C" void port_ptr32_trap(const void*) { std::abort(); }
// This fixture never presents or starts the VI timer. Make an accidental
// presentation fail instead of pretending to supply a working video clock.
extern "C" uint32_t VIGetRetraceCount(void) {
    std::fprintf(stderr, "FAIL GX integration: unexpected presentation/VI clock\n");
    std::abort();
}
static void check(bool condition, const char* reason) {
    if (!condition) { std::fprintf(stderr, "FAIL GX integration: %s\n", reason); std::exit(1); }
}
int sms_gx_static_integration_test() {
    void* desired = reinterpret_cast<void*>(uintptr_t(0x80000000u));
    void* mapped = mmap(desired, 1 << 20, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check(mapped == desired, "non-destructive MEM1 placement");
    memory = static_cast<unsigned char*>(mapped);
    check(reinterpret_cast<uintptr_t>(image) > UINT32_MAX, "real high source pointer");
    for (unsigned i = 0; i < sizeof image; ++i) image[i] = uint8_t(i * 3 + 1);
    for (unsigned i = 0; i < sizeof vertices; ++i) vertices[i] = uint8_t(i * 5 + 7);
    check(port_gx_static_configure(memory, 1 << 20, allocate, nullptr), "configure");
    const void* stagedImage = port_gx_static_register(image, sizeof image);
    const void* stagedVertices = port_gx_static_register(vertices, sizeof vertices);
    check(stagedImage && stagedVertices, "register immutable resources");
    GXPC_SetMemoryWindow(memory, 1 << 20);
    GXPC_AddBigEndianRange(stagedVertices, sizeof vertices);
    GXTexObj object;
    GXInitTexObj(&object, image, 4, 4, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, 0);
    check(GXGetTexObjData(&object) == image, "getter preserves original identity");
    GXLoadTexObj(&object, GX_TEXMAP0);
    check(gx::g.texImage[0] == stagedImage, "immediate texture uses MEM1 copy");
    GXInitTexObjData(&object, const_cast<void*>(stagedImage));
    check(GXGetTexObjData(&object) == image, "data setter and reverse lookup");
    void* list = memory + 0x10000;
    GXBeginDisplayList(list, 4096);
    GXLoadTexObj(&object, GX_TEXMAP0);
    const auto* array = static_cast<const unsigned char*>(stagedVertices) + 8;
    GXSetArray(GX_VA_POS, array, 6);
    const u32 length = GXEndDisplayList();
    check(length > 0 && length <= 4096, "record display list");
    gx::g.texImage[0] = nullptr;
    gx::g.arrayBase[0] = nullptr;
    GXCallDisplayList(list, length);
    check(gx::g.texImage[0] == stagedImage, "FIFO replay texture address");
    check(gx::g.arrayBase[0] == array, "FIFO replay array offset");
    check(gx::g.arrayBigEndian[0], "FIFO replay preserves BE range");
    check(std::memcmp(stagedImage, image, sizeof image) == 0, "texture bytes preserved");
    check(std::memcmp(stagedVertices, vertices, sizeof vertices) == 0, "array bytes preserved");
    std::printf("PASS real GX API/FIFO: high source, getters, immediate texture, record/replay texture+array, BE, bytes (%u DL bytes)\n", length);
    return 0;
}
#ifndef SMS_GX_INTEGRATION_NO_MAIN
int main() { return sms_gx_static_integration_test(); }
#endif

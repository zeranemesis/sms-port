"""Adapt known immutable GX blobs in the PC build mirror, never console files."""
from pathlib import Path
import difflib
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'artifacts/android-aurora/gx-static-patch/base'
OUT = ROOT / 'artifacts/android-aurora/gx-static-patch'
paths = ['src/System/Application.cpp', 'src/MarioUtil/gd-reinit-gx.cpp',
         'src/Map/BathWaterManager.cpp', 'src/Player/ModelWaterManager.cpp']
patches = []
for name in paths:
    source = BASE / name
    if not source.exists(): source = ROOT / 'decomp' / name
    before = source.read_bytes().decode('latin1').replace('\r\n', '\n')
    assert 'port_gx_static_' not in before, 'Use the mirror before this patch: ' + name
    include = '''#if defined(TARGET_PC) && defined(__ANDROID__)
#include "gx_static_memory.h"
#include <stdlib.h>
#endif
'''
    after = include + before
    if name.endswith('Application.cpp'):
        helper = '''#if defined(TARGET_PC) && defined(__ANDROID__)
extern u8* port_mem1_base;
extern u32 port_mem1_size;
extern u8 NullTexData[32];
struct MobileStaticPool { u8* base; size_t used; };
static MobileStaticPool mobileStaticPool;
static void* mobileStaticAllocate(size_t bytes, size_t alignment, void* context) {
    MobileStaticPool* pool = (MobileStaticPool*)context;
    size_t previous = __atomic_load_n(&pool->used, __ATOMIC_RELAXED);
    for (;;) {
        size_t offset = (previous + alignment - 1) & ~(alignment - 1);
        if (offset > 0x2000 || bytes > 0x2000 - offset) return 0;
        if (__atomic_compare_exchange_n(&pool->used, &previous, offset + bytes,
                                        false, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
            return pool->base + offset;
    }
}
#endif

'''
        marker = 'void TApplication::initialize()'
        assert after.count(marker) == 1
        after = after.replace(marker, helper + marker)
        marker = '\tJKRGetRootHeap()->becomeCurrentHeap();'
        setup = '''
#if defined(TARGET_PC) && defined(__ANDROID__)
    // The later scene solid heap consumes the root's remaining free block.
    // Reserve this bounded persistent arena before that happens; never stage
    // immutable FIFO resources in a scene heap that will be freed.
    mobileStaticPool.base = (u8*)JKRHeap::alloc(0x2000, 0x20, JKRGetRootHeap());
    mobileStaticPool.used = 0;
    if (!mobileStaticPool.base) abort();
    if (!port_gx_static_configure(port_mem1_base, port_mem1_size,
                                  mobileStaticAllocate, &mobileStaticPool) ||
        !port_gx_static_register(NullTexData, sizeof(NullTexData))) abort();
#endif'''
        after = after.replace(marker, marker + setup, 1)
    elif name.endswith('gd-reinit-gx.cpp'):
        marker = '\tGXInitTexObj(&obj, &NullTexData,'
        assert after.count(marker) == 1
        after = after.replace(marker, '''#if defined(TARGET_PC) && defined(__ANDROID__)
    if (!port_gx_static_register(NullTexData, sizeof(NullTexData))) abort();
#endif
''' + marker)
    elif name.endswith('BathWaterManager.cpp'):
        marker = '\t\tGXInitTexObj(&unk80114, clear_z_TX,'
        assert after.count(marker) == 1
        after = after.replace(marker, '''#if defined(TARGET_PC) && defined(__ANDROID__)
        if (!port_gx_static_register(clear_z_TX, sizeof(clear_z_TX))) abort();
#endif
''' + marker)
    else:
        marker = 'sphere_glist_p = tmp_data;'
        assert after.count(marker) == 2
        after = after.replace(marker, '''sphere_glist_p = tmp_data;
#if defined(TARGET_PC) && defined(__ANDROID__)
    sphere_glist_p = const_cast<void*>(port_gx_static_register(tmp_data, sizeof(tmp_data)));
    if (!sphere_glist_p) abort();
    GXPC_AddBigEndianRange(sphere_glist_p, sizeof(tmp_data));
#endif''')
        marker = 'GXPC_AddBigEndianRange(tmp_data, sizeof(tmp_data));'
        assert after.count(marker) == 1
        after = after.replace(marker, 'GXPC_AddBigEndianRange(sphere_glist_p, sizeof(tmp_data));')
    for folder, content in [('base', before), ('patched', after)]:
        target = OUT / folder / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content.encode('latin1'))
    patches.append(''.join(difflib.unified_diff(before.splitlines(True), after.splitlines(True),
                                              fromfile='a/' + name, tofile='b/' + name)))
target = ROOT / 'decomp-patches/zzzzz-mobile-gx-static-memory.patch'
target.write_bytes(''.join(patches).encode('latin1'))
print(target)
print(hashlib.sha256(target.read_bytes()).hexdigest())

#ifndef SMS_MOBILE_GX_STATIC_MEMORY_H
#define SMS_MOBILE_GX_STATIC_MEMORY_H

// Explicit immutable GX resources only. C++03-compatible platform interface.
#include <stddef.h>

typedef void* (*port_gx_static_alloc_fn)(size_t bytes, size_t alignment, void* context);

// Configure once after the root heap exists. MEM1 must start at 0x80000000;
// allocations must fit that window, be 32-byte aligned and avoid physical 0.
// An identical repeated configuration is allowed; replacing it is rejected.
bool port_gx_static_configure(void* mem1, size_t mem1_bytes,
                              port_gx_static_alloc_fn allocate, void* context);

// Register valid, immutable source bytes. One stable MEM1 copy per exact
// source range; other overlapping registrations are rejected. Existing MEM1
// resources pass through without allocation/copy. NULL indicates failure.
// The allocator is called outside the metadata lock; an in-flight overlapping
// registration fails rather than waiting/reentering the allocator.
const void* port_gx_static_register(const void* source, size_t bytes);

// Resolve a registered source OR its MEM1 copy, including contained subranges.
// Unregistered MEM1 resources pass through; unknown outside-MEM1 ranges fail.
// Partial overlaps, null/empty/overflowing ranges and pending copies fail.
const void* port_gx_static_resolve(const void* pointer, size_t bytes);

// Reverse a resolved copy to the original identity, preserving an offset.
// Unstaged MEM1 resources pass through. Useful for GXGetTexObjData/All.
const void* port_gx_static_original(const void* pointer, size_t bytes);

// Source and allocator/context lifetimes must cover the whole game session.
// There is no reset/free: recorded GX commands may retain a copy's address.
// Do not register mutable images, palettes, EFB destinations or opaque user data.
#endif

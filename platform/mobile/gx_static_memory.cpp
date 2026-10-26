#include "gx_static_memory.h"

#include <stdint.h>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>
#include <cstdio>
#if defined(__ANDROID__) && defined(SMS_ANDROID_APP)
#include <android/log.h>
#endif

namespace {
struct Range {
    uintptr_t begin;
    uintptr_t end;
};

bool make_range(const void* pointer, size_t bytes, Range& range)
{
    const uintptr_t begin = reinterpret_cast<uintptr_t>(pointer);
    if (!pointer || !bytes || bytes > UINTPTR_MAX - begin)
        return false;
    range.begin = begin;
    range.end = begin + bytes;
    return true;
}
bool contains(const Range& outer, const Range& inner)
{
    return inner.begin >= outer.begin && inner.end <= outer.end;
}
bool overlaps(const Range& a, const Range& b)
{
    return a.begin < b.end && b.begin < a.end;
}

struct Entry {
    Range source;
    Range copy;
    bool ready;
    Entry(const Range& src) : source(src), copy{0, 0}, ready(false) {}
};
struct Registry {
    std::mutex mutex;
    bool configured = false;
    Range mem1{0, 0};
    port_gx_static_alloc_fn allocate = NULL;
    void* context = NULL;
    std::vector<std::shared_ptr<Entry> > entries;
};
Registry& registry()
{
    static Registry* state = new Registry;
    return *state;
}

// Called with the metadata lock. The first cache block is excluded because the
// FIFO treats physical address zero as null (and texture addresses lose 5 bits).
bool usable_mem1(const Registry& state, const Range& range)
{
    return contains(state.mem1, range) && range.begin >= state.mem1.begin + 32;
}
const void* resolve_locked(const Registry& state, const Range& range, bool original)
{
    for (const auto& entry : state.entries) {
        if (contains(entry->source, range)) {
            if (!entry->ready)
                return NULL;
            return reinterpret_cast<const void*>(original ? range.begin :
                entry->copy.begin + (range.begin - entry->source.begin));
        }
        if (entry->copy.begin && contains(entry->copy, range)) {
            if (!entry->ready)
                return NULL;
            return reinterpret_cast<const void*>(original ?
                entry->source.begin + (range.begin - entry->copy.begin) : range.begin);
        }
        if (overlaps(entry->source, range) ||
            (entry->copy.begin && overlaps(entry->copy, range)))
            return NULL;
    }
    return usable_mem1(state, range) ? reinterpret_cast<const void*>(range.begin) : NULL;
}
void discard_pending(Registry& state, const std::shared_ptr<Entry>& pending)
{
    for (auto it = state.entries.begin(); it != state.entries.end(); ++it) {
        if (*it == pending) {
            state.entries.erase(it);
            return;
        }
    }
}
} // namespace

bool port_gx_static_configure(void* mem1, size_t mem1_bytes,
                              port_gx_static_alloc_fn allocate, void* context)
{
    Range window;
    // BP texture addresses carry 24 bits after >>5, so keep the whole window
    // within that representable span as well as the four-byte pointer slots.
    if (!allocate || reinterpret_cast<uintptr_t>(mem1) != uintptr_t(0x80000000u) ||
        mem1_bytes <= 32 || mem1_bytes > size_t(0x20000000u) ||
        !make_range(mem1, mem1_bytes, window))
        return false;
    Registry& state = registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.configured)
        return state.mem1.begin == window.begin && state.mem1.end == window.end &&
               state.allocate == allocate && state.context == context;
    state.mem1 = window;
    state.allocate = allocate;
    state.context = context;
    state.configured = true;
    return true;
}

static const void* register_impl(const void* source, size_t bytes)
{
    Range source_range;
    if (!make_range(source, bytes, source_range))
        return NULL;
    Registry& state = registry();
    std::shared_ptr<Entry> pending;
    port_gx_static_alloc_fn allocate;
    void* context;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (!state.configured)
            return NULL;
        // A MEM1 resource needs no clone. Still honor tracked-range boundaries
        // so a query cannot escape a staged copy into its adjacent allocation.
        if (overlaps(state.mem1, source_range))
            return resolve_locked(state, source_range, false);
        for (const auto& entry : state.entries) {
            if (entry->source.begin == source_range.begin && entry->source.end == source_range.end)
                return entry->ready ? reinterpret_cast<const void*>(entry->copy.begin) : NULL;
            if (overlaps(entry->source, source_range) ||
                (entry->copy.begin && overlaps(entry->copy, source_range)))
                return NULL;
        }
        pending.reset(new Entry(source_range));
        state.entries.push_back(pending);
        allocate = state.allocate;
        context = state.context;
    }

    // The injected rootheap allocator can enter game scheduling. Never hold a
    // host mutex across it: another emulated thread might need the registry.
    void* copy;
    try {
        copy = allocate(bytes, 32, context);
    } catch (...) {
        std::lock_guard<std::mutex> lock(state.mutex);
        discard_pending(state, pending);
        return NULL;
    }
    Range copy_range;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        bool valid = make_range(copy, bytes, copy_range) &&
                     !(reinterpret_cast<uintptr_t>(copy) & 31) &&
                     usable_mem1(state, copy_range);
        if (valid) {
            for (const auto& entry : state.entries) {
                if (entry != pending && entry->copy.begin && overlaps(entry->copy, copy_range)) {
                    valid = false;
                    break;
                }
            }
        }
        if (!valid) {
            discard_pending(state, pending);
            return NULL;
        }
        pending->copy = copy_range;
    }
    std::memcpy(copy, source, bytes);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        pending->ready = true;
    }
    return copy;
}

const void* port_gx_static_register(const void* source, size_t bytes)
{
    const void* result = register_impl(source, bytes);
    if (!result) {
        Registry& state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
#if defined(__ANDROID__) && defined(SMS_ANDROID_APP)
        __android_log_print(ANDROID_LOG_ERROR, "SunshineGame",
            "immutable registration failed: source=%p bytes=%zu configured=%d heap=%p entries=%zu",
            source, bytes, int(state.configured), state.context, state.entries.size());
#endif
        std::fprintf(stderr, "[gx-mobile] immutable registration failed: source=%p bytes=%zu configured=%d heap=%p entries=%zu\n",
            source, bytes, int(state.configured), state.context, state.entries.size());
        for (const auto& entry : state.entries)
            std::fprintf(stderr, "[gx-mobile] source=[%p,%p) copy=[%p,%p) ready=%d\n",
                reinterpret_cast<void*>(entry->source.begin), reinterpret_cast<void*>(entry->source.end),
                reinterpret_cast<void*>(entry->copy.begin), reinterpret_cast<void*>(entry->copy.end), int(entry->ready));
    }
    return result;
}

const void* port_gx_static_resolve(const void* pointer, size_t bytes)
{
    Range range;
    if (!make_range(pointer, bytes, range))
        return NULL;
    Registry& state = registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.configured ? resolve_locked(state, range, false) : NULL;
}

const void* port_gx_static_original(const void* pointer, size_t bytes)
{
    Range range;
    if (!make_range(pointer, bytes, range))
        return NULL;
    Registry& state = registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.configured ? resolve_locked(state, range, true) : NULL;
}

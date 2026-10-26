#include "spc_native_registry.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

namespace {
struct CallbackEntry {
    std::vector<unsigned char> bytes;
    port_spc_native_invoker invoker;
};

struct CallbackRegistry {
    std::mutex mutex;
    std::vector<std::shared_ptr<const CallbackEntry> > entries;
};

CallbackRegistry& registry()
{
    // Intentionally process lifetime: tokens may outlive game scenes and may
    // still be used by other static objects during application shutdown.
    static CallbackRegistry* instance = new CallbackRegistry;
    return *instance;
}
} // namespace

uint32_t port_spc_register_erased(const void* callback_bytes, size_t byte_count,
                                port_spc_native_invoker invoker)
{
    if (!callback_bytes || !byte_count || !invoker)
        return 0;

    CallbackRegistry& state = registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    for (size_t i = 0; i < state.entries.size(); ++i) {
        const CallbackEntry& entry = *state.entries[i];
        if (entry.invoker == invoker && entry.bytes.size() == byte_count &&
            std::memcmp(entry.bytes.data(), callback_bytes, byte_count) == 0)
            return static_cast<uint32_t>(i + 1);
    }

    // No wrap, reset, or token reuse. Exhaustion is fatal rather than binding
    // the wrong callback or pretending that a valid function is null.
    if (state.entries.size() >= std::numeric_limits<uint32_t>::max())
        std::abort();

    std::shared_ptr<CallbackEntry> entry(new CallbackEntry);
    const unsigned char* bytes = static_cast<const unsigned char*>(callback_bytes);
    entry->bytes.assign(bytes, bytes + byte_count);
    entry->invoker = invoker;
    state.entries.push_back(entry);
    return static_cast<uint32_t>(state.entries.size());
}

bool port_spc_invoke(uint32_t token, void* interpreter, uint32_t argc)
{
    if (!token)
        return false;

    std::shared_ptr<const CallbackEntry> entry;
    {
        CallbackRegistry& state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        if (static_cast<size_t>(token) > state.entries.size())
            return false;
        entry = state.entries[static_cast<size_t>(token) - 1];
    }
    // The shared entry remains stable while a callback registers/invokes other
    // callbacks, including when that registration grows the registry vector.
    entry->invoker(entry->bytes.data(), interpreter, argc);
    return true;
}

void port_spc_call_checked(uint32_t token, void* interpreter, uint32_t argc)
{
    if (!port_spc_invoke(token, interpreter, argc))
        std::abort();
}

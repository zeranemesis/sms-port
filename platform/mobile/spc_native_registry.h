#ifndef SMS_MOBILE_SPC_NATIVE_REGISTRY_H
#define SMS_MOBILE_SPC_NATIVE_REGISTRY_H

// Host-only SPC callbacks. Tokens fit the existing script symbol's u32 slot;
// function pointers stay at their native width. This header supports C++03.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef void (*port_spc_native_invoker)(const void* callback_bytes,
                                      void* interpreter, uint32_t argc);

// Copies the callback representation immediately. An invoker identifies its
// callback type; identical representation + invoker receives the same token.
// Null/empty input returns zero. Tokens have process lifetime and are not reset.
uint32_t port_spc_register_erased(const void* callback_bytes, size_t byte_count,
                                port_spc_native_invoker invoker);

// Unknown/zero tokens return false without calling anything. The interpreter
// must be the matching Interp object used by the callback registration.
// Callback exceptions propagate. No registry lock is held during the call.
bool port_spc_invoke(uint32_t token, void* interpreter, uint32_t argc);
void port_spc_call_checked(uint32_t token, void* interpreter, uint32_t argc);

namespace port_spc_detail {
template <class Interp>
void invoke_native(const void* callback_bytes, void* interpreter, uint32_t argc)
{
    typedef void (*Callback)(Interp*, uint32_t);
    Callback callback;
    // memcpy avoids alignment/aliasing assumptions about the erased storage.
    memcpy(&callback, callback_bytes, sizeof(callback));
    callback(static_cast<Interp*>(interpreter), argc);
}
} // namespace port_spc_detail

template <class Interp>
uint32_t port_spc_register(void (*callback)(Interp*, uint32_t))
{
    if (!callback)
        return 0;
    return port_spc_register_erased(&callback, sizeof(callback),
                                   &port_spc_detail::invoke_native<Interp>);
}

#endif

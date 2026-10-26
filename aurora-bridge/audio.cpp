#include "sms_aurora_audio.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

struct SabAudioOutput {
    SDL_AudioStream* stream = nullptr;
    SabAudioCallback callback = nullptr;
    void* userdata = nullptr;
    SabAudioOutput* next = nullptr;
    std::atomic<bool> stopping{false};
    bool ownsAudioRef = false;
    // Whole PCM frames, allocated with the handle, never grown on the callback.
    alignas(16) uint8_t scratch[2048]{};

    ~SabAudioOutput() {
        stopping.store(true, std::memory_order_release);
        if (stream) {
            // SDL obtains the stream lock and waits for a running callback.
            // The registry and the AI FIFO mutex are not held by our closer.
            SDL_SetAudioStreamGetCallback(stream, nullptr, nullptr);
            SDL_DestroyAudioStream(stream); // also closes its logical device
        }
        if (ownsAudioRef) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
};

namespace {
std::mutex outputsMutex;
SabAudioOutput* outputs = nullptr;

void SDLCALL fill_stream(void* userdata, SDL_AudioStream* stream, int additional, int) {
    auto* output = static_cast<SabAudioOutput*>(userdata);
    if (!output || additional <= 0 || output->stopping.load(std::memory_order_acquire)) return;
    // additional is in the INPUT format (32kHz S16 stereo), not device frames.
    // SDL permits slight oversupply, so round only the final partial frame up.
    while (additional > 0 && !output->stopping.load(std::memory_order_acquire)) {
        int count = std::min(additional, int(sizeof output->scratch));
        count = (count + 3) & ~3;
        std::memset(output->scratch, 0, size_t(count));
        try {
            output->callback(output->userdata, output->scratch, count);
        } catch (...) {
            // No C++ exception may escape into SDL's C callback boundary.
            std::memset(output->scratch, 0, size_t(count));
        }
        // Put copies these bytes; the same preallocated scratch can be reused.
        if (!SDL_PutAudioStreamData(stream, output->scratch, count)) return;
        additional -= count;
    }
}
} // namespace

extern "C" SAB_API SabAudioHandle sab_audio_open(SabAudioCallback callback, void* userdata) {
    if (!callback) return nullptr;
    try {
        std::lock_guard<std::mutex> lock(outputsMutex);
        std::unique_ptr<SabAudioOutput> output(new (std::nothrow) SabAudioOutput);
        if (!output || !SDL_InitSubSystem(SDL_INIT_AUDIO)) return nullptr;
        output->ownsAudioRef = true;
        output->callback = callback;
        output->userdata = userdata;
        const SDL_AudioSpec format{SDL_AUDIO_S16, 2, 32000};
        output->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                                   &format, fill_stream, output.get());
        if (!output->stream) return nullptr;
        output->next = outputs;
        outputs = output.release();
        return outputs;
    } catch (...) {
        return nullptr;
    }
}

extern "C" SAB_API int sab_audio_start(SabAudioHandle handle) {
    if (!handle) return 0;
    try {
        std::lock_guard<std::mutex> lock(outputsMutex);
        for (auto* output = outputs; output; output = output->next) {
            if (output == handle) return SDL_ResumeAudioStreamDevice(output->stream) ? 1 : 0;
        }
    } catch (...) {
    }
    return 0;
}

extern "C" SAB_API void sab_audio_close(SabAudioHandle handle) {
    if (!handle) return;
    try {
        SabAudioOutput* removed = nullptr;
        {
            std::lock_guard<std::mutex> lock(outputsMutex);
            auto** link = &outputs;
            while (*link && *link != handle) link = &(*link)->next;
            if (!*link) return;
            removed = *link;
            *link = removed->next;
        }
        delete removed; // joins callback outside the registry/AI mutex
    } catch (...) {
    }
}

extern "C" SAB_API void sab_audio_close_all(void) {
    try {
        SabAudioOutput* removed;
        {
            std::lock_guard<std::mutex> lock(outputsMutex);
            removed = outputs;
            outputs = nullptr;
        }
        while (removed) {
            auto* next = removed->next;
            delete removed;
            removed = next;
        }
    } catch (...) {
    }
}

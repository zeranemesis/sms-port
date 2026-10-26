#include "mobile_input.h"
#include <dolphin/pad.h>
#include <cmath>
#include <mutex>
#include <cstdlib>
#if defined(__ANDROID__) && defined(SMS_ANDROID_APP)
#include <android/log.h>
#endif

namespace {
struct State {
    unsigned int buttons = 0;
    unsigned int pressed = 0;
    float lx = 0, ly = 0, cx = 0, cy = 0, lt = 0, rt = 0;
    float peakLt = 0, peakRt = 0;
};
State states[2];
std::mutex inputMutex;
constexpr unsigned int ValidButtons = PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X |
    PAD_BUTTON_Y | PAD_BUTTON_START | PAD_BUTTON_UP | PAD_BUTTON_DOWN |
    PAD_BUTTON_LEFT | PAD_BUTTON_RIGHT | PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_TRIGGER_Z;

float bounded(float value, float lo, float hi) {
    if (!std::isfinite(value)) return 0;
    return value < lo ? lo : value > hi ? hi : value;
}
int stick(float value) {
    const int result = int(value * 100.f);
    // Match pad.cpp's host-stick range/dead zone; PADClamp still performs
    // the game's own octagonal clamp, rather than changing the gameplay.
    return result > -8 && result < 8 ? 0 : result;
}
void mergeStick(s8& x, s8& y, float rawX, float rawY) {
    const int nx = stick(rawX), ny = stick(rawY);
    // Keep a complete stick vector from one source. Combining X from one
    // producer with Y from another would invent an unintended direction.
    if (nx * nx + ny * ny > int(x) * x + int(y) * y) {
        x = s8(nx);
        y = s8(ny);
    }
}
} // namespace

extern "C" void sms_mobile_pad_submit(int source, unsigned int buttons, float lx,
    float ly, float cx, float cy, float lt, float rt) {
    if (source < 0 || source >= 2) return;
    State next;
    next.buttons = buttons & ValidButtons;
    next.lx = bounded(lx, -1, 1); next.ly = bounded(ly, -1, 1);
    next.cx = bounded(cx, -1, 1); next.cy = bounded(cy, -1, 1);
    next.lt = bounded(lt, 0, 1); next.rt = bounded(rt, 0, 1);
    if (next.buttons & PAD_TRIGGER_L) next.lt = 1;
    if (next.buttons & PAD_TRIGGER_R) next.rt = 1;
    std::lock_guard<std::mutex> lock(inputMutex);
    // UI down/up can both arrive during a long game frame. Preserve a tap
    // until PADRead consumes it once; clearing a source still cancels it.
    next.pressed = states[source].pressed | (next.buttons & ~states[source].buttons);
    next.peakLt = next.lt > states[source].peakLt ? next.lt : states[source].peakLt;
    next.peakRt = next.rt > states[source].peakRt ? next.rt : states[source].peakRt;
#if defined(__ANDROID__) && defined(SMS_ANDROID_APP)
    if (std::getenv("SMS_ANDROID_INPUT_TRACE") && next.buttons != states[source].buttons)
        __android_log_print(ANDROID_LOG_INFO, "SunshineInput", "submit source=%d buttons=0x%x pending=0x%x", source, next.buttons, next.pressed);
#endif
    states[source] = next;
}

extern "C" void sms_mobile_pad_clear(int source) {
    std::lock_guard<std::mutex> lock(inputMutex);
    if (source == -1) states[0] = states[1] = State{};
    else if (source >= 0 && source < 2) states[source] = State{};
}

extern "C" void sms_mobile_pad_merge(PADStatus* status) {
    if (!status) return;
    State copy[2];
    {
        std::lock_guard<std::mutex> lock(inputMutex);
        copy[0] = states[0]; copy[1] = states[1];
        for (State& state : states) { state.pressed = 0; state.peakLt = state.peakRt = 0; }
    }
    for (const State& state : copy) {
        status->button |= u16(state.buttons | state.pressed);
        mergeStick(status->stickX, status->stickY, state.lx, state.ly);
        mergeStick(status->substickX, status->substickY, state.cx, state.cy);
        const int lt = int((state.lt > state.peakLt ? state.lt : state.peakLt) * 255.f);
        const int rt = int((state.rt > state.peakRt ? state.rt : state.peakRt) * 255.f);
        if (lt > status->triggerLeft) status->triggerLeft = u8(lt);
        if (rt > status->triggerRight) status->triggerRight = u8(rt);
    }
    // A partial JET press never sets the R click; full pressure does, at
    // exactly the same threshold as the existing SDL controller path.
    if (status->triggerLeft >= 250) status->button |= PAD_TRIGGER_L;
    if (status->triggerRight >= 250) status->button |= PAD_TRIGGER_R;
    status->analogA = status->button & PAD_BUTTON_A ? 255 : 0;
    status->analogB = status->button & PAD_BUTTON_B ? 255 : 0;
}

#ifdef __ANDROID__
#include <jni.h>
#include <atomic>
static std::atomic<bool> mobileMenuVisible{false};
extern "C" void sms_mobile_set_menu_visible(int visible) {
    mobileMenuVisible.store(visible != 0);
    if (visible) sms_mobile_pad_clear(-1);
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_smsunified_mobile_GameActivity_nativeMenuVisible(JNIEnv*, jclass) {
    return mobileMenuVisible.load() ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_org_smsunified_mobile_TouchControls_nativeSubmit(JNIEnv*, jclass, jint source,
    jint buttons, jfloat lx, jfloat ly, jfloat cx, jfloat cy, jfloat lt, jfloat rt) {
    sms_mobile_pad_submit(source, unsigned(buttons), lx, ly, cx, cy, lt, rt);
}
extern "C" JNIEXPORT void JNICALL
Java_org_smsunified_mobile_TouchControls_nativeClear(JNIEnv*, jclass, jint source) {
    sms_mobile_pad_clear(source);
}
#endif

#include "../mobile_input.h"
#include <dolphin/pad.h>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <thread>

static PADStatus read() {
    PADStatus status{};
    sms_mobile_pad_merge(&status);
    return status;
}
static void touch(unsigned bits, float x, float y, float right) {
    sms_mobile_pad_submit(SMS_MOBILE_TOUCH, bits, x, y, 0, 0, 0, right);
}
static void check(const char* name) { std::printf("PASS %s\n", name); }

int main() {
    sms_mobile_pad_clear(-1);
    touch(PAD_BUTTON_A, .7f, .4f, 160.f / 255.f);
    PADStatus status = read();
    assert(status.stickX == 70 && status.stickY == 40);
    assert(status.triggerRight == 160 && !(status.button & PAD_TRIGGER_R));
    assert(status.analogA == 255);
    PADStatus ports[4]{}; ports[0] = status;
    PADClamp(ports);
    assert(ports[0].triggerRight == 130 && !(ports[0].button & PAD_TRIGGER_R));
    check("moving JET stays analog before and after the real PADClamp");

    touch(PAD_TRIGGER_R, 0, 0, 1);
    status = read();
    assert(status.triggerRight == 255 && (status.button & PAD_TRIGGER_R));
    ports[0] = status; PADClamp(ports);
    assert(ports[0].triggerRight == 150 && (ports[0].button & PAD_TRIGGER_R));
    check("full JET preserves full pressure and the GameCube R click");

    sms_mobile_pad_clear(-1);
    status = PADStatus{}; status.triggerRight = 249;
    sms_mobile_pad_merge(&status);
    assert(!(status.button & PAD_TRIGGER_R));
    status.triggerRight = 250; sms_mobile_pad_merge(&status);
    assert(status.button & PAD_TRIGGER_R);
    check("digital click uses the existing SDL threshold 250");

    sms_mobile_pad_submit(SMS_MOBILE_CONTROLLER, PAD_BUTTON_B, -.9f, .9f, .5f, -.5f, .4f, .4f);
    touch(PAD_BUTTON_A, .6f, .8f, 160.f / 255.f);
    status = read();
    assert(status.stickX == -90 && status.stickY == 90);
    assert(status.substickX == 50 && status.substickY == -50);
    assert((status.button & (PAD_BUTTON_A | PAD_BUTTON_B)) == (PAD_BUTTON_A | PAD_BUTTON_B));
    assert(status.triggerRight == 160);
    sms_mobile_pad_clear(SMS_MOBILE_TOUCH);
    status = read();
    assert(status.button == PAD_BUTTON_B && status.triggerRight == 102);
    check("touch release leaves controller input intact and stick vectors stay coherent");

    sms_mobile_pad_clear(-1);
    status = PADStatus{}; status.button = PAD_BUTTON_X;
    status.stickX = 100; status.triggerLeft = 255;
    sms_mobile_pad_merge(&status);
    assert(status.stickX == 100 && (status.button & PAD_BUTTON_X) && (status.button & PAD_TRIGGER_L));
    check("neutral mobile producers preserve the existing SDL/keyboard PAD state");

    touch(0xffff0000u, 9, -9, std::numeric_limits<float>::infinity());
    sms_mobile_pad_submit(SMS_MOBILE_CONTROLLER, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0, 0, 0, 0);
    sms_mobile_pad_submit(4, PAD_BUTTON_A, 1, 1, 1, 1, 1, 1);
    status = read();
    assert(status.stickX == 100 && status.stickY == -100 && status.button == 0 && status.triggerRight == 0);
    check("invalid producer IDs, nonfinite values and unknown bits are rejected");

    touch(PAD_BUTTON_A | PAD_TRIGGER_R, 1, 1, 1);
    sms_mobile_pad_clear(-1);
    status = read();
    assert(status.button == 0 && status.stickX == 0 && status.stickY == 0 && status.triggerRight == 0);
    check("pause/focus reset releases every producer");

    touch(PAD_BUTTON_START, 0, 0, 0);
    touch(0, 0, 0, 0);
    assert(read().button & PAD_BUTTON_START);
    assert(!(read().button & PAD_BUTTON_START));
    check("a complete quick tap survives a long frame and is consumed once");

    touch(PAD_TRIGGER_R, 0, 0, 1);
    touch(0, 0, 0, 0);
    status = read();
    assert((status.button & PAD_TRIGGER_R) && status.triggerRight == 255);
    status = read();
    assert(!(status.button & PAD_TRIGGER_R) && status.triggerRight == 0);
    check("a quick full JET tap keeps its analog pressure with its click");

    std::atomic<bool> done{false};
    std::thread writer([&] {
        while (!done.load()) {
            touch(PAD_BUTTON_A, 1, 1, 1);
            touch(PAD_BUTTON_B, -1, -1, 0);
        }
    });
    for (int i = 0; i < 20000; ++i) {
        status = read();
        assert(status.stickX == status.stickY);
        if (status.button & PAD_BUTTON_A) assert(status.triggerRight == 255);
        if ((status.button & PAD_BUTTON_B) && !(status.button & PAD_BUTTON_A)) assert(status.triggerRight == 0);
    }
    done = true; writer.join();
    check("UI publication and game-thread reads cannot observe a torn snapshot");
    std::puts("10 mobile input tests passed");
}

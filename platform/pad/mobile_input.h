#ifndef SMS_MOBILE_INPUT_H
#define SMS_MOBILE_INPUT_H

// Platform input only. Axes are normalized [-1,1], with positive Y up;
// triggers are [0,1]. Buttons use the GameCube PAD bit layout.
enum SmsMobileInputSource { SMS_MOBILE_TOUCH = 0, SMS_MOBILE_CONTROLLER = 1 };

#ifdef __cplusplus
extern "C" {
#endif
struct PADStatus;
void sms_mobile_pad_submit(int source, unsigned int buttons, float lx, float ly,
                           float cx, float cy, float lt, float rt);
// source = -1 clears both producers (pause/focus loss).
void sms_mobile_pad_clear(int source);
void sms_mobile_pad_merge(struct PADStatus* status);
#ifdef __cplusplus
}
#endif
#endif

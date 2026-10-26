package org.smsunified.mobile;

import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;

/** Game-only overlay. The host must load libsms before installing it. */
public final class TouchControls extends View implements InputManager.InputDeviceListener {
    private static final int TOUCH = 0, CONTROLLER = 1;
    private static final int LEFT = 1, RIGHT = 2, DOWN = 4, UP = 8;
    private static final int Z = 16, R = 32, L = 64;
    private static final int A = 256, B = 512, X = 1024, Y = 2048, START = 4096;
    private static final int MOVE_STICK = 0, CAMERA_STICK = 1, BUTTON_FIRST = 2;
    // Raw PAD R=160, below the 250 digital-click threshold. The normal game
    // PADClamp/getLRLevel still apply; the overlay does not alter FLUDD logic.
    private static final float JET_FREE = 160f / 255f;

    private static native void nativeSubmit(int source, int buttons, float lx, float ly,
                                            float cx, float cy, float lt, float rt);
    private static native void nativeClear(int source);

    private static final class Button {
        final String label;
        final float x, y, size;
        final int bits;
        final float trigger;
        final RectF bounds = new RectF();
        Button(String label, float x, float y, float size, int bits, float trigger) {
            this.label = label; this.x = x; this.y = y; this.size = size;
            this.bits = bits; this.trigger = trigger;
        }
    }
    private static final class Finger {
        final int control;
        float x, y;
        Finger(int control, float x, float y) { this.control = control; this.x = x; this.y = y; }
    }

    private final Button[] buttons = {
        new Button("A", .91f, .70f, 1.15f, A, 0),
        new Button("B", .80f, .84f, 1f, B, 0),
        new Button("X", .70f, .62f, .85f, X, 0),
        new Button("Y", .81f, .54f, .85f, Y, 0),
        new Button("JET", .77f, .35f, 1f, 0, JET_FREE),
        new Button("JET +", .91f, .35f, 1f, R, 1),
        new Button("L", .065f, .17f, .85f, L, 0),
        new Button("Z", .93f, .16f, .85f, Z, 0),
        new Button("START", .50f, .12f, .8f, START, 0),
        new Button("↑", .37f, .17f, .6f, UP, 0),
        new Button("↓", .37f, .37f, .6f, DOWN, 0),
        new Button("←", .32f, .27f, .6f, LEFT, 0),
        new Button("→", .42f, .27f, .6f, RIGHT, 0)
    };
    private final SparseArray<Finger> fingers = new SparseArray<>();
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final InputManager input;
    private final float density;
    private float moveX, moveY, cameraX, cameraY, moveRadius, cameraRadius;
    private int controllerId = -1, keyButtons, hatButtons;
    private float lx, ly, cx, cy, lt, rt;
    private boolean digitalL2, digitalR2;
    private float controlSize = 1f;

    /** Add once after SDL's surface, on the Android UI thread. */
    public static TouchControls install(Activity activity) {
        TouchControls controls = new TouchControls(activity);
        activity.addContentView(controls, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        return controls;
    }

    public TouchControls(Context context) {
        super(context);
        density = getResources().getDisplayMetrics().density;
        input = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setContentDescription("Commandes Sunshine : déplacement, caméra, A B X Y, JET libre, JET visée, L Z et Start");
        setWillNotDraw(false);
        reloadPreferences();
    }
    public void reloadPreferences() {
        releaseInputs();
        updateVisibility();
        try { setAlpha(Math.max(.15f,Math.min(1f,Float.parseFloat(MobileSettings.get(getContext(), "touch_opacity", "70"))/100f))); }
        catch(NumberFormatException ignored) { setAlpha(.7f); }
        try { controlSize=Math.max(.8f,Math.min(1.2f,Float.parseFloat(MobileSettings.get(getContext(), "touch_size", "100"))/100f)); }
        catch(NumberFormatException ignored) { controlSize=1f; }
        layoutControls();
    }

    private void updateVisibility() {
        String mode=MobileSettings.get(getContext(),"touch_mode","auto");
        boolean controller=false;
        for(int id:InputDevice.getDeviceIds()) if(gamepad(InputDevice.getDevice(id))) {controller=true;break;}
        boolean show=mode.equals("always") || (!mode.equals("never") && !controller);
        if(!show) {fingers.clear();nativeClear(TOUCH);}
        setVisibility(show?VISIBLE:INVISIBLE);
    }
    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (input != null) input.registerInputDeviceListener(this, new Handler(Looper.getMainLooper()));
        requestApplyInsets();
    }
    @Override protected void onDetachedFromWindow() {
        releaseInputs();
        if (input != null) input.unregisterInputDeviceListener(this);
        super.onDetachedFromWindow();
    }
    @Override public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                   insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
        layoutControls();
        return insets;
    }
    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        fingers.clear();
        nativeClear(TOUCH);
        layoutControls();
    }
    private void layoutControls() {
        float w = Math.max(1, getWidth() - getPaddingLeft() - getPaddingRight());
        float h = Math.max(1, getHeight() - getPaddingTop() - getPaddingBottom());
        moveX = getPaddingLeft() + .15f * w; moveY = getPaddingTop() + .77f * h;
        cameraX = getPaddingLeft() + .59f * w; cameraY = getPaddingTop() + .80f * h;
        moveRadius = Math.min(84 * density, h * .17f) * controlSize;
        cameraRadius = Math.min(65 * density, h * .13f) * controlSize;
        float radius = Math.min(30 * density, h * .075f) * controlSize;
        for (Button button : buttons) {
            float x = getPaddingLeft() + button.x * w, y = getPaddingTop() + button.y * h;
            float r = radius * button.size;
            button.bounds.set(x - r, y - r, x + r, y + r);
        }
        invalidate();
    }
    private boolean controlHeld(int control) {
        for (int i = 0; i < fingers.size(); ++i)
            if (fingers.valueAt(i).control == control) return true;
        return false;
    }
    private int hit(float x, float y) {
        if (distance(x, y, moveX, moveY) <= moveRadius * 1.25f) return MOVE_STICK;
        if (distance(x, y, cameraX, cameraY) <= cameraRadius * 1.25f) return CAMERA_STICK;
        for (int i = 0; i < buttons.length; ++i)
            if (buttons[i].bounds.contains(x, y)) return BUTTON_FIRST + i;
        return -1;
    }
    private static float distance(float x, float y, float cx, float cy) {
        float dx = x - cx, dy = y - cy;
        return (float) Math.sqrt(dx * dx + dy * dy);
    }
    private void publishTouch() {
        int bits = 0;
        float x = 0, y = 0, sx = 0, sy = 0, right = 0;
        for (int i = 0; i < fingers.size(); ++i) {
            Finger finger = fingers.valueAt(i);
            if (finger.control == MOVE_STICK || finger.control == CAMERA_STICK) {
                boolean move = finger.control == MOVE_STICK;
                float radius = Math.max(1, move ? moveRadius : cameraRadius);
                float dx = (finger.x - (move ? moveX : cameraX)) / radius;
                float dy = ((move ? moveY : cameraY) - finger.y) / radius;
                float length = Math.max(1, (float) Math.sqrt(dx * dx + dy * dy));
                if (move) { x = dx / length; y = dy / length; }
                else { sx = dx / length; sy = dy / length; }
            } else {
                Button button = buttons[finger.control - BUTTON_FIRST];
                bits |= button.bits;
                right = Math.max(right, button.trigger);
            }
        }
        nativeSubmit(TOUCH, bits, x, y, sx, sy, (bits & L) != 0 ? 1 : 0, right);
        invalidate();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked(), index = event.getActionIndex();
        if ((action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_UP) &&
            getContext() instanceof Activity && ((Activity)getContext()).getIntent().getBooleanExtra("capture_frames", false))
            android.util.Log.i("SunshineInput", "touch action=" + action + " x=" + event.getX(index) + " y=" + event.getY(index) + " hit=" + hit(event.getX(index), event.getY(index)));
        if (action == MotionEvent.ACTION_CANCEL) {
            fingers.clear(); nativeClear(TOUCH); invalidate(); return true;
        }
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int control = hit(event.getX(index), event.getY(index));
            if (control >= 0 && fingers.size() < 10 &&
                (control >= BUTTON_FIRST || !controlHeld(control))) {
                fingers.put(event.getPointerId(index), new Finger(control, event.getX(index), event.getY(index)));
            }
        }
        for (int i = 0; i < event.getPointerCount(); ++i) {
            Finger finger = fingers.get(event.getPointerId(i));
            if (finger != null) { finger.x = event.getX(i); finger.y = event.getY(i); }
        }
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP)
            fingers.remove(event.getPointerId(index));
        publishTouch();
        return true;
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (!focused) releaseInputs();
    }
    /** Call from GameActivity.onPause before SDL pauses the game thread. */
    public void releaseInputs() {
        fingers.clear(); controllerId = -1; keyButtons = hatButtons = 0;
        lx = ly = cx = cy = lt = rt = 0; digitalL2 = digitalR2 = false;
        nativeClear(-1);
        invalidate();
    }

    private static boolean gamepad(InputDevice device) {
        if (device == null) return false;
        int sources = device.getSources();
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
               (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }
    private boolean selectDevice(int id) {
        if (controllerId == -1) controllerId = id;
        return controllerId == id;
    }
    private static boolean hasAxis(InputDevice device, int axis) {
        return device != null && device.getMotionRange(axis) != null;
    }
    private static float stickAxis(MotionEvent event, int axis) {
        InputDevice device = event.getDevice();
        InputDevice.MotionRange range = device == null ? null : device.getMotionRange(axis, event.getSource());
        if (range == null && device != null) range = device.getMotionRange(axis);
        if (range == null) return 0;
        float value = event.getAxisValue(axis);
        if (Math.abs(value) <= range.getFlat()) return 0;
        float scale = value < 0 ? -range.getMin() : range.getMax();
        return scale > 0 ? Math.max(-1, Math.min(1, value / scale)) : 0;
    }
    private static float triggerAxis(MotionEvent event, int axis, int fallback) {
        InputDevice device = event.getDevice();
        if (!hasAxis(device, axis)) axis = fallback;
        if (!hasAxis(device, axis)) return 0;
        InputDevice.MotionRange range = device.getMotionRange(axis);
        float span = range.getMax() - range.getMin();
        return span > 0 ? Math.max(0, Math.min(1, (event.getAxisValue(axis) - range.getMin()) / span)) : 0;
    }
    private void publishController() {
        nativeSubmit(CONTROLLER, keyButtons | hatButtons, lx, ly, cx, cy,
                     digitalL2 ? 1 : lt, digitalR2 ? 1 : rt);
    }
    /** Delegate Activity.dispatchGenericMotionEvent here before SDL. */
    public boolean handleGenericMotionEvent(MotionEvent event) {
        if (event.getActionMasked() != MotionEvent.ACTION_MOVE || !gamepad(event.getDevice())) return false;
        if (!selectDevice(event.getDeviceId())) return true;
        lx = stickAxis(event, MotionEvent.AXIS_X); ly = -stickAxis(event, MotionEvent.AXIS_Y);
        InputDevice device = event.getDevice();
        boolean zr = hasAxis(device, MotionEvent.AXIS_Z) && hasAxis(device, MotionEvent.AXIS_RZ);
        cx = stickAxis(event, zr ? MotionEvent.AXIS_Z : MotionEvent.AXIS_RX);
        cy = -stickAxis(event, zr ? MotionEvent.AXIS_RZ : MotionEvent.AXIS_RY);
        lt = triggerAxis(event, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE);
        rt = triggerAxis(event, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS);
        float hx = event.getAxisValue(MotionEvent.AXIS_HAT_X), hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y);
        hatButtons = (hx < -.5f ? LEFT : hx > .5f ? RIGHT : 0) | (hy < -.5f ? UP : hy > .5f ? DOWN : 0);
        publishController(); return true;
    }
    /** Delegate Activity.dispatchKeyEvent here before SDL. */
    public boolean handleKeyEvent(KeyEvent event) {
        InputDevice device = event.getDevice();
        if (!gamepad(device)) return false;
        int bit;
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_BUTTON_A: bit = A; break;
            case KeyEvent.KEYCODE_BUTTON_B: bit = B; break;
            case KeyEvent.KEYCODE_BUTTON_X: bit = X; break;
            case KeyEvent.KEYCODE_BUTTON_Y: bit = Y; break;
            case KeyEvent.KEYCODE_BUTTON_L1: bit = L; break;
            case KeyEvent.KEYCODE_BUTTON_R1: bit = Z; break;
            case KeyEvent.KEYCODE_BUTTON_START: case KeyEvent.KEYCODE_BUTTON_SELECT: bit = START; break;
            case KeyEvent.KEYCODE_DPAD_UP: bit = UP; break;
            case KeyEvent.KEYCODE_DPAD_DOWN: bit = DOWN; break;
            case KeyEvent.KEYCODE_DPAD_LEFT: bit = LEFT; break;
            case KeyEvent.KEYCODE_DPAD_RIGHT: bit = RIGHT; break;
            case KeyEvent.KEYCODE_BUTTON_L2: case KeyEvent.KEYCODE_BUTTON_R2: bit = 0; break;
            default: return false;
        }
        if (!selectDevice(event.getDeviceId())) return true;
        boolean down = event.getAction() == KeyEvent.ACTION_DOWN && !event.isCanceled();
        if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_L2) {
            // Android may emit an L2/R2 key at half travel as well as its
            // analog axis. Only use the digital fallback without that axis.
            if (!hasAxis(device, MotionEvent.AXIS_LTRIGGER) && !hasAxis(device, MotionEvent.AXIS_BRAKE)) digitalL2 = down;
        } else if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_R2) {
            if (!hasAxis(device, MotionEvent.AXIS_RTRIGGER) && !hasAxis(device, MotionEvent.AXIS_GAS)) digitalR2 = down;
        } else if (down) keyButtons |= bit;
        else keyButtons &= ~bit;
        publishController(); return true;
    }
    @Override public boolean onGenericMotionEvent(MotionEvent event) {
        return handleGenericMotionEvent(event) || super.onGenericMotionEvent(event);
    }
    @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
        return handleKeyEvent(event) || super.onKeyDown(keyCode, event);
    }
    @Override public boolean onKeyUp(int keyCode, KeyEvent event) {
        return handleKeyEvent(event) || super.onKeyUp(keyCode, event);
    }
    @Override public void onInputDeviceAdded(int id) { updateVisibility(); }
    @Override public void onInputDeviceChanged(int id) { onInputDeviceRemoved(id); }
    @Override public void onInputDeviceRemoved(int id) {
        updateVisibility();
        if (controllerId != id) return;
        controllerId = -1; keyButtons = hatButtons = 0;
        lx = ly = cx = cy = lt = rt = 0; digitalL2 = digitalR2 = false;
        nativeClear(CONTROLLER);
    }

    private void circle(Canvas canvas, float x, float y, float radius, String label, boolean held) {
        paint.setColor(held ? Color.argb(170, 50, 130, 190) : Color.argb(95, 20, 25, 35));
        paint.setStyle(Paint.Style.FILL); canvas.drawCircle(x, y, radius, paint);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(1.5f * density);
        paint.setColor(Color.argb(180, 245, 245, 245)); canvas.drawCircle(x, y, radius, paint);
        paint.setStyle(Paint.Style.FILL); paint.setTextAlign(Paint.Align.CENTER);
        paint.setTextSize(Math.min(13 * density, radius * .55f));
        canvas.drawText(label, x, y - (paint.ascent() + paint.descent()) / 2, paint);
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        circle(canvas, moveX, moveY, moveRadius, "MARIO", controlHeld(MOVE_STICK));
        circle(canvas, cameraX, cameraY, cameraRadius, "CAM", controlHeld(CAMERA_STICK));
        for (int i = 0; i < buttons.length; ++i) {
            Button button = buttons[i];
            circle(canvas, button.bounds.centerX(), button.bounds.centerY(), button.bounds.width() / 2,
                   button.label, controlHeld(BUTTON_FIRST + i));
        }
        for (int i = 0; i < fingers.size(); ++i) {
            Finger finger = fingers.valueAt(i);
            if (finger.control >= BUTTON_FIRST) continue;
            boolean move = finger.control == MOVE_STICK;
            float x = move ? moveX : cameraX, y = move ? moveY : cameraY;
            float radius = move ? moveRadius : cameraRadius;
            float length = Math.max(radius, distance(finger.x, finger.y, x, y));
            float scale = radius / Math.max(1, length);
            paint.setStyle(Paint.Style.FILL); paint.setColor(Color.argb(210, 120, 210, 255));
            canvas.drawCircle(x + (finger.x - x) * scale, y + (finger.y - y) * scale,
                              radius * .22f, paint);
        }
    }
}

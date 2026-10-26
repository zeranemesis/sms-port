package org.smsunified.mobile;

import android.os.Build;
import android.os.Process;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.content.Intent;
import android.view.View;
import android.view.Gravity;
import android.widget.Button;
import android.widget.FrameLayout;

/** A game session owns its process; a subsequent start has a fresh console OS. */
public class GameActivity extends AuroraActivity {
    private TouchControls controls;
    private Button menuButton;
    private static native boolean nativeMenuVisible();
    private final android.os.Handler menuHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private boolean menuWasVisible;
    private final Runnable menuPoll = new Runnable() {
        @Override public void run() {
            boolean visible = nativeMenuVisible();
            if (menuButton != null) menuButton.setVisibility(visible ? View.INVISIBLE : View.VISIBLE);
            if (controls != null) {
                if (visible) {
                    if (!menuWasVisible) controls.releaseInputs();
                    controls.setVisibility(View.INVISIBLE);
                }
                if (!visible && menuWasVisible) controls.reloadPreferences();
            }
            menuWasVisible = visible;
            menuHandler.postDelayed(this, 80);
        }
    };
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        controls = TouchControls.install(this);
        Button menu = new Button(this); menuButton = menu; menu.setText("MENU");
        FrameLayout.LayoutParams position = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        position.leftMargin = (int)(180 * getResources().getDisplayMetrics().density);
        menu.setOnClickListener(v -> openMobileMenu()); addContentView(menu, position);
        menuHandler.post(menuPoll);
    }
    private void openMobileMenu() {
        if (controls != null) controls.releaseInputs();
        // Same in-game PartyBoard menu shortcut as Mario Party 4's F1/Back.
        org.libsdl.app.SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_F1);
        org.libsdl.app.SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_F1);
    }
    @Override public void onBackPressed() { openMobileMenu(); }
    @Override protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (intent.getBooleanExtra("quit_game", false)) finish();
        else if (intent.getBooleanExtra("open_menu", false) && !nativeMenuVisible()) openMobileMenu();
    }
    @Override protected void onResume() {
        super.onResume();
        if (controls != null) controls.reloadPreferences();
    }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if (nativeMenuVisible()) return super.dispatchKeyEvent(event);
        return controls != null && controls.handleKeyEvent(event) || super.dispatchKeyEvent(event);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (nativeMenuVisible()) return super.dispatchGenericMotionEvent(event);
        return controls != null && controls.handleGenericMotionEvent(event) || super.dispatchGenericMotionEvent(event);
    }
    @Override protected void onPause() {
        if (controls != null) controls.releaseInputs();
        super.onPause();
    }
    @Override protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL3", "sms_aurora", "sms" };
    }
    @Override protected String[] getArguments() {
        boolean emulator = Build.HARDWARE.contains("ranchu") || Build.HARDWARE.contains("goldfish");
        java.util.ArrayList<String> arguments = new java.util.ArrayList<>();
        if (emulator) arguments.add("--android-cpu-adapter");
        if (getIntent().getBooleanExtra("capture_frames", false)) arguments.add("--android-capture");
        if (getIntent().getBooleanExtra("stage_diagnostic", false)) {
            arguments.add("--android-stage-tour");
            arguments.add("--android-stage=" + getIntent().getIntExtra("diagnostic_stage", 2));
            arguments.add("--android-scenario=" + getIntent().getIntExtra("diagnostic_scenario", 0));
            arguments.add("--android-stage-frames=" + getIntent().getIntExtra("diagnostic_frames", 1800));
        }
        if (getIntent().getBooleanExtra("performance_profile", false)) arguments.add("--android-performance-profile");
        if (getIntent().getBooleanExtra("fifo_reference", false)) arguments.add("--android-fifo-reference");
        if (getIntent().getBooleanExtra("surface_reference", false)) arguments.add("--android-surface-reference");
        if (getIntent().getBooleanExtra("shader_cold_test", false)) arguments.add("--android-shader-cold-test");
        if (getIntent().getBooleanExtra("open_menu", false)) arguments.add("--android-menu");
        return arguments.toArray(new String[0]);
    }
    @Override protected void onDestroy() {
        menuHandler.removeCallbacksAndMessages(null);
        // SDL sends quit and waits for its main thread. If the native entry
        // returned, no global console runtime is reused in a surviving JVM.
        super.onDestroy();
        Process.killProcess(Process.myPid());
    }
}

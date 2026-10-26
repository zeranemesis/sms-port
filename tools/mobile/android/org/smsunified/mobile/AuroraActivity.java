package org.smsunified.mobile;

import android.content.Context;
import android.view.SurfaceHolder;
import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

/** SDL owns the Surface; Aurora observes its real lifetime, including boot. */
public abstract class AuroraActivity extends SDLActivity {
    private static native void nativeSurfaceReady(boolean ready);

    @Override protected SDLSurface createSDLSurface(Context context) {
        return new AuroraSurface(context);
    }

    private static final class AuroraSurface extends SDLSurface {
        AuroraSurface(Context context) { super(context); }

        @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            nativeSurfaceReady(false);
            super.surfaceChanged(holder, format, width, height);
            nativeSurfaceReady(mIsSurfaceReady);
        }

        @Override public void surfaceDestroyed(SurfaceHolder holder) {
            // Stop new Aurora frames before SDL invalidates the native window.
            nativeSurfaceReady(false);
            super.surfaceDestroyed(holder);
        }
    }
}

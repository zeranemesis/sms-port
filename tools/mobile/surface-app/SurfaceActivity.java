package org.smsunified.surface;

import org.smsunified.mobile.AuroraActivity;

/** Dedicated renderer diagnostic. Does not load or start the game. */
public class SurfaceActivity extends AuroraActivity {
    @Override protected String[] getLibraries() {
        return new String[] { "c++_shared", "SDL3", "sms_aurora", "sms_mobile_surface_probe" };
    }
}

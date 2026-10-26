package org.smsunified.diagnostic;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public final class ProbeActivity extends Activity {
    static { System.loadLibrary("sms_mobile_probe_jni"); }
    private static native String nativeRun();
    private TextView output;
    private Button rerun;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(24, 48, 24, 24);
        TextView title = new TextView(this);
        title.setText("Sunshine — diagnostic Android\nTest du runtime ; le jeu n'est pas encore inclus.");
        title.setTextSize(20);
        layout.addView(title);
        rerun = new Button(this);
        rerun.setText("Relancer le diagnostic");
        layout.addView(rerun);
        output = new TextView(this);
        output.setTextIsSelectable(true);
        output.setTypeface(android.graphics.Typeface.MONOSPACE);
        ScrollView scroll = new ScrollView(this);
        scroll.addView(output);
        layout.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        setContentView(layout);
        rerun.setOnClickListener(view -> runProbe());
        runProbe();
    }

    private void runProbe() {
        rerun.setEnabled(false);
        output.setText("Diagnostic en cours…");
        new Thread(() -> {
            String result;
            try { result = nativeRun(); }
            catch (Throwable error) { result = "RESULT FAIL\n" + error; }
            Log.i("SunshineProbe", result);
            try (FileOutputStream file = openFileOutput("last-result.txt", MODE_PRIVATE)) {
                file.write(result.getBytes(StandardCharsets.UTF_8));
            } catch (Exception error) { Log.e("SunshineProbe", "Cannot save result", error); }
            final String text = result;
            runOnUiThread(() -> {
                output.setText(text);
                rerun.setEnabled(true);
            });
        }, "SunshineProbe").start();
    }
}

package org.smsunified.mobile;

import android.content.Context;
import android.util.AtomicFile;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

/** The Java menus and native runtime share one file, including across processes. */
final class MobileSettings {
    static Map<String,String> read(Context context) {
        Map<String,String> result = new LinkedHashMap<>();
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                new AtomicFile(new File(context.getFilesDir(), "settings.txt")).openRead(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                int equals = line.indexOf('=');
                if (equals > 0 && !line.trim().startsWith("#"))
                    result.put(line.substring(0, equals).trim(), line.substring(equals + 1).trim());
            }
        } catch (FileNotFoundException firstRun) {
        } catch (IOException error) { android.util.Log.e("SunshineSettings", "Read failed", error); }
        return result;
    }
    static void put(Context context, String key, String value) throws IOException {
        Map<String,String> values = read(context); values.put(key, value);
        AtomicFile file = new AtomicFile(new File(context.getFilesDir(), "settings.txt"));
        FileOutputStream output = file.startWrite();
        try {
            for (Map.Entry<String,String> entry : values.entrySet())
                output.write((entry.getKey() + " = " + entry.getValue() + "\n").getBytes(StandardCharsets.UTF_8));
            file.finishWrite(output);
        } catch (IOException failure) { file.failWrite(output); throw failure; }
    }
    static String get(Context context, String key, String fallback) {
        String value = read(context).get(key); return value == null ? fallback : value;
    }
}

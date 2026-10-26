package org.smsunified.mobile;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;

/** User-owned disc import stays outside the native game process. */
public class LauncherActivity extends Activity {
    private TextView status;
    private Button play, load;
    private static final int IMPORT_DISC = 1;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        installMenuResources();
        installPipelineSeed();
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(32, 32, 32, 32);
        TextView title = new TextView(this);
        title.setText("Super Mario Sunshine — Aurora"); title.setTextSize(26);
        layout.addView(title);
        status = new TextView(this); status.setTextSize(18); layout.addView(status);
        load = new Button(this); load.setText("Importer mon disque PAL (.iso / .gcm)");
        load.setOnClickListener(v -> {
            Intent choose = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            choose.addCategory(Intent.CATEGORY_OPENABLE); choose.setType("*/*");
            startActivityForResult(choose, IMPORT_DISC);
        });
        layout.addView(load);
        play = new Button(this); play.setText("Jouer"); play.setOnClickListener(v -> launch()); layout.addView(play);
        Button settings = new Button(this); settings.setText("Réglages, commandes, sauvegardes et mods");
        settings.setOnClickListener(v -> { if (hasGame()) launch(true); else new AlertDialog.Builder(this).setMessage("Importe les données du jeu pour ouvrir le menu PartyBoard.").setPositiveButton("Fermer",null).show(); }); layout.addView(settings);
        Button log = new Button(this); log.setText("Voir le journal du dernier lancement");
        log.setOnClickListener(v -> showLog()); layout.addView(log);
        setContentView(layout);
        refresh();
        if (state == null && getIntent().getBooleanExtra("launch_game", false) && hasGame()) {
            getIntent().removeExtra("launch_game");
            launch();
        }
    }
    @Override protected void onResume() { super.onResume(); if (status != null) refresh(); }
    private void installMenuResources() {
        File destination = new File(getFilesDir(), "res/partyboard"); destination.mkdirs();
        try {
            String[] resources = getAssets().list("partyboard");
            if (resources != null) for (String name : resources) {
                android.util.AtomicFile file = new android.util.AtomicFile(new File(destination,name));
                FileOutputStream output = file.startWrite();
                try (InputStream input = getAssets().open("partyboard/"+name)) {
                    byte[] buffer=new byte[65536];int n;while((n=input.read(buffer))!=-1)output.write(buffer,0,n);
                    file.finishWrite(output);
                } catch(Exception error) { file.failWrite(output); throw error; }
            }
        } catch(Exception error) { android.util.Log.e("SunshineMenu","Resource installation failed",error); }
    }
    private void installPipelineSeed() {
        android.util.AtomicFile file = new android.util.AtomicFile(new File(getFilesDir(), "initial_pipeline_cache.db"));
        FileOutputStream output = null;
        try (InputStream input = getAssets().open("initial_pipeline_cache.db")) {
            output = file.startWrite();
            byte[] buffer = new byte[65536]; int n;
            while ((n = input.read(buffer)) != -1) output.write(buffer, 0, n);
            file.finishWrite(output);
        } catch (java.io.FileNotFoundException optional) {
            if (output != null) file.failWrite(output);
        } catch (Exception error) {
            if (output != null) file.failWrite(output);
            android.util.Log.e("SunshineShaders", "Pipeline seed installation failed", error);
        }
    }
    private boolean hasGame() {
        File base = getExternalFilesDir(null);
        return base != null && (new File(base, "game.iso").isFile() || new File(base, "game/files").isDirectory());
    }
    private void refresh() {
        boolean ready = hasGame(); play.setEnabled(ready);
        status.setText(ready ? "Données du jeu disponibles. Manette ou commandes tactiles." : "Importe ton disque PAL pour commencer.");
    }
    private void launch() {
        launch(false);
    }
    private void launch(boolean menu) {
        Intent game = new Intent(this, GameActivity.class);
        game.putExtra("open_menu", menu);
        game.putExtra("capture_frames", getIntent().getBooleanExtra("capture_frames", false));
        game.putExtra("stage_diagnostic", getIntent().getBooleanExtra("stage_diagnostic", false));
        game.putExtra("diagnostic_stage", getIntent().getIntExtra("diagnostic_stage", 2));
        game.putExtra("diagnostic_scenario", getIntent().getIntExtra("diagnostic_scenario", 0));
        game.putExtra("diagnostic_frames", getIntent().getIntExtra("diagnostic_frames", 1800));
        game.putExtra("performance_profile", getIntent().getBooleanExtra("performance_profile", false));
        game.putExtra("fifo_reference", getIntent().getBooleanExtra("fifo_reference", false));
        game.putExtra("surface_reference", getIntent().getBooleanExtra("surface_reference", false));
        game.putExtra("shader_cold_test", getIntent().getBooleanExtra("shader_cold_test", false));
        startActivity(game);
    }
    @Override protected void onActivityResult(int request, int result, Intent selection) {
        super.onActivityResult(request, result, selection);
        if (request != IMPORT_DISC || result != RESULT_OK || selection == null || selection.getData() == null) return;
        load.setEnabled(false); play.setEnabled(false); status.setText("Import du disque…");
        final android.net.Uri uri = selection.getData();
        new Thread(() -> {
            String error = null;
            File folder = getExternalFilesDir(null);
            File temporary = new File(folder, "game.importing");
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                if (input == null) throw new java.io.IOException("Disque illisible");
                byte[] header = new byte[32]; int length = 0;
                while (length < header.length) { int got = input.read(header, length, header.length - length); if (got < 0) break; length += got; }
                if (length != 32 || header[0] != 'G' || header[1] != 'M' || header[2] != 'S' || header[3] != 'P'
                    || header[4] != '0' || header[5] != '1' || (header[28] & 255) != 0xC2 || (header[29] & 255) != 0x33
                    || (header[30] & 255) != 0x9F || (header[31] & 255) != 0x3D)
                    throw new java.io.IOException("Cette version attend un disque GameCube PAL GMSP01 au format ISO/GCM.");
                try (FileOutputStream output = new FileOutputStream(temporary)) {
                    output.write(header); byte[] buffer = new byte[1024 * 1024]; int got;
                    while ((got = input.read(buffer)) != -1) output.write(buffer, 0, got);
                    output.getFD().sync();
                }
                if (!temporary.renameTo(new File(folder, "game.iso"))) throw new java.io.IOException("Impossible de finaliser l'import");
            } catch (Exception problem) { error = problem.getMessage(); temporary.delete(); }
            final String message = error;
            runOnUiThread(() -> { load.setEnabled(true); refresh(); if (message != null) status.setText(message); });
        }, "DiscImport").start();
    }
    private void showLog() {
        String message;
        File path = new File(getFilesDir(), "game-runtime.log");
        try (FileInputStream input = new FileInputStream(path)) {
            long skip = Math.max(0, path.length() - 12000); input.getChannel().position(skip);
            byte[] bytes = new byte[12000]; int got = input.read(bytes);
            message = got > 0 ? new String(bytes, 0, got, java.nio.charset.StandardCharsets.UTF_8) : "Journal vide.";
        } catch (Exception absent) { message = "Aucun lancement enregistré."; }
        new AlertDialog.Builder(this).setTitle("Dernier lancement").setMessage(message).setPositiveButton("Fermer", null).show();
    }
}

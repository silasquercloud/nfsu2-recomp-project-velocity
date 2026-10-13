package com.nfsu2.androidevolved;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Launcher in the style of the MW and Carbon ports: game files and JUGAR on the left, options on the
 * right (Vulkan driver, resolution, frame cap, widescreen, compatibility).
 */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_DRIVER_ZIP = 45, REQUEST_DRIVER_PROBE = 46, REQUEST_SAVE_REPORT = 47, REQUEST_SAVE_VELOCITY = 48;
    // NFSU2 green.
    private static final int ACCENT = 0xFF8EE03A;
    /** {label, guest language name, LANGUAGES/<file>.bin}. */
    private static final String[][] LANGUAGES = {
        {"Español", "Spanish", "Spanish"}, {"English", "English UK", "English"},
        {"Français", "French", "French"}, {"Deutsch", "German", "German"},
        {"Italiano", "Italian", "Italian"}, {"Nederlands", "Dutch", "Dutch"},
        {"Svenska", "Swedish", "Swedish"}, {"Dansk", "Danish", "Danish"},
        {"日本語", "Japanese", "Japanese"}, {"한국어", "Korean", "Korean"},
        {"繁體中文", "Chinese (Traditional)", "Chinese"}, {"ไทย", "Thai", "Thailand"}
    };

    private LinearLayout checksList, optionsList;
    private TextView status;
    private Button play, access;
    private boolean playAfterProbe, checkingUpdates, resumed;
    private File pendingReport;
    private String pendingCrash;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override protected void attachBaseContext(android.content.Context base) {
        super.attachBaseContext(UiText.wrap(base));
    }

    @Override protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
        setContentView(buildScreen());
        checkUpdates(false);
    }

    @Override protected void onResume() {
        super.onResume();
        resumed = true;
        refresh();
        String crash = Diagnostics.unreportedCrash(this);
        if (crash != null) {
            Diagnostics.acknowledgeCrashes(this);
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(UiText.t(LauncherActivity.this, R.string.crash_title)).setMessage(crash + UiText.t(LauncherActivity.this, R.string.crash_prompt))
                    .setPositiveButton(UiText.t(LauncherActivity.this, R.string.report_github), (dialog, which) -> prepareReport(crash))
                    .setNegativeButton(UiText.t(LauncherActivity.this, R.string.not_now), null).show();
        }
    }

    @Override protected void onPause() {
        resumed = false;
        super.onPause();
    }

    // ---- Screen ------------------------------------------------------------------------------------------

    private View buildScreen() {
        FrameLayout screen = new FrameLayout(this);
        screen.setBackground(new GradientDrawable(GradientDrawable.Orientation.TL_BR,
                new int[] {0xFF060D08, 0xFF0B2210, 0xFF060D08}));
        LinearLayout columns = new LinearLayout(this);
        columns.setPadding(dp(24), dp(10), dp(24), dp(10));
        screen.addView(columns, new FrameLayout.LayoutParams(-1, -1));

        // Left: logo, game files and JUGAR.
        LinearLayout left = new LinearLayout(this);
        left.setOrientation(LinearLayout.VERTICAL);
        left.setGravity(Gravity.CENTER_HORIZONTAL);
        ScrollView leftScroll = new ScrollView(this);
        leftScroll.setFillViewport(true);
        leftScroll.addView(left);
        columns.addView(leftScroll, new LinearLayout.LayoutParams(0, -1, 1.1f));

        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.icono);
        logo.setAdjustViewBounds(true);
        header.addView(logo, new LinearLayout.LayoutParams(dp(76), dp(76)));
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.setPadding(dp(14), 0, 0, 0);
        titles.addView(label("Need for Speed Underground 2", 18, Color.WHITE, true));
        titles.addView(label(UiText.t(LauncherActivity.this, R.string.native_subtitle), 12, 0x99FFFFFF, false));
        header.addView(titles);
        left.addView(header, new LinearLayout.LayoutParams(-1, -2));

        LinearLayout card = card();
        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(-1, -2);
        cardParams.topMargin = dp(8);
        left.addView(card, cardParams);
        card.addView(label(UiText.t(LauncherActivity.this, R.string.game_files), 12, ACCENT, true));
        status = label("", 13, 0xDDFFFFFF, false);
        status.setPadding(0, dp(4), 0, dp(6));
        card.addView(status);
        checksList = new LinearLayout(this);
        checksList.setOrientation(LinearLayout.VERTICAL);
        card.addView(checksList);

        play = actionButton(UiText.t(LauncherActivity.this, R.string.play), true);
        play.setOnClickListener(view -> play());
        LinearLayout.LayoutParams playParams = new LinearLayout.LayoutParams(-1, dp(54));
        playParams.topMargin = dp(8);
        left.addView(play, playParams);

        access = actionButton(UiText.t(LauncherActivity.this, R.string.storage_access), false);
        access.setOnClickListener(view -> requestAccess());
        LinearLayout.LayoutParams accessParams = new LinearLayout.LayoutParams(-1, dp(46));
        accessParams.topMargin = dp(10);
        left.addView(access, accessParams);

        // Right: options.
        LinearLayout right = card();
        LinearLayout.LayoutParams rightParams = new LinearLayout.LayoutParams(0, -1, 1f);
        rightParams.leftMargin = dp(24);
        columns.addView(right, rightParams);
        right.addView(label(UiText.t(LauncherActivity.this, R.string.options), 12, ACCENT, true));
        TextView note = label(UiText.t(LauncherActivity.this, R.string.options_apply), 12, 0x88FFFFFF, false);
        note.setPadding(0, dp(2), 0, dp(4));
        right.addView(note);
        ScrollView scroll = new ScrollView(this);
        optionsList = new LinearLayout(this);
        optionsList.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(optionsList);
        right.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1f));
        return screen;
    }

    private void refresh() {
        boolean allowed = allowed();
        File root = gameRoot();
        checksList.removeAllViews();
        boolean exe = allowed && new File(root, "SPEED2.EXE").isFile();
        addCheck("SPEED2.EXE", exe, true);
        StringBuilder missing = new StringBuilder();
        for (String folder : new String[] {"GLOBAL", "TRACKS", "CARS", "FRONTEND", "LANGUAGES"})
            if (!(allowed && new File(root, folder).isDirectory())) missing.append(missing.length() == 0 ? "" : ", ").append(folder);
        addCheck(missing.length() == 0 ? UiText.t(LauncherActivity.this, R.string.game_folders) : UiText.t(LauncherActivity.this, R.string.missing_folders) + missing, missing.length() == 0, true);
        addCheck(UiText.t(LauncherActivity.this, R.string.widescreen_data),
                allowed && new File(root, "scripts/NFSUnderground2.WidescreenFix.dat").isFile(), false);
        boolean ready = exe && !languages().isEmpty();
        status.setText(!allowed ? UiText.t(LauncherActivity.this, R.string.allow_storage)
                : ready ? UiText.t(LauncherActivity.this, R.string.storage_path) : UiText.t(LauncherActivity.this, R.string.copy_game));
        play.setEnabled(ready);
        play.setAlpha(ready ? 1f : .45f);
        access.setVisibility(allowed ? View.GONE : View.VISIBLE);
        refreshOptions();
    }

    private void refreshOptions() {
        optionsList.removeAllViews();
        optionsList.addView(optionRow(getString(R.string.ui_language), launcherLocaleLabel(), this::chooseLauncherLocale));
        optionsList.addView(optionRow(getString(R.string.v5_graphics_dxvk_manager), getString(R.string.v5_verified_matched_runtime_packages), () -> new DxvkManager(this,this::refreshOptions).show()));
        optionsList.addView(optionRow(getString(R.string.v5_pipeline_cache), getString(R.string.v5_clear_on_next_game_launch), () -> {
            new AlertDialog.Builder(this).setTitle(getString(R.string.velocity4_clear_shader_cache_title)).setMessage(getString(R.string.velocity4_clear_shader_cache_body))
                .setPositiveButton(getString(R.string.v5_clear),(dialog,which)->{try{new File(getFilesDir(),"velocity-clear-cache").createNewFile();}catch(Exception error){message(getString(R.string.v5_cache),error.getMessage());}})
                .setNegativeButton(android.R.string.cancel,null).show();
        }));
        optionsList.addView(optionRow(getString(R.string.v5_shader_compilation_statistics), getString(R.string.v5_export_csv_compile_rate_waits_cache_lookups), this::exportVelocity));
        GpuDrivers.Driver driver = GpuDrivers.selected(this);
        // Custom drivers (libadrenotools) exist only in the 64-bit build.
        if (android.os.Process.is64Bit())
            optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.vulkan_driver), driver == null ? UiText.t(LauncherActivity.this, R.string.system_driver_short) : driver.name, this::chooseDriver));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.probe_driver), UiText.t(LauncherActivity.this, R.string.probe_capabilities), () -> probeDriver(false)));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.game_language), languageLabel(), this::chooseLanguage));
        for (GameOptions.Option option : GameOptions.ALL)
            optionsList.addView(optionRow(option.title(this), option.label(this, GameOptions.get(this, option.key)), () -> chooseOption(option)));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.velocity_section), getString(R.string.v5_experimental_version,BuildConfig.VERSION_NAME), () ->
                message(UiText.t(LauncherActivity.this, R.string.velocity_section), UiText.t(LauncherActivity.this, R.string.velocity_help))));
        for (GameOptions.Option option : GameOptions.VELOCITY) {
            if (option.key.equals(GameOptions.PROFILING) && !BuildConfig.VELOCITY_PROFILING) continue;
            optionsList.addView(optionRow(option.title(this), option.label(this, GameOptions.get(this, option.key)), () -> chooseOption(option)));
        }
        if (BuildConfig.VELOCITY_PROFILING)
            optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.export_performance), UiText.t(LauncherActivity.this, R.string.export_hint), this::exportVelocity));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.online_server), GameOptions.onlineServer(this), this::chooseOnlineServer));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.check_updates),
                checkingUpdates ? UiText.t(LauncherActivity.this, R.string.checking_github) : UiText.t(LauncherActivity.this, R.string.version_prefix) + ReleaseUpdates.installed(this), () -> checkUpdates(true)));
        optionsList.addView(optionRow(UiText.t(LauncherActivity.this, R.string.report_problem), UiText.t(LauncherActivity.this, R.string.full_log), () -> prepareReport(null)));
    }

    private void exportVelocity() {
        File report = new File(getFilesDir(),"velocity-performance.csv");
        if (!report.isFile()) {
            message("Project Velocity", UiText.t(LauncherActivity.this, R.string.no_frames));
            return;
        }
        startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                .setType("text/csv").putExtra(Intent.EXTRA_TITLE,"Project-Velocity-performance.csv"), REQUEST_SAVE_VELOCITY);
    }

    // ---- Options ---------------------------------------------------------------------------------------

    private List<String[]> languages() {
        List<String[]> found = new ArrayList<>();
        File root = gameRoot();
        for (String[] language : LANGUAGES)
            if (new File(root, "LANGUAGES/" + language[2] + ".bin").isFile()) found.add(language);
        return found;
    }

    private String languageLabel() {
        String current = GameOptions.language(this);
        for (String[] language : LANGUAGES) if (language[1].equals(current)) return language[0];
        return current;
    }

    private String launcherLocaleLabel() {
        String tag=GameOptions.prefs(this).getString(UiText.PREF,"system");
        return getString(tag.equals("pt-BR")?R.string.locale_portuguese:tag.equals("en")?R.string.locale_english:R.string.locale_system);
    }

    private void chooseLauncherLocale() {
        String[] tags={"system","pt-BR","en"};
        String current=GameOptions.prefs(this).getString(UiText.PREF,"system");
        int selected=java.util.Arrays.asList(tags).indexOf(current);
        new AlertDialog.Builder(this).setTitle(R.string.ui_language)
                .setSingleChoiceItems(new String[]{getString(R.string.locale_system),getString(R.string.locale_portuguese),getString(R.string.locale_english)},Math.max(0,selected),(dialog,which)->{
                    GameOptions.prefs(this).edit().putString(UiText.PREF,tags[which]).apply();
                    dialog.dismiss();recreate();
                }).setNegativeButton(R.string.cancel,null).show();
    }

    private void chooseLanguage() {
        List<String[]> found = allowed() ? languages() : new ArrayList<>();
        if (found.isEmpty()) { message(UiText.t(LauncherActivity.this, R.string.game_language), UiText.t(LauncherActivity.this, R.string.no_game_languages)); return; }
        String[] names = new String[found.size()];
        int checked = 0;
        for (int i = 0; i < names.length; i++) {
            names[i] = found.get(i)[0];
            if (found.get(i)[1].equals(GameOptions.language(this))) checked = i;
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle(UiText.t(LauncherActivity.this, R.string.game_language))
                .setSingleChoiceItems(names, checked, (dialog, which) -> {
                    GameOptions.prefs(this).edit().putString(GameOptions.LANGUAGE, found.get(which)[1]).apply();
                    dialog.dismiss(); refreshOptions();
                }).setNegativeButton(UiText.t(LauncherActivity.this, R.string.cancel), null).show();
    }

    private void chooseOption(GameOptions.Option option) {
        String current = GameOptions.get(this, option.key);
        int checked = 0;
        for (int i = 0; i < option.values.length; i++) if (option.values[i].equals(current)) checked = i;
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle(option.title(this))
                .setSingleChoiceItems(option.labels(this), checked, (dialog, which) -> {
                    GameOptions.set(this, option.key, option.values[which]);
                    dialog.dismiss(); refreshOptions();
                }).setNegativeButton(UiText.t(LauncherActivity.this, R.string.cancel), null).show();
    }

    private void chooseOnlineServer() {
        android.widget.EditText input = new android.widget.EditText(this);
        input.setSingleLine(true);
        input.setText(GameOptions.onlineServer(this));
        input.setSelection(input.getText().length());
        input.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_VARIATION_URI);
        android.widget.FrameLayout box = new android.widget.FrameLayout(this);
        int pad = (int) (20 * getResources().getDisplayMetrics().density);
        box.setPadding(pad, pad / 2, pad, 0);
        box.addView(input);
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle(UiText.t(LauncherActivity.this, R.string.online_server))
                .setMessage(UiText.t(LauncherActivity.this, R.string.online_help))
                .setView(box)
                .setPositiveButton(UiText.t(LauncherActivity.this, R.string.save), (dialog, which) -> {
                    String value = input.getText().toString().trim();
                    if (!GameOptions.validServer(value)) { message(UiText.t(LauncherActivity.this, R.string.online_server), "\"" + value + UiText.t(LauncherActivity.this, R.string.invalid_server_suffix)); return; }
                    GameOptions.prefs(this).edit().putString(GameOptions.ONLINE_SERVER, value).apply();
                    refreshOptions();
                })
                .setNeutralButton(UiText.t(LauncherActivity.this, R.string.restore), (dialog, which) -> {
                    GameOptions.prefs(this).edit().remove(GameOptions.ONLINE_SERVER).apply();
                    refreshOptions();
                })
                .setNegativeButton(UiText.t(LauncherActivity.this, R.string.cancel), null).show();
    }

    private void chooseDriver() {
        List<GpuDrivers.Driver> drivers = GpuDrivers.list(this);
        String[] names = new String[drivers.size() + 1];
        names[0] = UiText.t(LauncherActivity.this, R.string.system_driver);
        int selected = 0;
        for (int i = 0; i < drivers.size(); ++i) {
            names[i + 1] = drivers.get(i).name;
            if (drivers.get(i).id.equals(GpuDrivers.selectedId(this))) selected = i + 1;
        }
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert).setTitle(UiText.t(LauncherActivity.this, R.string.vulkan_driver))
                .setSingleChoiceItems(names, selected, (dialog, which) -> {
                    GpuDrivers.select(this, which == 0 ? "" : drivers.get(which - 1).id);
                    refreshOptions(); dialog.dismiss();
                })
                .setPositiveButton(UiText.t(LauncherActivity.this, R.string.import_zip), (dialog, which) -> startActivityForResult(
                        new Intent(Intent.ACTION_OPEN_DOCUMENT).setType("*/*").addCategory(Intent.CATEGORY_OPENABLE),
                        REQUEST_DRIVER_ZIP))
                .setNeutralButton(UiText.t(LauncherActivity.this, R.string.remove_driver), (dialog, which) -> {
                    GpuDrivers.Driver driver = GpuDrivers.selected(this);
                    if (driver == null) return;
                    try { GpuDrivers.remove(this, driver); refreshOptions(); }
                    catch (java.io.IOException error) { message(UiText.t(LauncherActivity.this, R.string.vulkan_driver), UiText.error(this,error)); }
                })
                .setNegativeButton(UiText.t(LauncherActivity.this, R.string.back), null).show();
    }

    private void probeDriver(boolean thenPlay) {
        playAfterProbe = thenPlay;
        play.setEnabled(false);
        startActivityForResult(new Intent(this, GpuProbeActivity.class), REQUEST_DRIVER_PROBE);
    }

    private void showDriverReport(String report) {
        play.setEnabled(true);
        try {
            org.json.JSONObject gpu = new org.json.JSONObject(report);
            try (java.io.FileOutputStream out=new java.io.FileOutputStream(new File(getFilesDir(),"velocity-gpu-report.json"))) { out.write(report.getBytes(java.nio.charset.StandardCharsets.UTF_8)); }
            boolean failed = gpu.optBoolean("driverLoadFailed") || gpu.has("probeError");
            if (playAfterProbe && !failed && gpu.optBoolean("compatible")) { playAfterProbe = false; launch(); return; }
            playAfterProbe = false;
            String text = failed ? UiText.t(LauncherActivity.this, R.string.driver_failure_prefix) + UiText.code(this,gpu.optString("probeError", getString(R.string.vulkan_start_error)))
                    : "GPU: " + gpu.optString("gpu") + "\nVulkan: " + gpu.optString("vulkan")
                      + "\nDriver: " + gpu.optString("driverName", "—") + " " + gpu.optString("driverInfo", "")
                      + UiText.t(LauncherActivity.this, R.string.bc_label) + (gpu.optBoolean("bcTextures") ? UiText.t(LauncherActivity.this, R.string.bc_gpu) : UiText.t(LauncherActivity.this, R.string.bc_cpu))
                      + "\n" + (gpu.optBoolean("compatible") ? UiText.t(LauncherActivity.this, R.string.compatible)
                              : UiText.t(LauncherActivity.this, R.string.missing_features) + gpu.optJSONArray("missing"));
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(UiText.t(LauncherActivity.this, R.string.probe_title)).setMessage(text).setPositiveButton(UiText.t(LauncherActivity.this, R.string.ok), null)
                    .setNeutralButton(UiText.t(LauncherActivity.this, R.string.use_system_driver), (dialog, which) -> { GpuDrivers.select(this, ""); refreshOptions(); })
                    .show();
        } catch (Exception error) { message(UiText.t(LauncherActivity.this, R.string.probe_title), UiText.t(LauncherActivity.this, R.string.invalid_diagnostic) + UiText.error(this,error)); }
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_SAVE_VELOCITY) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                Uri destination = data.getData();
                worker.execute(() -> {
                    try (java.io.InputStream input = new java.io.FileInputStream(new File(getFilesDir(), "velocity-performance.csv"));
                         java.io.OutputStream output = getContentResolver().openOutputStream(destination,"w")) {
                        if (output == null) throw new java.io.IOException(UiText.t(LauncherActivity.this, R.string.destination_unavailable));
                        byte[] buffer = new byte[16384];
                        for (int n; (n = input.read(buffer)) != -1;) output.write(buffer,0,n);
                        main.post(() -> message("Project Velocity", UiText.t(LauncherActivity.this, R.string.report_saved)));
                    } catch (Exception error) { main.post(() -> message("Project Velocity",UiText.error(this,error))); }
                });
            }
            return;
        }
        if (requestCode == REQUEST_SAVE_REPORT) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) finishReport(data.getData());
            else pendingReport = null;
            return;
        }
        if (requestCode == REQUEST_DRIVER_PROBE) {
            showDriverReport(resultCode == RESULT_OK && data != null ? data.getStringExtra("gpu_report")
                    : "{\"probeError\":\"probe_cancelled\",\"driverLoadFailed\":true}");
        } else if (requestCode == REQUEST_DRIVER_ZIP && resultCode == RESULT_OK && data != null && data.getData() != null) {
            Uri zip = data.getData();
            status.setText(UiText.t(LauncherActivity.this, R.string.importing_driver));
            worker.execute(() -> {
                try {
                    GpuDrivers.Driver driver = GpuDrivers.importZip(this, zip);
                    main.post(() -> {
                        GpuDrivers.select(this, driver.id); refresh();
                        status.setText(UiText.t(LauncherActivity.this, R.string.driver_imported_prefix) + driver.name + UiText.t(LauncherActivity.this, R.string.driver_imported_suffix));
                    });
                } catch (Exception error) {
                    main.post(() -> { refresh(); message(UiText.t(LauncherActivity.this, R.string.vulkan_driver), UiText.t(LauncherActivity.this, R.string.driver_import_failure) + UiText.error(this,error)); });
                }
            });
        }
    }

    // ---- Updates and reports ---------------------------------------------------------------------------

    private void checkUpdates(boolean manual) {
        if (checkingUpdates) return;
        checkingUpdates = true;
        if (optionsList != null) refreshOptions();
        ReleaseUpdates.check(this, manual, result -> {
            checkingUpdates = false;
            if (isFinishing() || isDestroyed()) return;
            refreshOptions();
            if (result.error != null) { if (manual) message(UiText.t(LauncherActivity.this, R.string.updates), result.error); return; }
            if (!ReleaseVersion.newer(result.tag, ReleaseUpdates.installed(this))) {
                if (manual) message(UiText.t(LauncherActivity.this, R.string.updates), UiText.t(LauncherActivity.this, R.string.installed_prefix) + ReleaseUpdates.installed(this)
                        + UiText.t(LauncherActivity.this, R.string.no_update_suffix));
                return;
            }
            android.content.SharedPreferences updates = ReleaseUpdates.prefs(this);
            if (!manual && result.tag.equals(updates.getString("notified_tag", ""))) return;
            updates.edit().putString("notified_tag", result.tag).apply();
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(UiText.t(LauncherActivity.this, R.string.new_version) + result.tag)
                    .setMessage(UiText.t(LauncherActivity.this, R.string.update_message))
                    .setPositiveButton(UiText.t(LauncherActivity.this, R.string.update), (dialog, which) -> openUrl(Uri.parse(result.url)))
                    .setNegativeButton(UiText.t(LauncherActivity.this, R.string.later), null).show();
        });
    }

    /** Builds the report ZIP, lets the player save it, then opens a pre-filled GitHub issue. */
    private void prepareReport(String crash) {
        status.setText(UiText.t(LauncherActivity.this, R.string.preparing_report));
        worker.execute(() -> {
            try {
                File report = Diagnostics.createReport(this);
                main.post(() -> {
                    refresh();
                    pendingReport = report; pendingCrash = crash;
                    new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                            .setTitle(UiText.t(LauncherActivity.this, R.string.report_github))
                            .setMessage(UiText.t(LauncherActivity.this, R.string.report_instructions))
                            .setPositiveButton(UiText.t(LauncherActivity.this, R.string.save_open_github), (dialog, which) -> startActivityForResult(
                                    new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE)
                                            .setType("application/zip").putExtra(Intent.EXTRA_TITLE, report.getName()),
                                    REQUEST_SAVE_REPORT))
                            .setNegativeButton(UiText.t(LauncherActivity.this, R.string.cancel), (dialog, which) -> pendingReport = null).show();
                });
            } catch (Exception error) {
                main.post(() -> { refresh(); message(UiText.t(LauncherActivity.this, R.string.report_problem), UiText.t(LauncherActivity.this, R.string.prepare_report_error) + UiText.error(this,error)); });
            }
        });
    }

    private void finishReport(Uri destination) {
        File report = pendingReport;
        String crash = pendingCrash;
        pendingReport = null;
        if (report == null || !report.isFile()) { message(UiText.t(LauncherActivity.this, R.string.report_problem), UiText.t(LauncherActivity.this, R.string.report_unavailable)); return; }
        worker.execute(() -> {
            try (java.io.InputStream input = new java.io.FileInputStream(report);
                 java.io.OutputStream output = getContentResolver().openOutputStream(destination, "w")) {
                if (output == null) throw new java.io.IOException(UiText.t(LauncherActivity.this, R.string.open_destination_error));
                byte[] buffer = new byte[65536];
                for (int n; (n = input.read(buffer)) != -1;) output.write(buffer, 0, n);
                Uri issue = Diagnostics.issueUri(this, crash, report.getName());
                main.post(() -> openUrl(issue));
            } catch (Exception error) {
                main.post(() -> message(UiText.t(LauncherActivity.this, R.string.report_problem), UiText.t(LauncherActivity.this, R.string.save_zip_error) + UiText.error(this,error)));
            }
        });
    }

    private void openUrl(Uri uri) {
        try { startActivity(new Intent(Intent.ACTION_VIEW, uri)); }
        catch (android.content.ActivityNotFoundException error) { message("GitHub", UiText.t(LauncherActivity.this, R.string.no_browser) + uri); }
    }

    // ---- Launch ----------------------------------------------------------------------------------------

    private void play() {
        if (android.os.Process.is64Bit() && GpuDrivers.selected(this) != null) probeDriver(true);
        else launch();
    }

    private void launch() {
        // Each launch uses a fresh guest machine in the dedicated :game process.
        android.app.ActivityManager manager = (android.app.ActivityManager) getSystemService(ACTIVITY_SERVICE);
        List<android.app.ActivityManager.RunningAppProcessInfo> processes = manager.getRunningAppProcesses();
        if (processes != null) for (android.app.ActivityManager.RunningAppProcessInfo process : processes)
            if (process.uid == android.os.Process.myUid() && process.processName.equals(getPackageName() + ":game"))
                android.os.Process.killProcess(process.pid);
        DisplayMetrics metrics = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        int[] size = GameOptions.renderSize(this, Math.max(metrics.widthPixels, metrics.heightPixels),
                Math.min(metrics.widthPixels, metrics.heightPixels));
        String[] loader = GpuDrivers.loaderArguments(this);
        startActivity(new Intent(this, GameActivity.class)
                .putExtra("language", GameOptions.language(this))
                .putExtra("renderWidth", size[0]).putExtra("renderHeight", size[1])
                .putExtra("frameCap", Integer.parseInt(GameOptions.get(this, GameOptions.FPS)))
                .putExtra("widescreen", GameOptions.get(this, GameOptions.WIDESCREEN).equals("on"))
                .putExtra("minimapTop", GameOptions.get(this, GameOptions.MINIMAP).equals("top"))
                .putExtra("backBuffers", Integer.parseInt(GameOptions.get(this, GameOptions.LATENCY)))
                .putExtra("cpuTextures", GameOptions.get(this, GameOptions.TEXTURES).equals("cpu"))
                .putExtra("onlineServer", GameOptions.onlineServer(this))
                .putExtra("driverLoader", loader));
    }

    // ---- Storage ---------------------------------------------------------------------------------------

    private static File gameRoot() { return new File(Environment.getExternalStorageDirectory(), "nfsu2"); }

    private boolean allowed() {
        return Build.VERSION.SDK_INT >= 30 ? Environment.isExternalStorageManager()
            : checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestAccess() {
        if (Build.VERSION.SDK_INT >= 30)
            startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + getPackageName())));
        else requestPermissions(new String[] {Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE}, 1);
    }

    @Override public void onRequestPermissionsResult(int code, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(code, permissions, results);
        refresh();
    }

    @Override protected void onDestroy() {
        worker.shutdown();
        super.onDestroy();
    }

    // ---- Widgets ---------------------------------------------------------------------------------------

    private void addCheck(String name, boolean ok, boolean required) {
        TextView row = label((ok ? "✓  " : required ? "✗  " : "!  ") + name + (ok || required ? "" : UiText.t(LauncherActivity.this, R.string.optional_suffix)), 13,
                ok ? 0xFF7CD992 : required ? 0xFFFF6B6B : 0xFFFFC857, false);
        row.setPadding(0, dp(2), 0, dp(2));
        checksList.addView(row);
    }

    private View optionRow(String title, String value, Runnable onClick) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(12), dp(8), dp(12), dp(8));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0x168EE03A);
        bg.setCornerRadius(dp(10));
        row.setBackground(bg);
        row.setClickable(true);
        row.setOnClickListener(v -> onClick.run());
        row.addView(label(title, 14, Color.WHITE, false), new LinearLayout.LayoutParams(0, -2, 1f));
        TextView current = label(value + "  ›", 13, ACCENT, true);
        current.setGravity(Gravity.END);
        row.addView(current, new LinearLayout.LayoutParams(0, -2, 1f));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
        lp.topMargin = dp(5);
        row.setLayoutParams(lp);
        return row;
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(12), dp(16), dp(12));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xCC0E1A12);
        bg.setCornerRadius(dp(16));
        bg.setStroke(dp(1), 0x448EE03A);
        card.setBackground(bg);
        return card;
    }

    private Button actionButton(String text, boolean primary) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(primary ? 0xFF0A1C05 : Color.WHITE);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, primary ? 22 : 15);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setLetterSpacing(primary ? .12f : 0f);
        GradientDrawable bg = primary
                ? new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT, new int[] {0xFFB8F25A, 0xFF3FB82A})
                : new GradientDrawable();
        if (!primary) { bg.setColor(0x22FFFFFF); bg.setStroke(dp(1), 0x55FFFFFF); }
        bg.setCornerRadius(dp(14));
        b.setBackground(bg);
        b.setStateListAnimator(null);
        return b;
    }

    private TextView label(String text, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(text);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        if (bold) { t.setTypeface(Typeface.DEFAULT_BOLD); t.setLetterSpacing(.06f); }
        return t;
    }

    private void message(String title, String text) {
        new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle(title).setMessage(text).setPositiveButton(UiText.t(LauncherActivity.this, R.string.ok), null).show();
    }

    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}

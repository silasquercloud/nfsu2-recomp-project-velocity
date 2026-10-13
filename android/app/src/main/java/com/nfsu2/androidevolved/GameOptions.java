package com.nfsu2.androidevolved;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Launcher options. GameActivity reads them when the game process starts and hands them to the native
 * runtime before the guest boots; changing one takes effect on the next JUGAR.
 */
final class GameOptions {
    static final class Option {
        final String key, defaultValue;
        final int title;
        final String[] values;
        final int[] labelIds;
        Option(String key, int title, String defaultValue, String[] values, int[] labels) {
            this.key = key; this.title = title; this.defaultValue = defaultValue; this.values = values; this.labelIds = labels;
        }
        String title(Context context){return context.getString(title);}
        String[] labels(Context context) {
            String[] result=new String[labelIds.length];
            for(int i=0;i<result.length;i++)result[i]=context.getString(labelIds[i]);
            return result;
        }
        String label(Context context, String value) {
            for (int i = 0; i < values.length; i++) if (values[i].equals(value)) return context.getString(labelIds[i]);
            return value;
        }
    }

    static final String RESOLUTION = "resolution", FPS = "fps", WIDESCREEN = "widescreen", LATENCY = "latency",
            TEXTURES = "textures", MINIMAP = "minimap", LANGUAGE = "language";

    static final Option[] ALL = {
            new Option(RESOLUTION, R.string.resolution, "75",
                    new String[] {"100", "75", "50", "720"},
                    new int[]{R.string.resolution_native, R.string.resolution_balanced, R.string.resolution_fast,
                            R.string.resolution_720}),
            new Option(FPS, R.string.fps_limit, "120",
                    new String[] {"30", "60", "90", "120", "0"},
                    new int[]{R.string.fps_battery, R.string.fps_60, R.string.fps_90, R.string.fps_120, R.string.unlimited}),
            new Option(WIDESCREEN, R.string.widescreen, "on",
                    new String[] {"on", "off"},
                    new int[]{R.string.widescreen_fixed, R.string.widescreen_original}),
            new Option(MINIMAP, R.string.minimap, "top",
                    new String[] {"top", "bottom"},
                    new int[]{R.string.minimap_top, R.string.minimap_bottom}),
            new Option(LATENCY, R.string.cpu_gpu_sync, "2",
                    new String[] {"2", "1"},
                    new int[]{R.string.sync_parallel, R.string.sync_latency}),
            new Option(TEXTURES, R.string.compressed_textures, "auto",
                    new String[] {"auto", "cpu"},
                    new int[]{R.string.textures_auto, R.string.textures_cpu}),
    };

    static final String PROFILING="velocity_profiling", SHADER_CACHE="velocity_shader_cache", FRAME_PACING="velocity_frame_pacing";
    static final String HUD="velocity_hud", ORIENTATION="screen_orientation";
    static final String ASYNC="shader_async", WORKERS="shader_workers", PREWARM="shader_prewarm", PROFILE="graphics_profile";
    static final Option[] VELOCITY = {
        new Option(PROFILE,R.string.v5_graphics_profile,"balanced",new String[]{"compatibility","balanced","performance","experimental"},new int[]{R.string.v5_universal_compatibility_one_auto_worker,R.string.v5_balanced_conservative_auto_workers,R.string.v5_performance_parallel_presentation,R.string.v5_experimental_up_to_four_auto_workers}),
        new Option(ASYNC,R.string.v5_async_shader_compilation,"auto",new String[]{"auto","on","off"},new int[]{R.string.v5_auto_conservative_worker_budget,R.string.v5_on_required_pipelines_prioritized,R.string.v5_off_synchronous_fallback}),
        new Option(WORKERS,R.string.v5_shader_workers,"0",new String[]{"0","1","2","3","4"},new int[]{R.string.v5_auto,R.string.v5_1,R.string.v5_2,R.string.v5_3,R.string.v5_4}),
        new Option(PREWARM,R.string.v5_pipeline_prewarming,"on",new String[]{"on","off"},new int[]{R.string.v5_on_known_states_as_shaders_load,R.string.v5_off}),
        new Option(HUD,R.string.v5_performance_hud,"compact",new String[]{"off","compact","detailed"},new int[]{R.string.v5_off_19,R.string.v5_compact_fps_gpu_vulkan,R.string.v5_detailed_timings_pipelines_memory}),
        new Option(ORIENTATION,R.string.v5_screen_orientation,"auto",new String[]{"auto","normal","reverse"},new int[]{R.string.v5_automatic_both_landscape_directions,R.string.v5_landscape,R.string.v5_reverse_landscape}),
        new Option(FRAME_PACING,R.string.frame_pacing,"default",new String[]{"default","experimental"},new int[]{R.string.pacing_default,R.string.pacing_experimental}),
        new Option(PROFILING,R.string.profiling,"off",new String[]{"off","on"},new int[]{R.string.profiling_off,R.string.profiling_on}),
        new Option(SHADER_CACHE,R.string.shader_cache,"on",new String[]{"off","on"},new int[]{R.string.cache_off,R.string.cache_on}),
    };

    private static final String PREFS = "nfsu2_game";

    private GameOptions() {}

    static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static Option find(String key) {
        for (Option o : ALL) if (o.key.equals(key)) return o;
        for (Option o : VELOCITY) if (o.key.equals(key)) return o;
        throw new IllegalArgumentException(key);
    }

    static String get(Context context, String key) {
        Option o = find(key);
        String value = prefs(context).getString(key, o.defaultValue);
        for (String allowed : o.values) if (allowed.equals(value)) return value;
        return o.defaultValue;
    }

    static void set(Context context, String key, String value) {
        prefs(context).edit().putString(key, value).apply();
    }

    static final String ONLINE_SERVER = "online_server", DEFAULT_ONLINE_SERVER = "eahub.eu";

    /** EA Nation replacement server: the game's *.ea.com hosts resolve to this name or IPv4 address. */
    static String onlineServer(Context context) {
        String value = prefs(context).getString(ONLINE_SERVER, DEFAULT_ONLINE_SERVER);
        return validServer(value) ? value : DEFAULT_ONLINE_SERVER;
    }

    static boolean validServer(String value) {
        return value != null && value.length() <= 253 && value.matches("[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*");
    }

    /** The game's language folder name (LANGUAGES/<file>.bin) chosen in the launcher. */
    static String language(Context context) {
        return prefs(context).getString(LANGUAGE, "Spanish");
    }

    /** Render size for the selected resolution on a display of width × height (landscape). */
    static int[] renderSize(Context context, int width, int height) {
        String value = get(context, RESOLUTION);
        if (value.equals("720")) return new int[] {1280, 720};
        float scale = Integer.parseInt(value) / 100f;
        return new int[] {Math.round(width * scale / 2f) * 2, Math.round(height * scale / 2f) * 2};
    }
}

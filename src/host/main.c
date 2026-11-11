/* Host for the Project DIVA recompilation.
 *
 * Loads the game's executable into guest memory, opens a window and calls
 * module_start. The game's own threads do the rest.
 *
 *   diva.exe <elf> <disc dir> <data dir> [seconds]
 *
 * <disc dir> holds PSP_GAME, <data dir> receives what the game writes (saves go
 * to <data dir>/ms). With [seconds] the run ends itself after printing a report
 * and capturing the screen to <data dir>/last_frame.ppm; 0 runs until the window
 * is closed.
 *
 * The report runs on a thread of its own. The game may never return from
 * module_start or may be stuck inside it, and that is when the report matters. */

#include "audio_out.h"
#include "hooks.h"
#include "extras.h"
#include "language.h"
#include "letters.h"
#include "controls.h"
#include "system_dialogs.h"
#include "language_screen.h"
#include "mods.h"
#include "module_skins.h"
#include "note_skins.h"
#include "screenshot.h"
#include "presence.h"

/* Project DIVA Recomp's application on Discord. */
#define DISCORD_APPLICATION_ID "1568866146696958012"
#include "options.h"
#include "settings.h"
#include "translation.h"
#include "lifesearch.h"
#include "watch.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>
#include <psprecomp/module.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <direct.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void psp_recomp_register(void);   /* in the generated diva_register.c */

/* The game's image is read straight into RAM. */
#define MODULE_SPAN      0x01800000u
#define USER_MEMORY_END  0x0A000000u
#define START_STACK_SIZE 0x00040000u

/* Sound the game may have queued ahead of the card; see audio_out.c. */
#define AUDIO_LEAD_US 46000u

/* The PSP's refresh rate, and so how the game was written to run. */
#define ORIGINAL_REFRESH_HZ 59.94

static char     g_data_dir[1024];
static unsigned g_seconds;
static volatile LONG g_reporting;
static ULONGLONG g_started_ms;
static int g_mod_files;

static void report(const char *why) {
    /* Whichever failure gets here first owns the report. */
    if (InterlockedExchange(&g_reporting, 1)) {
        Sleep(INFINITE);
    }
    watch_dump();
    fprintf(stderr, "\n==== report: %s ====\n", why);
    fprintf(stderr, "frames %llu  ge commands %llu  ge vertices %llu  ge pixels %llu\n",
            (unsigned long long)psp_display_vblanks(),
            (unsigned long long)psp_ge_command_count(),
            (unsigned long long)psp_ge_vertex_count(),
            (unsigned long long)psp_ge_pixels());
    const double run_us = (double)(GetTickCount64() - g_started_ms) * 1000.0;
    fprintf(stderr, "renderer %s\n", psp_render_current()->name);
    fprintf(stderr, "of %.0f s: %.1f presented fps, GE thread busy %.1f%%, sound gaps %llu\n",
            run_us / 1e6, (double)psp_display_flips() * 1e6 / run_us,
            100.0 * (double)psp_ge_busy_us() / run_us,
            (unsigned long long)audio_out_underruns());
    double queued, at_card;
    audio_out_waits(&queued, &at_card);
    fprintf(stderr, "sound waits %.0f ms in its queue and %.0f ms at the card, on average\n", queued, at_card);
    fprintf(stderr, "framebuffer 0x%08X  audio blocks %llu  bytes read %llu  ge callbacks %llu\n",
            psp_display_framebuffer(),
            (unsigned long long)psp_audio_blocks(),
            (unsigned long long)psp_io_bytes_read(),
            (unsigned long long)psp_ge_callbacks_delivered());
    fprintf(stderr, "threads created %llu started %llu  switches %llu  parks %llu  vtime %llu us\n",
            (unsigned long long)psp_threadman_created(),
            (unsigned long long)psp_threadman_started(),
            (unsigned long long)psp_threadman_switches(),
            (unsigned long long)psp_threadman_parks(),
            (unsigned long long)psp_threadman_vtime_us());
    fprintf(stderr, "bad memory accesses %llu  dispatch misses %llu  dispatch calls %llu\n",
            (unsigned long long)psp_mem_bad_access,
            (unsigned long long)psp_dispatch_misses(),
            (unsigned long long)psp_dispatch_calls());
    fprintf(stderr, "unimplemented firmware calls %llu  free user memory %u\n",
            (unsigned long long)psp_hle_unimplemented_calls(), psp_sysmem_free());
    psp_threadman_dump(stderr);
    psp_threadman_dump_times(stderr);
    psp_display_dump_frames(stderr);
    psp_sched_dump(stderr, 90);
    {
        uint64_t decoded = 0, decode_us = 0, worst = 0;
        psp_render_gl_texture_stats(&decoded, &decode_us, &worst);
        fprintf(stderr, "textures decoded %llu in %llu ms, slowest %llu ms\n", (unsigned long long)decoded,
                (unsigned long long)(decode_us / 1000), (unsigned long long)(worst / 1000));
    }
    psp_ge_dump_stats(stderr);
    psp_hle_dump_counts(stderr, 80);
    psp_hle_dump_recent(stderr);
    psp_trace_dump();

    char path[1200];
    snprintf(path, sizeof path, "%s/last_frame.ppm", g_data_dir);
    if (psp_display_capture(path) == 0) fprintf(stderr, "captured %s\n", path);
    else fprintf(stderr, "no frame to capture\n");
    fflush(stderr);
    fflush(stdout);
}

/* What is worth telling the player of what the system's save screens would have said, in the
 * language of the game. Loading says nothing when it works. Having no save says nothing either:
 * the game asks at every start, and a new player has no use for hearing it. */
static const char *in_language(const char *english, const char *german, const char *japanese) {
    const int ui = language_ui();
    return ui == UI_GERMAN ? german : ui == UI_JAPANESE ? japanese : english;
}

static void on_savedata_event(int event) {
    switch (event) {
    case PSP_SAVEDATA_SAVED:
        psp_render_gl_show_message(in_language("Game saved", "Spiel gespeichert", "\xE3\x82\xBB\xE3\x83\xBC\xE3\x83\x96\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F"));
        break;
    case PSP_SAVEDATA_DELETED:
        psp_render_gl_show_message(in_language("Save data deleted", "Spielstand gel\xC3\xB6scht", "\xE3\x82\xBB\xE3\x83\xBC\xE3\x83\x96\xE3\x83\x87\xE3\x83\xBC\xE3\x82\xBF\xE3\x82\x92\xE5\x89\x8A\xE9\x99\xA4\xE3\x81\x97\xE3\x81\xBE\xE3\x81\x97\xE3\x81\x9F"));
        break;
    case PSP_SAVEDATA_FAILED:
        psp_render_gl_show_message(in_language("Could not save", "Speichern fehlgeschlagen", "\xE3\x82\xBB\xE3\x83\xBC\xE3\x83\x96\xE3\x81\xA7\xE3\x81\x8D\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x81\xA7\xE3\x81\x97\xE3\x81\x9F"));
        break;
    case PSP_SAVEDATA_UNREADABLE:
        /* Playing on would start from nothing, and saving would then overwrite it. */
        psp_render_gl_show_held_message(in_language("Your save data could not be read. Close the game before saving. Press a button.",
                                                     "Spielstand nicht lesbar. Vor dem Speichern das Spiel schlie\xC3\x9F" "en. Taste dr\xC3\xBC" "cken.",
                                                     "\xE3\x82\xBB\xE3\x83\xBC\xE3\x83\x96\xE3\x83\x87\xE3\x83\xBC\xE3\x82\xBF\xE3\x82\x92\xE8\xAA\xAD\xE3\x82\x81\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93"));
        break;
    default:
        break;
    }
}

static void on_dispatch_miss(uint32_t addr) {
    char why[64];
    snprintf(why, sizeof why, "jump to unknown address 0x%08X", addr);
    report(why);
    _Exit(3);
}

static LONG WINAPI on_crash(EXCEPTION_POINTERS *e) {
    char why[128];
    snprintf(why, sizeof why, "host exception 0x%08lX at %p",
             e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    report(why);
    _Exit(4);
}

static const char *g_disc_dir;
static int g_ask_language, g_ask_at_title;

/* The game waits here, in the middle of reading the title screen's picture, until there is an answer. */
static void at_title(void) {
    if (!g_ask_at_title) return;
    if (g_ask_language) language_apply(language_screen_run(g_options.language, g_disc_dir));
    /* DIVA_ASK_CONFIRM, a test switch, asks again even with a choice saved. */
    if (g_options.confirm_cross < 0 || getenv("DIVA_ASK_CONFIRM")) controls_ask_confirm();
    options_save(g_data_dir);
}

static unsigned __stdcall watchdog(void *arg) {
    (void)arg;
    const ULONGLONG start = GetTickCount64();
    ULONGLONG next_beat = start + 5000;
    for (;;) {
        Sleep(100);
        const ULONGLONG now = GetTickCount64();
        if (psp_window_closed()) { report("window closed"); _Exit(0); }
        if (psp_exit_requested()) { report("the game asked to exit"); _Exit(0); }
        if (g_seconds && now - start >= g_seconds * 1000ull) { report("time limit"); _Exit(0); }
        if (now >= next_beat) {
            next_beat += 5000;
            watch_dump();
            static uint64_t last_trims;
            const uint64_t trims = audio_out_trims();
            if (trims != last_trims) {
                fprintf(stderr, "[%3llus] sound trimmed %llu times\n", (unsigned long long)((now - start) / 1000),
                        (unsigned long long)(trims - last_trims));
                last_trims = trims;
            }
            static uint64_t last_flips, last_busy, last_gaps;
            const uint64_t flips = psp_display_flips(), busy = psp_ge_busy_us(), gaps = audio_out_underruns();
            static uint64_t last_life, last_asleep, last_sema, last_queue, last_decoded, last_decode_us;
            uint64_t life = 0, asleep = 0, sema = 0, queue = 0, decoded = 0, decode_us = 0, worst = 0;
            psp_threadman_times("user_main", &life, &asleep, &sema, &queue);
            psp_render_gl_texture_stats(&decoded, &decode_us, &worst);
            const double span = life > last_life ? (double)(life - last_life) : 1.0;
            fprintf(stderr, "[%3llus] %2llu fps  GE %2llu%%  gaps %llu  main run %2.0f%% sema %2.0f%% queue %2.0f%%"
                            "  tex %llu in %llu ms\n",
                    (unsigned long long)((now - start) / 1000),
                    (unsigned long long)((flips - last_flips) / 5),
                    (unsigned long long)((busy - last_busy) / 50000),
                    (unsigned long long)(gaps - last_gaps),
                    100.0 * (span - (double)(asleep - last_asleep) - (double)(queue - last_queue)) / span,
                    100.0 * (double)(sema - last_sema) / span,
                    100.0 * (double)(queue - last_queue) / span,
                    (unsigned long long)(decoded - last_decoded),
                    (unsigned long long)((decode_us - last_decode_us) / 1000));
            last_life = life;
            last_asleep = asleep;
            last_sema = sema;
            last_queue = queue;
            last_decoded = decoded;
            last_decode_us = decode_us;
            last_flips = flips;
            last_busy = busy;
            last_gaps = gaps;
            fprintf(stderr, "       frames %llu  ge %llu  read %llu  switches %llu  bad %llu\n",
                    (unsigned long long)psp_display_vblanks(),
                    (unsigned long long)psp_ge_command_count(),
                    (unsigned long long)psp_io_bytes_read(),
                    (unsigned long long)psp_threadman_switches(),
                    (unsigned long long)psp_mem_bad_access);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <elf> <disc dir> <data dir> [seconds]\n", argv[0]);
        return 2;
    }
    const char *elf = argv[1];
    snprintf(g_data_dir, sizeof g_data_dir, "%s", argv[3]);
    g_seconds = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 10) : 0;

    setvbuf(stderr, NULL, _IONBF, 0);
    settings_load_dev(g_data_dir);
    settings_load(g_data_dir);
    SetUnhandledExceptionFilter(on_crash);

    char ms[1100];
    snprintf(ms, sizeof ms, "%s/ms", g_data_dir);
    _mkdir(g_data_dir);
    _mkdir(ms);

    if (psp_mem_init() != 0) { fprintf(stderr, "memory init failed\n"); return 1; }
    psp_module m;
    if (psp_module_load(elf, MODULE_SPAN, &m) != 0) return 1;

    psp_cpu_reset();
    psp_hle_init();
    psp_threadman_set_realtime(1);
    psp_set_miss_handler(on_dispatch_miss);
    /* DIVA_FPS is for tests: the game's speed follows this rate. */
    const char *fps = getenv("DIVA_FPS");
    psp_display_set_refresh_rate(fps ? strtod(fps, NULL) : ORIGINAL_REFRESH_HZ);
    /* The name other copies see ad hoc; the computer's name when not given. */
    if (getenv("DIVA_NICKNAME")) psp_netadhoc_set_nickname(getenv("DIVA_NICKNAME"));

    /* Everything from the end of .bss up is the game's to allocate. */
    psp_sysmem_set_range((m.mem_end + 0xFFFu) & ~0xFFFu, USER_MEMORY_END);
    const uint32_t stack = psp_sysmem_alloc(START_STACK_SIZE, 1);
    if (!stack) { fprintf(stderr, "no memory for the start stack\n"); return 1; }
    m.stack_top = stack + START_STACK_SIZE - 16;

    psp_io_set_root(g_data_dir);
    psp_io_set_disc_root(argv[2]);

    psp_recomp_register();

    fprintf(stderr, "module 0x%08X..0x%08X  entry 0x%08X  gp 0x%08X  %d constructors  %u functions\n",
            m.base, m.mem_end, m.entry, m.gp, m.nctors, psp_dispatch_count());
    fprintf(stderr, "user memory free %u  disc %s  data %s\n", psp_sysmem_free(), argv[2], g_data_dir);

    if (psp_window_start("Project DIVA Recomp", 2) != 0) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }

    hooks_start();
    int chosen = 0;
    if (g_settings.translations || g_settings.extras) chosen = options_load(g_data_dir);
    if (g_settings.extras && g_options.ask_language >= 0) g_settings.language_prompt = g_options.ask_language;
    if (g_settings.extras) {
        if (g_options.render_scale && !getenv("DIVA_RENDER_SCALE")) g_settings.render_scale = g_options.render_scale;
        if (g_options.smoothing >= 0) g_settings.present_linear = g_options.smoothing;
        psp_window_set_mode(g_options.display_mode);
        controls_start(argv[2]);
        system_dialogs_start();
    }
    language_start(g_data_dir);
    if (g_settings.translations) {
        const char *lang_dir = getenv("DIVA_LANG_DIR");
        translation_start(lang_dir ? lang_dir : "lang", g_data_dir);
        language_apply(g_options.language);
        /* The picture the title screen starts with: the intros have been and gone by then. */
        /* DIVA_SKIP_LANGUAGE, a test switch, goes on with the language saved, for runs left alone. */
        g_ask_language = (g_settings.language_prompt || !chosen) && !(chosen && getenv("DIVA_SKIP_LANGUAGE"));
        if (g_ask_language || (g_settings.extras && (g_options.confirm_cross < 0 || getenv("DIVA_ASK_CONFIRM")))) {
            g_disc_dir = argv[2];
            g_ask_at_title = 1;
        }
    }
    if (g_settings.extras) {
        char romaji[1100];
        snprintf(romaji, sizeof romaji, "%s\\..\\lang\\romaji", g_data_dir);
        translation_romaji_start(romaji);
        extras_start(g_data_dir);
    }
    if (g_ask_at_title) mods_on_first_read("title.png", at_title);

    /* By default beside the game's data. */
    const char *mods_dir = getenv("DIVA_MODS");
    if (g_settings.mods) {
        char default_mods[1100];
        snprintf(default_mods, sizeof default_mods, "%s\\..\\mods", g_data_dir);
        g_mod_files = mods_init(mods_dir ? mods_dir : default_mods);
        module_skins_scan(mods_dir ? mods_dir : default_mods, argv[2], g_data_dir);
        /* Note skins are more choices in Options, so they come with the extras. */
        if (g_settings.extras && note_skins_scan(mods_dir ? mods_dir : default_mods, g_data_dir) && g_options.note_skin[0]) {
            const int skin = note_skins_find(g_options.note_skin);
            if (skin >= 0) g_options.note_style = OPTIONS_NOTES_COUNT + skin;
        }
    }

    if (strcmp(g_settings.renderer, "software") != 0) {
        psp_render_gl_set_scale(g_settings.render_scale);
        /* The edition with no extras keeps to the PSP's screen. */
        if (g_settings.extras || getenv("DIVA_WIDE")) {
            psp_render_gl_set_wide(1);
            psp_render_gl_set_wide_view(g_options.wide_view);
            psp_render_gl_set_blur(g_options.blur);
            /* title.png: the title screen's logo and words sit on its 3D scene, which is not moved. */
            psp_render_gl_keep_in_place(0x6EDA8883u);
        }
        psp_render_gl_set_present_filter(g_settings.present_linear);
        if (g_settings.extras) psp_render_gl_set_background_dim(g_options.background_dim);
        if (g_settings.extras) language_notes_changed();
        /* What the renderer learns, kept for the next game: where the footer's pieces go. */
        psp_render_gl_set_state_dir(g_data_dir);
        /* DIVA_SAVE_TEXTURES saves what is still at the game's own resolution. */
        if (getenv("DIVA_SAVE_TEXTURES")) {
            char dump_dir[1100];
            snprintf(dump_dir, sizeof dump_dir, "%s\\texture_cache", g_data_dir);
            _mkdir(dump_dir);
            psp_render_gl_set_dump_dir(dump_dir);
        }
        if (g_settings.hd_textures) {
            char hd_dir[1100];
            snprintf(hd_dir, sizeof hd_dir, "%s\\..\\textures_hd", g_data_dir);
            psp_render_gl_set_replacement_dir(hd_dir);
            letters_load(hd_dir);
        }
        const char *overlay = getenv("DIVA_SHOW_FPS");
        if (overlay) psp_render_gl_set_overlay(overlay[0] && overlay[0] != '0');
        else psp_render_gl_set_overlay(g_settings.extras && g_options.show_fps);
        if (psp_render_select("gl") == 0) {
            psp_window_set_external(1);
            /* The game's own screenshots, kept in the player's Pictures folder. */
            psp_utility_set_screenshot_handler(screenshot_take);
        }
    }
    /* What the player is doing, on their Discord profile. The application's id is public: it is the
     * name Discord shows, not a secret. DIVA_DISCORD_ID in dev.ini can name another for testing. */
    const char *discord_id = getenv("DIVA_DISCORD_ID");
    presence_start(discord_id ? discord_id : DISCORD_APPLICATION_ID);
    psp_ge_set_async(1);

    g_started_ms = GetTickCount64();
    audio_out_start();
    if (g_settings.save_messages) psp_savedata_set_event_handler(on_savedata_event);
    if (g_mod_files) {
        char line[64];
        snprintf(line, sizeof line, "Mods: %d", g_mod_files);
        psp_render_gl_show_message(line);
    }
    psp_audio_set_sink(audio_out_submit);
    psp_audio_set_lead(AUDIO_LEAD_US);
    _beginthreadex(NULL, 0, watchdog, NULL, 0, NULL);

    watch_start();
    lifesearch_start();
    const uint32_t result = psp_module_start(&m);
    fprintf(stderr, "module_start returned 0x%08X\n", result);

    /* The game's threads keep running. This one is the vblank interrupt. The
     * watchdog ends the process.
     *
     * The game wakes on the interrupt, works, then waits for the next refresh. With
     * both at the same moment it misses that refresh every frame and runs at 30, so
     * how long the interrupt comes after a refresh is a setting. */
    for (;;) {
        psp_display_wait_vblank();
        if (g_settings.interrupt_offset_us) psp_threadman_sleep_us(g_settings.interrupt_offset_us);
        psp_sched_note("vblank interrupt", NULL, 0);
        psp_intr_tick();
    }
}

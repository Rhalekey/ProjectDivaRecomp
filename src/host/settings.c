/* Settings: what an edition of the game is.
 *
 * Both editions are this same program, and the differences are all here. Settings
 * are read in layers, each overriding the last:
 *
 *   1. the defaults below, which are the recommended edition
 *   2. the edition's file, from profiles\ (DIVA_PROFILE names it)
 *   3. settings.ini beside the player's data, for their own changes
 *   4. environment variables, for testing
 *
 * An unknown key is reported and ignored, so an old file keeps working.
 */

#include "settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

diva_settings g_settings = {
    /* renderer */ "gl",
    /* render_scale */ 3,
    /* present_linear */ 1,
    /* interrupt_offset_us */ 8000,
    /* save_messages */ 1,
    /* mods */ 1,
    /* translations */ 1,
    /* language_prompt */ 1,
    /* hd_textures */ 1,
    /* extras */ 1,
};

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

static void apply(const char *key, const char *value, const char *where) {
    if (!strcmp(key, "renderer")) {
        snprintf(g_settings.renderer, sizeof g_settings.renderer, "%s", value);
    } else if (!strcmp(key, "render_scale")) {
        g_settings.render_scale = atoi(value);
    } else if (!strcmp(key, "present_filter")) {
        g_settings.present_linear = !strcmp(value, "linear");
    } else if (!strcmp(key, "vblank_interrupt_offset_us")) {
        g_settings.interrupt_offset_us = (uint32_t)strtoul(value, NULL, 10);
    } else if (!strcmp(key, "save_messages")) {
        g_settings.save_messages = atoi(value) != 0;
    } else if (!strcmp(key, "mods")) {
        g_settings.mods = atoi(value) != 0;
    } else if (!strcmp(key, "hd_textures")) {
        g_settings.hd_textures = atoi(value) != 0;
    } else if (!strcmp(key, "translations")) {
        g_settings.translations = atoi(value) != 0;
    } else if (!strcmp(key, "language_prompt")) {
        g_settings.language_prompt = atoi(value) != 0;
    } else if (!strcmp(key, "extras")) {
        g_settings.extras = atoi(value) != 0;
    } else {
        fprintf(stderr, "settings: %s: unknown key \"%s\"\n", where, key);
    }
}

/* key = value lines. A '#' or ';' starts a comment. */
static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *comment = strpbrk(line, "#;");
        if (comment) *comment = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        apply(trim(line), trim(eq + 1), path);
    }
    fclose(f);
    fprintf(stderr, "settings: read %s\n", path);
    return 1;
}

void settings_load(const char *data_dir) {
    const char *profile = getenv("DIVA_PROFILE");
    if (profile && !load_file(profile)) fprintf(stderr, "settings: cannot read %s\n", profile);

    char own[1100];
    snprintf(own, sizeof own, "%s\\..\\settings.ini", data_dir);
    load_file(own);

    const char *v;
    if ((v = getenv("DIVA_RENDERER")) != NULL) apply("renderer", v, "environment");
    if ((v = getenv("DIVA_RENDER_SCALE")) != NULL) apply("render_scale", v, "environment");
    if ((v = getenv("DIVA_INTR_OFFSET_US")) != NULL) apply("vblank_interrupt_offset_us", v, "environment");
    if (getenv("DIVA_NO_MODS")) g_settings.mods = 0;

    fprintf(stderr, "settings: renderer %s, scale %d, %s, vblank interrupt +%u us, save messages %d, mods %d, translations %d, language prompt %d\n",
            g_settings.renderer, g_settings.render_scale, g_settings.present_linear ? "linear" : "nearest",
            g_settings.interrupt_offset_us, g_settings.save_messages, g_settings.mods, g_settings.translations,
            g_settings.language_prompt);
}

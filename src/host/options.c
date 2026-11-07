/* The player's choices, in a folder beside the game's saves.
 *
 * Not inside the game's own save: the game takes that folder existing to mean there
 * is a save to load, and its file has a fixed format that a real PSP could still read.
 */

#include "options.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>

diva_options g_options = {
    "ja", "japanese", "japanese", -1, 1, -1,
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    0, 0, -1, 2,
};

static void folder(const char *data_dir, char *out, size_t cap) {
    snprintf(out, cap, "%s\\ms\\PSP\\SAVEDATA\\ULJM05472RECOMP", data_dir);
}

int options_load(const char *data_dir) {
    char path[1200];
    folder(data_dir, path, sizeof path);
    strcat(path, "\\OPTIONS.INI");
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int chosen = 0;
    char line[128], value[32];
    int number, button;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "language = %31s", value) == 1) {
            snprintf(g_options.language, sizeof g_options.language, "%s", value);
            chosen = 1;
        } else if (sscanf(line, "song_names = %31s", value) == 1) {
            snprintf(g_options.song_names, sizeof g_options.song_names, "%s", value);
        } else if (sscanf(line, "lyrics = %31s", value) == 1) {
            snprintf(g_options.lyrics, sizeof g_options.lyrics, "%s", value);
        } else if (sscanf(line, "ask_language = %d", &number) == 1) {
            g_options.ask_language = number != 0;
        } else if (sscanf(line, "show_fps = %d", &number) == 1) {
            g_options.show_fps = number != 0;
        } else if (sscanf(line, "display_mode = %d", &number) == 1) {
            if (number >= 0 && number < PSP_WINDOW_MODE_COUNT) g_options.display_mode = number;
        } else if (sscanf(line, "render_scale = %d", &number) == 1) {
            if (number >= 1 && number <= 8) g_options.render_scale = number;
        } else if (sscanf(line, "smoothing = %d", &number) == 1) {
            g_options.smoothing = number != 0;
        } else if (sscanf(line, "wide_view = %d", &number) == 1) {
            if (number >= 0 && number <= 2) g_options.wide_view = number;
        } else if (sscanf(line, "confirm_cross = %d", &number) == 1) {
            g_options.confirm_cross = number != 0;
        } else if (sscanf(line, "key %d = %d", &button, &number) == 2 && button >= 0 && button < PSP_BIND_COUNT) {
            g_options.keys[button] = number;
        } else if (sscanf(line, "pad %d = %d", &button, &number) == 2 && button >= 0 && button < PSP_BIND_COUNT) {
            g_options.pads[button] = number;
        }
    }
    fclose(f);
    return chosen;
}

void options_save(const char *data_dir) {
    char path[1200];
    snprintf(path, sizeof path, "%s\\ms", data_dir);
    _mkdir(path);
    snprintf(path, sizeof path, "%s\\ms\\PSP", data_dir);
    _mkdir(path);
    snprintf(path, sizeof path, "%s\\ms\\PSP\\SAVEDATA", data_dir);
    _mkdir(path);
    folder(data_dir, path, sizeof path);
    _mkdir(path);
    strcat(path, "\\OPTIONS.INI");
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "options: cannot write %s\n", path);
        return;
    }
    fprintf(f, "language = %s\nsong_names = %s\nlyrics = %s\nshow_fps = %d\n", g_options.language, g_options.song_names,
            g_options.lyrics, g_options.show_fps);
    fprintf(f, "display_mode = %d\nwide_view = %d\n", g_options.display_mode, g_options.wide_view);
    if (g_options.render_scale) fprintf(f, "render_scale = %d\n", g_options.render_scale);
    if (g_options.smoothing >= 0) fprintf(f, "smoothing = %d\n", g_options.smoothing);
    if (g_options.ask_language >= 0) fprintf(f, "ask_language = %d\n", g_options.ask_language);
    if (g_options.confirm_cross >= 0) fprintf(f, "confirm_cross = %d\n", g_options.confirm_cross);
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        if (g_options.keys[i] >= 0) fprintf(f, "key %d = %d\n", i, g_options.keys[i]);
        if (g_options.pads[i] >= 0) fprintf(f, "pad %d = %d\n", i, g_options.pads[i]);
    }
    fclose(f);
}

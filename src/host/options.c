/* The player's choices, in a folder beside the game's saves.
 *
 * Not inside the game's own save: the game takes that folder existing to mean there
 * is a save to load, and its file has a fixed format that a real PSP could still read.
 */

#include "options.h"

#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

diva_options g_options = {
    "ja", "japanese", "japanese", -1, 1, -1,
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    0, 0, -1, 2,
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    0,
    0,
    1,
    OPTIONS_STYLE_PLAYSTATION,
    OPTIONS_NOTES_PLAYSTATION,
    "",
    "",
    0,
    0,
    {40, 41},
    OPTIONS_HIT_BOTH,
    0,
};

int options_face_letter(int face) {
    /* Below, right, left, above. */
    static const char XBOX[4] = {'A', 'B', 'X', 'Y'}, NINTENDO[4] = {'B', 'A', 'Y', 'X'};
    if (face < 0 || face > 3) return 0;
    if (g_options.button_style == OPTIONS_STYLE_XBOX) return XBOX[face];
    if (g_options.button_style == OPTIONS_STYLE_NINTENDO) return NINTENDO[face];
    return 0;
}

int options_letter_for(int cross) {
    return options_face_letter(cross ? OPTIONS_FACE_CROSS : OPTIONS_FACE_CIRCLE);
}

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
    char line[128], value[32], skin[48];
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
        } else if (sscanf(line, "button_style = %d", &number) == 1) {
            if (number >= 0 && number < OPTIONS_STYLE_COUNT) g_options.button_style = number;
        } else if (sscanf(line, "rin_module = %d", &number) == 1) {
            if (number == 40 || number == 48) g_options.singer_module[0] = number;
        } else if (sscanf(line, "len_module = %d", &number) == 1) {
            if (number == 41 || number == 49) g_options.singer_module[1] = number;
        } else if (sscanf(line, "restart_pad = %d", &number) == 1) {
            g_options.restart_pad = number;
        } else if (sscanf(line, "restart_key = %d", &number) == 1) {
            if (number >= 0 && number < 256) g_options.restart_key = number;
        } else if (sscanf(line, "dpad_notes = %d", &number) == 1) {
            if (number >= OPTIONS_HIT_OFF && number <= OPTIONS_HIT_BOTH) g_options.dpad_notes = number;
        } else if (sscanf(line, "free_singers = %d", &number) == 1) {
            g_options.free_singers = number != 0;
        } else if (sscanf(line, "keep_vocal = %d", &number) == 1) {
            g_options.keep_vocal = number != 0;
        } else if (sscanf(line, "module_skin = %47[^\r\n]", skin) == 1) {
            snprintf(g_options.module_skin, sizeof g_options.module_skin, "%s", skin);
        } else if (sscanf(line, "note_skin = %47[^\r\n]", skin) == 1) {
            snprintf(g_options.note_skin, sizeof g_options.note_skin, "%s", skin);
        } else if (sscanf(line, "note_style = %d", &number) == 1) {
            if (number >= 0 && number < OPTIONS_NOTES_COUNT) g_options.note_style = number;
        } else if (sscanf(line, "blur = %d", &number) == 1) {
            g_options.blur = number != 0;
        } else if (sscanf(line, "background_dim = %d", &number) == 1) {
            if (number >= 0 && number <= 100) g_options.background_dim = number;
        } else if (sscanf(line, "note_timing = %d", &number) == 1) {
            if (number >= OPTIONS_TIMING_LEAST && number <= OPTIONS_TIMING_MOST) g_options.note_timing = number;
        } else if (sscanf(line, "confirm_cross = %d", &number) == 1) {
            g_options.confirm_cross = number != 0;
        } else if (sscanf(line, "key %d = %d", &button, &number) == 2 && button >= 0 && button < PSP_BIND_COUNT) {
            g_options.keys[button] = number;
        } else if (sscanf(line, "key2 %d = %d", &button, &number) == 2 && button >= 0 && button < PSP_BIND_COUNT) {
            g_options.keys2[button] = number;
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
    /* Written beside it, then put in its place: a game closed halfway through leaves the old one. */
    char writing[1220];
    FILE *f = replace_file_begin(path, writing, sizeof writing);
    if (!f) {
        fprintf(stderr, "options: cannot write %s\n", path);
        return;
    }
    fprintf(f, "language = %s\nsong_names = %s\nlyrics = %s\nshow_fps = %d\n", g_options.language, g_options.song_names,
            g_options.lyrics, g_options.show_fps);
    fprintf(f, "display_mode = %d\nwide_view = %d\nbackground_dim = %d\nnote_timing = %d\n", g_options.display_mode, g_options.wide_view,
            g_options.background_dim, g_options.note_timing);
    fprintf(f, "restart_pad = %d\nrin_module = %d\nlen_module = %d\nfree_singers = %d\n", g_options.restart_pad, g_options.singer_module[0],
            g_options.singer_module[1], g_options.free_singers);
    fprintf(f, "keep_vocal = %d\n", g_options.keep_vocal);
    fprintf(f, "blur = %d\nbutton_style = %d\nnote_style = %d\ndpad_notes = %d\nrestart_key = %d\n", g_options.blur, g_options.button_style,
            g_options.note_style < OPTIONS_NOTES_COUNT ? g_options.note_style : 0,
            g_options.dpad_notes, g_options.restart_key);
    if (g_options.note_style >= OPTIONS_NOTES_COUNT && g_options.note_skin[0]) fprintf(f, "note_skin = %s\n", g_options.note_skin);
    if (g_options.module_skin[0]) fprintf(f, "module_skin = %s\n", g_options.module_skin);
    if (g_options.render_scale) fprintf(f, "render_scale = %d\n", g_options.render_scale);
    if (g_options.smoothing >= 0) fprintf(f, "smoothing = %d\n", g_options.smoothing);
    if (g_options.ask_language >= 0) fprintf(f, "ask_language = %d\n", g_options.ask_language);
    if (g_options.confirm_cross >= 0) fprintf(f, "confirm_cross = %d\n", g_options.confirm_cross);
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        if (g_options.keys[i] >= 0) fprintf(f, "key %d = %d\n", i, g_options.keys[i]);
        if (g_options.keys2[i] >= 0) fprintf(f, "key2 %d = %d\n", i, g_options.keys2[i]);
        if (g_options.pads[i] >= 0) fprintf(f, "pad %d = %d\n", i, g_options.pads[i]);
    }
    if (!replace_file_end(f, writing, path)) fprintf(stderr, "options: cannot write %s\n", path);
}

FILE *replace_file_begin(const char *path, char *temp, size_t cap) {
    snprintf(temp, cap, "%s.new", path);
    return fopen(temp, "w");
}

int replace_file_end(FILE *f, const char *temp, const char *path) {
    const int written = !ferror(f);
    if (fclose(f) != 0 || !written || !MoveFileExA(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        remove(temp);
        return 0;
    }
    return 1;
}

/* The project's own options, as more rows of the game's Options window, and a row of the main
 * menu that opens that window on the graphics options.
 *
 * That window (MyRoom::CWindowOption) lists what a small table says: how many rows, and a
 * function that names each one. Both are data, read each time the window opens, so changing them
 * gets the game's own list, cursor and font with no drawing here. The game does nothing with a
 * row past its two, so the hooks below supply what those rows do and the words beside them. The
 * main menu has such a table too.
 *
 * Addresses are for ULJM05472 v1.01, which Tools\build.ps1 checks.
 */

#include "extras.h"

#include "controls.h"
#include "language.h"
#include "letters.h"
#include "options.h"
#include "settings.h"
#include "timing.h"
#include "translation.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <stdio.h>
#include <string.h>

#define LIST_LABEL_FUNCTION 0x08B70600u
#define LIST_ROW_COUNT 0x08B70620u
#define BGM_LABEL_POINTER 0x08C5CEACu
#define SE_LABEL_POINTER 0x08C5CEB0u
#define GET_MENU_SOUND 0x08848C3Cu
#define PLAY_MENU_SOUND 0x08AB179Cu

/* No code of the game's is here. It only names labels() for the game's indirect call. */
#define LABELS_ADDRESS 0x09FFFF00u

/* The window object. Description lines are 4 of {text, x, y, ...}. */
#define WINDOW_ROW 180u
#define WINDOW_LINES 188u
#define LINE_SIZE 16u
#define LINE_COUNT 4u
#define LINE_X 306u
#define LINES_MIDDLE_Y 116

/* The main menu. Rows past its eight get nothing from its tables, and its lines of description
 * are the same 4 of {text, x, y, ...}. */
#define MENU_LABEL_FUNCTION 0x08B700A4u
#define MENU_ROW_COUNT 0x08B700C4u
#define MENU_ROW_NAMES 0x08B705E0u
#define MENU_LINES 216u
#define MENU_ROW 208u
#define MENU_ROW_OPTIONS 5u
#define MENU_ROW_GRAPHICS 8u
#define MENU_ROWS 9u

/* A list of the game's. */
#define LIST_ROWS 20u
#define LIST_ROW_SIZE 24u
#define LIST_ROW_ARROWS 17u
/* Where the menu keeps the row to come back to. */
#define MENU_ROW_KEPT 0x08C3B1E0u
/* No code of the game's is here either. */
#define MENU_LABELS_ADDRESS 0x09FFFF08u
#define DEFAULT_LINE_HEIGHT 16

enum { B_RIGHT = 0x0020, B_LEFT = 0x0080 };

/* The game's own are its two volumes. */
enum { ROW_DIM = 2, ROW_TIMING, ROW_LANGUAGE, ROW_SONGS, ROW_LYRICS, ROW_ASK, ROW_CONFIRM, ROW_BUTTONS, ROW_COUNT };
enum { G_DISPLAY, G_RESOLUTION, G_SMOOTHING, G_SHAPE, G_FPS, G_COUNT };

#define LABEL_WIDTH 22       /* what the game's "%-20s%2d" comes to */
#define LABEL_SIZE 32u
#define ARROWS_MIDDLE 126.0f /* in the game's pixels from the start of a row's text */

typedef struct {
    const char *name, *line1, *line2;
} row_text;

/* What the game's font reads: Shift-JIS, and German letters where translation.c puts them. */
static const row_text ROWS[UI_COUNT][ROW_COUNT] = {
    [UI_ENGLISH] = {
        [ROW_DIM] = {"Background dim", "Darkens the background", "during songs."},
        [ROW_TIMING] = {"Note timing", "Shifts the hit timing.", "Play a song to measure."},
        [ROW_LANGUAGE] = {"Language", "Change the language.", ""},
        [ROW_SONGS] = {"Song names", "Show song names in", "Japanese or romaji."},
        [ROW_LYRICS] = {"Lyrics", "Show lyrics in", "Japanese or romaji."},
        [ROW_ASK] = {"Ask at start", "Ask the language every", "time the game starts."},
        [ROW_CONFIRM] = {"Confirm button", "Which button confirms", "in the menus."},
        [ROW_BUTTONS] = {"Buttons", "Change the keyboard", "and controller buttons."},
    },
    [UI_JAPANESE] = {
        [ROW_DIM] = {"\x94w\x8ci\x82\xcc\x88\xc3\x82\xb3", "\x8b\xc8\x82\xcc\x8a\xd4\x81" "A\x94w\x8ci\x82\xf0", "\x88\xc3\x82\xad\x82\xb5\x82\xdc\x82\xb7\x81" "B"},
        [ROW_TIMING] = {"\x94" "\xbb" "\x92" "\xe8" "\x83" "^" "\x83" "C" "\x83" "~" "\x83" "\x93" "\x83" "O", "\x94" "\xbb" "\x92" "\xe8" "\x82" "\xcc" "\x83" "^" "\x83" "C" "\x83" "~" "\x83" "\x93" "\x83" "O" "\x82" "\xf0" "\x92" "\xb2" "\x90" "\xae", "\x8b" "\xc8" "\x82" "\xf0" "\x97" "V" "\x82" "\xd4" "\x82" "\xc6" "\x91" "\xaa" "\x92" "\xe8" "\x82" "\xb5" "\x82" "\xdc" "\x82" "\xb7"},
        [ROW_LANGUAGE] = {"\x8c\xbe\x8c\xea", "\x8c\xbe\x8c\xea\x82\xf0\x95\xcf\x8d\x58\x82\xb5\x82\xdc\x82\xb7", ""},
        [ROW_SONGS] = {"\x8b\xc8\x96\xbc", "\x8b\xc8\x96\xbc\x82\xcc\x95\x5c\x8b\x4c\x82\xf0\x95\xcf\x8d\x58\x82\xb5\x82\xdc\x82\xb7", ""},
        [ROW_LYRICS] = {"\x89\xcc\x8e\x8c", "\x89\xcc\x8e\x8c\x82\xcc\x95\x5c\x8b\x4c\x82\xf0\x95\xcf\x8d\x58\x82\xb5\x82\xdc\x82\xb7", ""},
        [ROW_ASK] = {"\x8b\x4e\x93\xae\x8e\x9e\x82\xc9\x8a\x6d\x94\x46", "\x8b\x4e\x93\xae\x8e\x9e\x82\xc9\x8c\xbe\x8c\xea\x82\xf0\x8a\x6d\x94\x46\x82\xb5\x82\xdc\x82\xb7", ""},
        [ROW_CONFIRM] = {"\x8c\x88\x92\xe8\x83\x7b\x83\x5e\x83\x93", "\x83\x81\x83\x6a\x83\x85\x81\x5b\x82\xc5\x8c\x88\x92\xe8\x82\xc9", "\x8e\x67\x82\xa4\x83\x7b\x83\x5e\x83\x93\x82\xc5\x82\xb7\x81\x42"},
        [ROW_BUTTONS] = {"\x83\x7b\x83\x5e\x83\x93\x90\xdd\x92\xe8", "\x83\x4c\x81\x5b\x83\x7b\x81\x5b\x83\x68\x82\xc6\x83\x52\x83\x93\x83\x67\x83\x8d\x81\x5b\x83\x89\x81\x5b", "\x82\xcc\x83\x7b\x83\x5e\x83\x93\x82\xf0\x95\xcf\x8d\x58\x82\xb5\x82\xdc\x82\xb7\x81\x42"},
    },
    [UI_GERMAN] = {
        [ROW_DIM] = {"Abdunkeln", "Hintergrund im Lied", "abdunkeln."},
        [ROW_TIMING] = {"Timing", "Verschiebt das Timing.", "Zum Messen Lied spielen."},
        [ROW_LANGUAGE] = {"Sprache", "Sprache \xa1" "ndern.", ""},
        [ROW_SONGS] = {"Liednamen", "Liednamen auf Japanisch", "oder in Romaji."},
        [ROW_LYRICS] = {"Liedtext", "Liedtext auf Japanisch", "oder in Romaji."},
        [ROW_ASK] = {"Beim Start", "Sprache bei jedem", "Start abfragen."},
        [ROW_CONFIRM] = {"Best\xa1tigen", "Welche Taste in Men\xa3s", "best\xa1tigt."},
        [ROW_BUTTONS] = {"Tasten", "Tastatur und Controller", "belegen."},
    },
};

static const row_text GRAPHICS_ROWS[UI_COUNT][G_COUNT] = {
    [UI_ENGLISH] = {
        [G_DISPLAY] = {"Display", "Window, borderless", "or full screen."},
        [G_RESOLUTION] = {"Resolution", "The game is drawn at", "0000x0000"},
        [G_SMOOTHING] = {"Smoothing", "Smooth the picture", "as it fits the window."},
        [G_SHAPE] = {"Image shape", "Fits other window shapes.", "Extend is recommended."},
        [G_FPS] = {"Show FPS", "Show the frame rate", "in the screen corner."},
    },
    [UI_JAPANESE] = {
        [G_DISPLAY] = {"\x95\x5c\x8e\xa6", "\x92\xca\x8f\xed\x81" "A\x98g\x82\xc8\x82\xb5\x81" "A", "\x91S\x89\xe6\x96\xca\x82\xf0\x91I\x82\xd1\x82\xdc\x82\xb7\x81" "B"},
        [G_RESOLUTION] = {"\x89\xf0\x91\x9c\x93x", "\x95`\x89\xe6\x82\xb7\x82\xe9\x89\xf0\x91\x9c\x93x:", "0000x0000"},
        [G_SMOOTHING] = {"\x83X\x83\x80\x81[\x83W\x83\x93\x83O", "\x89\xe6\x96\xca\x82\xf0\x82\xc8\x82\xdf\x82\xe7\x82\xa9\x82\xc9", "\x95\x5c\x8e\xa6\x82\xb5\x82\xdc\x82\xb7\x81" "B"},
        [G_SHAPE] = {"\x89\xe6\x96\xca\x94\xe4\x97\xa6", "\x91\xbc\x82\xcc\x94\xe4\x97\xa6\x82\xcc\x83" "E\x83" "B\x83\x93\x83h\x83" "E\x97p\x82\xc5\x82\xb7\x81" "B", "\x81u\x8ag\x92\xa3\x81v\x82\xf0\x82\xa8\x82\xb7\x82\xb7\x82\xdf\x82\xb5\x82\xdc\x82\xb7\x81" "B"},
        [G_FPS] = {"FPS\x95\x5c\x8e\xa6", "\x83t\x83\x8c\x81[\x83\x80\x83\x8c\x81[\x83g\x82\xf0\x95\x5c\x8e\xa6\x82\xb5\x82\xdc\x82\xb7", ""},
    },
    [UI_GERMAN] = {
        [G_DISPLAY] = {"Anzeige", "Fenster, randlos", "oder Vollbild."},
        [G_RESOLUTION] = {"Aufl\xa2sung", "Spiel wird gezeichnet in", "0000x0000"},
        [G_SMOOTHING] = {"Gl\xa1ttung", "Bild beim Anpassen", "ans Fenster gl\xa1tten."},
        [G_SHAPE] = {"Bildform", "F\xa3r andere Fensterformen.", "Breit wird empfohlen."},
        [G_FPS] = {"FPS zeigen", "Bildrate in der", "Ecke anzeigen."},
    },
};

static const row_text MENU_ROW_WORDS[UI_COUNT] = {
    [UI_ENGLISH] = {"Graphics", "Display, resolution", "and picture options."},
    [UI_JAPANESE] = {"\x83O\x83\x89\x83t\x83" "B\x83" "b\x83N", "\x89\xe6\x96\xca\x82\xc6\x89\xf0\x91\x9c\x93x\x82\xcc", "\x90\xdd\x92\xe8\x82\xf0\x82\xb5\x82\xdc\x82\xb7\x81" "B"},
    [UI_GERMAN] = {"Grafik", "Anzeige, Aufl\xa2sung", "und Bild einstellen."},
};

/* In the language of the rows: */
static const char *const WORD_JAPANESE[UI_COUNT] = {"Japanese", "\x93\xfa\x96\x7b\x8c\xea", "Japanisch"};
static const char *const WORD_ENGLISH[UI_COUNT] = {"English", "\x89\x70\x8c\xea", "Englisch"};
static const char *const WORD_GERMAN[UI_COUNT] = {"German", "\x83\x68\x83\x43\x83\x63\x8c\xea", "Deutsch"};
static const char *const WORD_ROMAJI[UI_COUNT] = {"Romaji", "\x83\x8d\x81\x5b\x83\x7d\x8e\x9a", "Romaji"};
static const char *const WORD_YES[UI_COUNT] = {"Yes", "\x82\xb7\x82\xe9", "Ja"};
static const char *const WORD_NO[UI_COUNT] = {"No", "\x82\xb5\x82\xc8\x82\xa2", "Nein"};
static const char *const WORD_ON[UI_COUNT] = {"On", "\x83\x49\x83\x93", "An"};
static const char *const WORD_OFF[UI_COUNT] = {"Off", "\x83\x49\x83\x74", "Aus"};
static const char *const WORD_WINDOW[UI_COUNT] = {"Window", "\x92\xca\x8f\xed", "Fenster"};
static const char *const WORD_BORDERLESS[UI_COUNT] = {"No edge", "\x98g\x82\xc8\x82\xb5", "Randlos"};
static const char *const WORD_FULL_SCREEN[UI_COUNT] = {"Full", "\x91S\x89\xe6\x96\xca", "Vollbild"};
static const char *const WORD_BARS[UI_COUNT] = {"Bars", "\x8d\x95\x91\xd1", "Balken"};
static const char *const WORD_CROP[UI_COUNT] = {"Crop", "3D\x8ag\x91\xe5", "Schnitt"};
static const char *const WORD_EXTEND[UI_COUNT] = {"Extend", "\x8ag\x92\xa3", "Breit"};
static const char *const WORD_CHANGE[UI_COUNT] = {"Change", "\x95\xcf\x8d\x58", "\xa1ndern"};
/* What the last song measured: before the number, after it when late or early, and with none. */
static const char *const TIMING_LAST[UI_COUNT] = {"Last song: ", "\x91" "O" "\x89" "\xf1" ":" " ", "Zuletzt: "};
static const char *const TIMING_LATE[UI_COUNT] = {" ms late", " " "m" "s" " " "\x92" "x" "\x82" "\xa2", " ms sp\xa1t"};
static const char *const TIMING_EARLY[UI_COUNT] = {" ms early", " " "m" "s" " " "\x91" "\x81" "\x82" "\xa2", " ms fr\xa3h"};
static const char *const TIMING_ON_TIME[UI_COUNT] = {"on time", "\x82" "\xd2" "\x82" "\xc1" "\x82" "\xbd" "\x82" "\xe8", "genau"};
#define TIMING_LINE_SIZE 48u
#define WORD_CIRCLE "\x81\x9b"
#define WORD_CROSS "\x81\x7e"

static char g_data_dir[1100];
static int g_ui;
static uint32_t g_labels[ROW_COUNT];
static uint32_t g_lines[UI_COUNT][ROW_COUNT][2];   /* two lines for each row */
static uint32_t g_graphics_labels[G_COUNT];
static uint32_t g_graphics_lines[UI_COUNT][G_COUNT][2];
static uint32_t g_menu_label[UI_COUNT];
static uint32_t g_menu_lines[UI_COUNT][2];
/* Which list the window opens on, whether that has just changed, and whether the main menu's
 * cursor goes back to the graphics row. */
static int g_graphics, g_list_changed, g_back_to_graphics;
static uint32_t g_held;
static int g_line_height = DEFAULT_LINE_HEIGHT;

static void write_string(uint32_t to, const char *s) {
    do psp_write8(to++, (uint8_t)*s);
    while (*s++);
}

static uint32_t new_string(const char *s) {
    const uint32_t at = psp_sysmem_alloc((uint32_t)strlen(s) + 1, 1);
    if (at) write_string(at, s);
    return at;
}

static const char *script_name(const char *script) {
    return !strcmp(script, "romaji") ? WORD_ROMAJI[g_ui] : WORD_JAPANESE[g_ui];
}

static void next_script(char *script, size_t size) {
    snprintf(script, size, "%s", strcmp(script, "romaji") ? "romaji" : "japanese");
}

static void apply_scripts(void) {
    translation_set_romaji(!strcmp(g_options.song_names, "romaji"), !strcmp(g_options.lyrics, "romaji"));
}

static const char *value_of(int row) {
    switch (row) {
    case ROW_DIM: {
        static char percent[8];
        if (!g_options.background_dim) return WORD_OFF[g_ui];
        snprintf(percent, sizeof percent, "%d%%", g_options.background_dim);
        return percent;
    }
    case ROW_TIMING: {
        static char ms[12];
        snprintf(ms, sizeof ms, g_options.note_timing ? "%+dms" : "%dms", g_options.note_timing);
        return ms;
    }
    case ROW_SONGS:
        return script_name(g_options.song_names);
    case ROW_LYRICS:
        return script_name(g_options.lyrics);
    case ROW_LANGUAGE:
        if (!strcmp(g_options.language, "ja")) return WORD_JAPANESE[g_ui];
        if (!strcmp(g_options.language, "de")) return WORD_GERMAN[g_ui];
        return WORD_ENGLISH[g_ui];
    case ROW_ASK:
        return g_settings.language_prompt ? WORD_YES[g_ui] : WORD_NO[g_ui];
    default:
    case ROW_CONFIRM:
        return g_options.confirm_cross == 1 ? WORD_CROSS : WORD_CIRCLE;
    case ROW_BUTTONS:
        return WORD_CHANGE[g_ui];
    }
}

/* The value goes between the arrows in the row's bar, which leave it 49 pixels. Spaces are all
 * that can move it, so it comes as near the middle as they allow. */
static void write_between_arrows(uint32_t to, const char *name, const char *value) {
    char text[LABEL_SIZE];
    const float gap = ARROWS_MIDDLE - letters_width_small(value) / 2.0f - letters_width_small(name);
    const int spaces = (int)(gap / LETTERS_SMALL_SPACE + 0.5f);
    snprintf(text, sizeof text, "%s%*s%s", name, spaces > 1 ? spaces : 1, "", value);
    write_string(to, text);
}

/* What the last song says, against what is set now, so that setting it right reads as on time. */
static void write_timing_lines(void) {
    int measured;
    if (!timing_last_song(&measured)) return;
    const int off = measured - g_options.note_timing;
    for (int ui = 0; ui < UI_COUNT; ui++) {
        char text[TIMING_LINE_SIZE];
        if (!off) snprintf(text, sizeof text, "%s%s", TIMING_LAST[ui], TIMING_ON_TIME[ui]);
        else snprintf(text, sizeof text, "%s%d%s", TIMING_LAST[ui], off > 0 ? off : -off, off > 0 ? TIMING_LATE[ui] : TIMING_EARLY[ui]);
        write_string(g_lines[ui][ROW_TIMING][1], text);
    }
}

static void write_label(int row) {
    char text[LABEL_SIZE];
    const char *value = value_of(row);
    if (row == ROW_DIM || row == ROW_TIMING) {
        write_between_arrows(g_labels[row], ROWS[g_ui][row].name, value);
        if (row == ROW_TIMING) write_timing_lines();
        return;
    }
    snprintf(text, sizeof text, "%-*s%s", LABEL_WIDTH - (int)strlen(value), ROWS[g_ui][row].name, value);
    write_string(g_labels[row], text);
}

/* As many times the PSP's picture as can be chosen. */
static const int RESOLUTIONS[] = {1, 2, 3, 4, 6, 8};
#define RESOLUTION_COUNT ((int)(sizeof RESOLUTIONS / sizeof RESOLUTIONS[0]))

static const char *graphics_value_of(int row) {
    static char size[16];
    switch (row) {
    case G_DISPLAY:
        return g_options.display_mode == PSP_WINDOW_BORDERLESS ? WORD_BORDERLESS[g_ui] :
               g_options.display_mode == PSP_WINDOW_FULL_SCREEN ? WORD_FULL_SCREEN[g_ui] : WORD_WINDOW[g_ui];
    case G_RESOLUTION:
        snprintf(size, sizeof size, "%dx", g_settings.render_scale);
        return size;
    case G_SMOOTHING:
        return g_settings.present_linear ? WORD_ON[g_ui] : WORD_OFF[g_ui];
    case G_SHAPE:
        return g_options.wide_view == 0 ? WORD_BARS[g_ui] : g_options.wide_view == 1 ? WORD_CROP[g_ui] : WORD_EXTEND[g_ui];
    default:
        return g_options.show_fps ? WORD_ON[g_ui] : WORD_OFF[g_ui];
    }
}

static void write_graphics_label(int row) {
    char text[LABEL_SIZE];
    write_between_arrows(g_graphics_labels[row], GRAPHICS_ROWS[g_ui][row].name, graphics_value_of(row));
    if (row != G_RESOLUTION) return;
    snprintf(text, sizeof text, "%dx%d", 480 * g_settings.render_scale, 272 * g_settings.render_scale);
    for (int ui = 0; ui < UI_COUNT; ui++) write_string(g_graphics_lines[ui][row][1], text);
}

static void write_all_labels(void) {
    if (!g_labels[ROW_COUNT - 1] || !g_graphics_labels[0]) return;  /* no memory for them */
    for (int row = ROW_DIM; row < ROW_COUNT; row++) write_label(row);
    for (int row = 0; row < G_COUNT; row++) write_graphics_label(row);
}

/* In place of the game's function that names the rows: the row in a0, its text in v0. */
static void labels(void) {
    const uint32_t row = psp_cpu.r[PSP_REG_A0];
    if (g_graphics) psp_cpu.r[PSP_REG_V0] = row < G_COUNT ? g_graphics_labels[row] : 0;
    else if (row == 0) psp_cpu.r[PSP_REG_V0] = psp_read32(BGM_LABEL_POINTER);
    else if (row == 1) psp_cpu.r[PSP_REG_V0] = psp_read32(SE_LABEL_POINTER);
    else psp_cpu.r[PSP_REG_V0] = row < ROW_COUNT ? g_labels[row] : 0;
}

/* The same for the main menu, whose own function gives its row names from this table. */
static void menu_labels(void) {
    const uint32_t row = psp_cpu.r[PSP_REG_A0];
    psp_cpu.r[PSP_REG_V0] = row == MENU_ROW_GRAPHICS ? g_menu_label[g_ui] : psp_read32(MENU_ROW_NAMES + row * 4);
}

/* The game's own sound for changing a value. It is called at the entry of a function of
 * the game's, so that function must find its registers as they were. */
static void play_sound(void) {
    uint32_t saved[32];
    memcpy(saved, psp_cpu.r, sizeof saved);
    psp_dispatch(GET_MENU_SOUND);
    psp_cpu.r[PSP_REG_A0] = psp_cpu.r[PSP_REG_V0];
    psp_cpu.r[PSP_REG_A1] = 0;
    psp_cpu.r[PSP_REG_A2] = 0;
    psp_cpu.r[PSP_REG_A3] = 0;
    psp_dispatch(PLAY_MENU_SOUND);
    memcpy(psp_cpu.r, saved, sizeof saved);
}

static void change(int row) {
    switch (row) {
    case ROW_SONGS:
        if (!translation_has_song_romaji()) return;
        next_script(g_options.song_names, sizeof g_options.song_names);
        apply_scripts();
        break;
    case ROW_LYRICS:
        if (!translation_has_lyric_romaji()) return;
        next_script(g_options.lyrics, sizeof g_options.lyrics);
        apply_scripts();
        break;
    case ROW_LANGUAGE:
        /* English, Japanese, German, and round again. */
        language_apply(!strcmp(g_options.language, "en") ? "ja" : !strcmp(g_options.language, "ja") ? "de" : "en");
        break;
    case ROW_ASK:
        g_settings.language_prompt = !g_settings.language_prompt;
        g_options.ask_language = g_settings.language_prompt;
        break;
    case ROW_CONFIRM:
        g_options.confirm_cross = g_options.confirm_cross != 1;
        controls_apply();
        break;
    case ROW_BUTTONS:
        controls_change_buttons();
        g_held = psp_window_buttons();
        break;
    default:
        return;
    }
    write_label(row);
    options_save(g_data_dir);
    play_sound();
}

/* One step along a list of `count`, and round again. */
static int stepped(int value, int count, int step) {
    return (value + step + count) % count;
}

static void change_graphics(int row, int step) {
    switch (row) {
    case G_DISPLAY:
        g_options.display_mode = stepped(g_options.display_mode, PSP_WINDOW_MODE_COUNT, step);
        psp_window_set_mode(g_options.display_mode);
        break;
    case G_RESOLUTION: {
        int at = 0;
        while (at < RESOLUTION_COUNT - 1 && RESOLUTIONS[at] < g_settings.render_scale) at++;
        g_settings.render_scale = RESOLUTIONS[stepped(at, RESOLUTION_COUNT, step)];
        g_options.render_scale = g_settings.render_scale;
        psp_render_gl_set_scale(g_settings.render_scale);
        break;
    }
    case G_SMOOTHING:
        g_settings.present_linear = !g_settings.present_linear;
        g_options.smoothing = g_settings.present_linear;
        psp_render_gl_set_present_filter(g_settings.present_linear);
        break;
    case G_SHAPE:
        g_options.wide_view = stepped(g_options.wide_view, 3, step);
        psp_render_gl_set_wide_view(g_options.wide_view);
        break;
    default:
        g_options.show_fps = !g_options.show_fps;
        psp_render_gl_set_overlay(g_options.show_fps);
        break;
    }
    write_graphics_label(row);
    options_save(g_data_dir);
    play_sound();
}

/* These go by ones, and on by themselves with the button held, as far as each end. They are saved
 * when the button comes up, not at each step. */
#define DIM_MOST 100
#define SLIDER_HOLD_FRAMES 20
#define SLIDER_REPEAT_FRAMES 2
static int g_slider_unsaved;

static void step_slider(int row, int step) {
    int *value = row == ROW_DIM ? &g_options.background_dim : &g_options.note_timing;
    const int least = row == ROW_DIM ? 0 : OPTIONS_TIMING_LEAST, most = row == ROW_DIM ? DIM_MOST : OPTIONS_TIMING_MOST;
    if (*value + step < least || *value + step > most) return;
    *value += step;
    g_slider_unsaved = 1;
    if (row == ROW_DIM) psp_render_gl_set_background_dim(*value);
    write_label(row);
}

static void update_slider(int row, uint32_t buttons, uint32_t pressed) {
    static int held_frames;
    const uint32_t held = buttons & (B_LEFT | B_RIGHT);
    if (pressed & (B_LEFT | B_RIGHT)) {
        held_frames = 0;
        step_slider(row, pressed & B_LEFT ? -1 : 1);
        play_sound();
    } else if (held && ++held_frames >= SLIDER_HOLD_FRAMES && held_frames % SLIDER_REPEAT_FRAMES == 0) {
        step_slider(row, held & B_LEFT ? -1 : 1);
    }
}

/* F11 and Alt+Enter change the display away from the menus, so it is noted here. */
static void note_display_mode(void) {
    const int mode = psp_window_get_mode();
    if (!g_graphics_labels[0]) return;
    if (mode == g_options.display_mode) return;
    g_options.display_mode = mode;
    write_graphics_label(G_DISPLAY);
    options_save(g_data_dir);
}

static void on_window_update(void) {
    const uint32_t buttons = psp_window_buttons();
    const uint32_t pressed = buttons & ~g_held;
    g_held = buttons;
    const uint32_t row = psp_read32(psp_cpu.r[PSP_REG_A0] + WINDOW_ROW);
    note_display_mode();
    if (g_slider_unsaved && !(buttons & (B_LEFT | B_RIGHT))) {
        g_slider_unsaved = 0;
        options_save(g_data_dir);
    }
    if (!g_graphics && (row == ROW_DIM || row == ROW_TIMING)) {
        update_slider((int)row, buttons, pressed);
        return;
    }
    if (!(pressed & (B_LEFT | B_RIGHT))) return;
    if (g_graphics) {
        if (row < G_COUNT) change_graphics((int)row, pressed & B_LEFT ? -1 : 1);
    } else if (row > ROW_TIMING && row < ROW_COUNT) {
        change((int)row);
    }
}

/* Words beside a row, in lines of the game's own: one or two, around the same middle. */
static void fill_lines(uint32_t lines, const uint32_t *words) {
    const int count = psp_read8(words[1]) ? 2 : 1;
    for (uint32_t i = 0; i < LINE_COUNT; i++) {
        const uint32_t line = lines + i * LINE_SIZE;
        const int used = i < (uint32_t)count;
        psp_write32(line, used ? words[i] : 0);
        if (!used) continue;
        psp_write32(line + 4, LINE_X);
        psp_write32(line + 8, (uint32_t)(LINES_MIDDLE_Y - (g_line_height / 2) * count + g_line_height * (int)i));
    }
}

/* The game fills these lines from a table with two rows, so past those it has filled them
 * with whatever came next in memory. None of that is drawn: this runs first. */
static void on_window_draw(void) {
    const uint32_t lines = psp_cpu.r[PSP_REG_A0] + WINDOW_LINES;
    const uint32_t row = psp_read32(psp_cpu.r[PSP_REG_A0] + WINDOW_ROW);
    if (g_graphics) {
        if (row < G_COUNT) fill_lines(lines, g_graphics_lines[g_ui][row]);
        return;
    }
    if (row < ROW_DIM) {
        /* Rows of two lines, so this is the size of the font in use. */
        const int gap = (int)psp_read32(lines + LINE_SIZE + 8) - (int)psp_read32(lines + 8);
        if (psp_read32(lines + LINE_SIZE) && gap > 0) g_line_height = gap;
        return;
    }
    if (row >= ROW_COUNT) return;
    fill_lines(lines, g_lines[g_ui][row]);
}

/* Choosing a row of the main menu. The graphics row goes as the Options row does, on the other
 * list, so all the game does from here on is what it knows how to do. */
static void on_menu_confirm(void) {
    const uint32_t menu = psp_cpu.r[PSP_REG_A0], row = psp_cpu.r[PSP_REG_A1];
    if (row != MENU_ROW_OPTIONS && row != MENU_ROW_GRAPHICS) return;
    const int graphics = row == MENU_ROW_GRAPHICS;
    if (graphics != g_graphics) g_list_changed = 1;
    g_graphics = graphics;
    psp_write32(LIST_ROW_COUNT, graphics ? G_COUNT : ROW_COUNT);
    if (graphics) {
        g_back_to_graphics = 1;
        psp_cpu.r[PSP_REG_A1] = MENU_ROW_OPTIONS;
        psp_write32(menu + MENU_ROW, MENU_ROW_OPTIONS);
    }
}

static void on_menu_enter(void) {
    if (g_back_to_graphics) psp_write32(MENU_ROW_KEPT, MENU_ROW_GRAPHICS);
    g_back_to_graphics = 0;
}

/* Entering the menu, it has just read its row to ask how to come in: from rows 0, 1, 4 and 5 as it
 * is, and from any other with a white fade, as those leave for scenes of their own. This one is
 * a window, as Options is. */
static void on_menu_row_read(void) {
    if (psp_cpu.r[PSP_REG_V1] == MENU_ROW_GRAPHICS) psp_cpu.r[PSP_REG_V1] = MENU_ROW_OPTIONS;
}

/* The game's tables have nothing for this row, and it has filled the lines from what follows. */
static void on_menu_draw(void) {
    const uint32_t menu = psp_cpu.r[PSP_REG_A0];
    note_display_mode();
    if (psp_read32(menu + MENU_ROW) == MENU_ROW_GRAPHICS) fill_lines(menu + MENU_LINES, g_menu_lines[g_ui]);
}

static void on_window_enter(void) {
    /* The row it was left on may not be on this list. */
    if (g_list_changed) psp_write32(psp_cpu.r[PSP_REG_A0] + WINDOW_ROW, 0);
    g_list_changed = 0;
    write_all_labels();
}

/* What the window puts at its top, just given by the game's function. */
static void on_window_title(void) {
    if (g_graphics) psp_cpu.r[PSP_REG_V0] = g_menu_label[g_ui];
}

/* The game has just marked its two volume rows for the bars with arrows. All of these change
 * with left and right too. */
static void on_window_rows_marked(void) {
    const uint32_t rows = psp_read32(psp_cpu.r[PSP_REG_S0] + LIST_ROWS);
    if (!g_graphics) {
        psp_write8(rows + ROW_DIM * LIST_ROW_SIZE + LIST_ROW_ARROWS, 1);
        psp_write8(rows + ROW_TIMING * LIST_ROW_SIZE + LIST_ROW_ARROWS, 1);
        return;
    }
    for (uint32_t row = 0; row < G_COUNT; row++) psp_write8(rows + row * LIST_ROW_SIZE + LIST_ROW_ARROWS, 1);
}

/* The first two rows of the game's list are its volumes, which it adjusts by their numbers. This
 * is where it has read the row to ask which it is on. */
static void on_window_volume(void) {
    if (g_graphics) psp_cpu.r[PSP_REG_V1] = ROW_LANGUAGE;
}

void extras_language_changed(void) {
    g_ui = language_ui();
    write_all_labels();
}

/* A row's two lines in guest memory. Zero if there is no room. */
static int new_lines(uint32_t lines[2], const row_text *text) {
    lines[0] = new_string(text->line1);
    lines[1] = new_string(text->line2);
    return lines[0] && lines[1];
}

static void leave_main_menu_alone(void) {
    fprintf(stderr, "extras: no memory for the graphics rows, leaving the main menu as it is\n");
    g_graphics_labels[0] = 0;
}

void extras_start(const char *data_dir) {
    snprintf(g_data_dir, sizeof g_data_dir, "%s", data_dir);
    g_ui = language_ui();
    /* A choice with nothing behind it, as on a computer the dictionary would not install on. */
    if (!translation_has_song_romaji()) snprintf(g_options.song_names, sizeof g_options.song_names, "japanese");
    if (!translation_has_lyric_romaji()) snprintf(g_options.lyrics, sizeof g_options.lyrics, "japanese");
    apply_scripts();
    for (int row = ROW_DIM; row < ROW_COUNT; row++) {
        g_labels[row] = psp_sysmem_alloc(LABEL_SIZE, 1);
        int have_memory = g_labels[row] != 0;
        for (int ui = 0; ui < UI_COUNT; ui++) {
            have_memory = new_lines(g_lines[ui][row], &ROWS[ui][row]) && have_memory;
            if (row != ROW_TIMING) continue;
            /* Its second line changes after each song. */
            g_lines[ui][row][1] = psp_sysmem_alloc(TIMING_LINE_SIZE, 1);
            if (g_lines[ui][row][1]) write_string(g_lines[ui][row][1], ROWS[ui][row].line2);
            else have_memory = 0;
        }
        if (!have_memory) {
            fprintf(stderr, "extras: no memory for the rows, leaving the window as it is\n");
            return;
        }
        write_label(row);
    }
    psp_register(LABELS_ADDRESS, labels);
    psp_write32(LIST_LABEL_FUNCTION, LABELS_ADDRESS);
    psp_write32(LIST_ROW_COUNT, ROW_COUNT);

    for (int row = 0; row < G_COUNT; row++) {
        g_graphics_labels[row] = psp_sysmem_alloc(LABEL_SIZE, 1);
        int have_memory = g_graphics_labels[row] != 0;
        for (int ui = 0; ui < UI_COUNT; ui++) have_memory = new_lines(g_graphics_lines[ui][row], &GRAPHICS_ROWS[ui][row]) && have_memory;
        if (!have_memory) {
            leave_main_menu_alone();
            return;
        }
        write_graphics_label(row);
    }
    for (int ui = 0; ui < UI_COUNT; ui++) {
        g_menu_label[ui] = new_string(MENU_ROW_WORDS[ui].name);
        if (!new_lines(g_menu_lines[ui], &MENU_ROW_WORDS[ui]) || !g_menu_label[ui]) {
            leave_main_menu_alone();
            return;
        }
    }
    psp_register(MENU_LABELS_ADDRESS, menu_labels);
    psp_write32(MENU_LABEL_FUNCTION, MENU_LABELS_ADDRESS);
    psp_write32(MENU_ROW_COUNT, MENU_ROWS);
}

int extras_hook(uint32_t addr) {
    if (addr == EXTRAS_WINDOW_UPDATE) on_window_update();
    else if (addr == EXTRAS_WINDOW_DRAW) on_window_draw();
    else if (addr == EXTRAS_WINDOW_ENTER) on_window_enter();
    else if (addr == EXTRAS_WINDOW_TITLE) on_window_title();
    else if (addr == EXTRAS_WINDOW_VOLUME) on_window_volume();
    else if (addr == EXTRAS_WINDOW_ROWS) { /* only so that the next can be inside it */ }
    else if (addr == EXTRAS_WINDOW_ROWS_MARKED) on_window_rows_marked();
    else if (addr == EXTRAS_MENU_ENTER) on_menu_enter();
    else if (addr == EXTRAS_MENU_ROW_READ) on_menu_row_read();
    else if (addr == EXTRAS_MENU_DRAW) on_menu_draw();
    else if (addr == EXTRAS_MENU_CONFIRM) on_menu_confirm();
    else return 0;
    return 1;
}

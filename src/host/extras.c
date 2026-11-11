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
#include "guest.h"

#include "controls.h"
#include "language.h"
#include "letters.h"
#include "note_skins.h"
#include "options.h"
#include "settings.h"
#include "timing.h"
#include "translation.h"
#include "ui_text.h"

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
#define MENU_ROW_GRAPHICS 6u   /* shown after Options; the game's rows from here on come one lower */
#define MENU_ROWS 9u
/* The game's lines of description for its rows, four pointers a row. */
#define MENU_ROW_LINES 0x08B70290u

/* A list of the game's. */
#define LIST_ROWS 20u
#define LIST_ROW_SIZE 24u
#define LIST_ROW_ARROWS 17u
/* Where the menu keeps the row to come back to. */
#define MENU_ROW_KEPT 0x08C3B1E0u
/* No code of the game's is here either. */
#define MENU_LABELS_ADDRESS 0x09FFFF08u
#define DEFAULT_LINE_HEIGHT 16

enum { B_RIGHT = 0x0020, B_LEFT = 0x0080, B_CIRCLE_KEY = 0x2000, B_CROSS_KEY = 0x4000 };

/* The game's own are its two volumes. */
/* In the order shown: what changes a song, the buttons, then the language. */
enum { ROW_TIMING = 2, ROW_DIM, ROW_NOTES, ROW_SINGERS, ROW_VOCAL, ROW_CONFIRM, ROW_ICONS, ROW_BUTTONS, ROW_DPAD, ROW_LANGUAGE, ROW_SONGS, ROW_LYRICS, ROW_ASK, ROW_COUNT };
#define ROW_FIRST ROW_TIMING   /* the first of these, after the game's two volumes */
enum { G_DISPLAY, G_SHAPE, G_RESOLUTION, G_SMOOTHING, G_BLUR, G_FPS, G_COUNT };   /* window and shape, then the picture, then the extra */

#define LABEL_WIDTH 22       /* what the game's "%-20s%2d" comes to */
#define LABEL_SIZE 32u
#define ARROWS_MIDDLE 126.0f /* in the game's pixels from the start of a row's text */

/* The rows' words are in lang\<code>_ui.txt, as extras.<row>.name, .line1 and .line2, and the same
 * under graphics.; they reach the game as its font reads them (ui_game_text). */
static const char *const ROW_KEY[ROW_COUNT] = {
    [ROW_TIMING] = "timing", [ROW_DIM] = "dim", [ROW_NOTES] = "notes", [ROW_SINGERS] = "singers", [ROW_VOCAL] = "vocal",
    [ROW_CONFIRM] = "confirm", [ROW_ICONS] = "icons", [ROW_BUTTONS] = "buttons", [ROW_DPAD] = "dpad", [ROW_LANGUAGE] = "language",
    [ROW_SONGS] = "songs", [ROW_LYRICS] = "lyrics", [ROW_ASK] = "ask"};
static const char *const G_KEY[G_COUNT] = {[G_DISPLAY] = "display", [G_SHAPE] = "shape", [G_RESOLUTION] = "resolution",
                                           [G_SMOOTHING] = "smoothing", [G_BLUR] = "blur", [G_FPS] = "fps"};

static const char *word(const char *key) { return ui_game_text(key); }
static int stepped(int value, int count, int step);

/* "<list>.<row>.<part>" of a row. */
static const char *row_word(const char *list, const char *row, const char *part) {
    char key[64];
    snprintf(key, sizeof key, "%s.%s.%s", list, row, part);
    return word(key);
}

#define TIMING_LINE_SIZE 48u
#define LINE_ROOM 64u        /* a line of a row's description, in any language */
static const char *const STYLE_NAME[OPTIONS_STYLE_COUNT] = {"PlayStation", "Xbox", "Nintendo"};
#define WORD_CIRCLE "\x81\x9b"
#define WORD_CROSS "\x81\x7e"

static char g_data_dir[1100];
/* In guest memory, in the language being played, written again when it changes. */
static uint32_t g_labels[ROW_COUNT];
static uint32_t g_lines[ROW_COUNT][2];   /* two lines for each row */
static uint32_t g_graphics_labels[G_COUNT];
static uint32_t g_graphics_lines[G_COUNT][2];
static uint32_t g_menu_label;
static uint32_t g_menu_lines[2];
/* Which list the window opens on, whether that has just changed, and whether the main menu's
 * cursor goes back to the graphics row. */
static int g_graphics, g_list_changed, g_back_to_graphics;
static uint32_t g_held;
static int g_line_height = DEFAULT_LINE_HEIGHT;

static void write_string(uint32_t to, const char *s) {
    if (!to) return;   /* no memory was found for it */
    do psp_write8(to++, (uint8_t)*s);
    while (*s++);
}

/* Text into a room of `room` bytes, cut short if a translation is longer, never inside a Shift-JIS
 * letter of two bytes. */
static void write_in_room(uint32_t to, const char *s, uint32_t room) {
    if (!to) return;
    uint32_t n = 0;
    while (s[n] && n + 1 < room) {
        const unsigned char c = (unsigned char)s[n];
        const int pair = (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
        if (pair && (n + 2 >= room || !s[n + 1])) break;
        psp_write8(to + n, c);
        if (pair) {
            psp_write8(to + n + 1, (uint8_t)s[n + 1]);
            n++;
        }
        n++;
    }
    psp_write8(to + n, 0);
}

static void write_line(uint32_t to, const char *s) { write_in_room(to, s, LINE_ROOM); }
static void write_label_text(uint32_t to, const char *s) { write_in_room(to, s, LABEL_SIZE); }

static uint32_t new_string(const char *s) {
    const uint32_t at = psp_sysmem_alloc((uint32_t)strlen(s) + 1, 1);
    if (at) write_string(at, s);
    return at;
}

static const char *script_name(const char *script) {
    return word(!strcmp(script, "romaji") ? "extras.romaji" : "language.ja");
}

static void next_script(char *script, size_t size) {
    snprintf(script, size, "%s", strcmp(script, "romaji") ? "romaji" : "japanese");
}

static void apply_scripts(void) {
    translation_set_romaji(!strcmp(g_options.song_names, "romaji"), !strcmp(g_options.lyrics, "romaji"));
}

void extras_next_note_style(void) { extras_step_note_style(1); }

void extras_step_note_style(int step) {
    g_options.note_style = stepped(g_options.note_style, OPTIONS_NOTES_COUNT + note_skins_count(), step);
    snprintf(g_options.note_skin, sizeof g_options.note_skin, "%s", note_skins_name(g_options.note_style - OPTIONS_NOTES_COUNT));
    language_notes_changed();
}

/* A note skin's name as the menu's letters can show it: ASCII only, cut short to fit the row. */
const char *extras_note_style_name(void) {
    static char shown[16];
    const char *name = note_skins_name(g_options.note_style - OPTIONS_NOTES_COUNT);
    size_t n = 0;
    for (; name[n] && n < sizeof shown - 1; n++) shown[n] = (unsigned char)name[n] >= 32 && (unsigned char)name[n] < 127 ? name[n] : '?';
    shown[n] = 0;
    if (name[n]) memcpy(shown + sizeof shown - 4, "...", 4);
    return shown;
}

static const char *value_of(int row) {
    switch (row) {
    case ROW_DIM: {
        static char percent[8];
        if (!g_options.background_dim) return word("extras.off");
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
    case ROW_LANGUAGE: {
        char key[32];
        snprintf(key, sizeof key, "language.%s", g_options.language);
        return word(key);
    }
    case ROW_ASK:
        return g_settings.language_prompt ? word("extras.yes") : word("extras.no");
    case ROW_ICONS:
        return STYLE_NAME[g_options.button_style];
    case ROW_NOTES:
        if (g_options.note_style >= OPTIONS_NOTES_COUNT) return extras_note_style_name();
        return g_options.note_style == OPTIONS_NOTES_KEYBOARD ? word("extras.keyboard") : STYLE_NAME[g_options.note_style];
    default:
    case ROW_CONFIRM: {
        /* Its sign in the style chosen, and which side of the pad it is on. */
        static char text[24];
        const int cross = g_options.confirm_cross == 1, letter = options_letter_for(cross);
        char sign[3] = {(char)letter, 0, 0};
        if (!letter) snprintf(sign, sizeof sign, "%s", cross ? WORD_CROSS : WORD_CIRCLE);
        snprintf(text, sizeof text, "%s %s", sign, cross ? word("extras.south") : word("extras.east"));
        return text;
    }
    case ROW_DPAD:
        return g_options.dpad_notes == OPTIONS_HIT_STICK ? word("extras.stick") : g_options.dpad_notes == OPTIONS_HIT_DPAD ? word("extras.dpad") :
               g_options.dpad_notes == OPTIONS_HIT_BOTH  ? word("extras.both")  : word("extras.off");
    case ROW_BUTTONS:
        return word("extras.change");
    case ROW_SINGERS:
        return g_options.free_singers ? word("extras.module") : word("extras.rin_len");
    case ROW_VOCAL:
        return g_options.keep_vocal ? word("extras.no") : word("extras.yes");
    }
}

/* The value goes between the arrows in the row's bar, which leave it 49 pixels. Spaces are all
 * that can move it, so it comes as near the middle as they allow. */
static void write_between_arrows(uint32_t to, const char *name, const char *value) {
    char text[LINE_ROOM * 2];
    const float gap = ARROWS_MIDDLE - letters_width_small(value) / 2.0f - letters_width_small(name);
    const int spaces = (int)(gap / LETTERS_SMALL_SPACE + 0.5f);
    snprintf(text, sizeof text, "%s%*s%s", name, spaces > 1 && spaces < LINE_ROOM ? spaces : 1, "", value);
    write_label_text(to, text);
}

/* What the last song says, against what is set now, so that setting it right reads as on time. */
static void write_timing_lines(void) {
    int measured;
    if (!timing_last_song(&measured)) return;
    const int off = measured - g_options.note_timing;
    char text[TIMING_LINE_SIZE];
    if (!off) snprintf(text, sizeof text, "%s%s", word("extras.timing.last"), word("extras.timing.on_time"));
    else snprintf(text, sizeof text, "%s%d%s", word("extras.timing.last"), off > 0 ? off : -off,
                  word(off > 0 ? "extras.timing.late" : "extras.timing.early"));
    write_line(g_lines[ROW_TIMING][1], text);
}

static void write_label(int row) {
    char text[LINE_ROOM * 2];
    const char *value = value_of(row);
    if (row == ROW_DIM || row == ROW_TIMING) {
        write_between_arrows(g_labels[row], row_word("extras", ROW_KEY[row], "name"), value);
        if (row == ROW_TIMING) write_timing_lines();
        return;
    }
    snprintf(text, sizeof text, "%-*s%s", LABEL_WIDTH - (int)strlen(value), row_word("extras", ROW_KEY[row], "name"), value);
    write_label_text(g_labels[row], text);
}

/* As many times the PSP's picture as can be chosen. */
static const int RESOLUTIONS[] = {1, 2, 3, 4, 6, 8};
#define RESOLUTION_COUNT ((int)(sizeof RESOLUTIONS / sizeof RESOLUTIONS[0]))

static const char *graphics_value_of(int row) {
    static char size[16];
    switch (row) {
    case G_DISPLAY:
        return g_options.display_mode == PSP_WINDOW_BORDERLESS ? word("graphics.borderless") :
               g_options.display_mode == PSP_WINDOW_FULL_SCREEN ? word("graphics.full_screen") : word("graphics.window");
    case G_RESOLUTION:
        snprintf(size, sizeof size, "%dx", g_settings.render_scale);
        return size;
    case G_SMOOTHING:
        return g_settings.present_linear ? word("extras.on") : word("extras.off");
    case G_BLUR:
        return g_options.blur ? word("extras.on") : word("extras.off");
    case G_SHAPE:
        return g_options.wide_view == 0 ? word("graphics.bars") : g_options.wide_view == 1 ? word("graphics.crop") : word("graphics.extend");
    default:
        return g_options.show_fps ? word("extras.on") : word("extras.off");
    }
}

static void write_graphics_label(int row) {
    char text[LINE_ROOM];
    write_between_arrows(g_graphics_labels[row], row_word("graphics", G_KEY[row], "name"), graphics_value_of(row));
    if (row != G_RESOLUTION) return;
    snprintf(text, sizeof text, "%dx%d", 480 * g_settings.render_scale, 272 * g_settings.render_scale);
    write_line(g_graphics_lines[row][1], text);
}

static void write_all_labels(void) {
    if (!g_labels[ROW_COUNT - 1] || !g_graphics_labels[0]) return;  /* no memory for them */
    for (int row = ROW_FIRST; row < ROW_COUNT; row++) write_label(row);
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
    psp_cpu.r[PSP_REG_V0] = row == MENU_ROW_GRAPHICS ? g_menu_label
                          : psp_read32(MENU_ROW_NAMES + (row > MENU_ROW_GRAPHICS ? row - 1 : row) * 4);
}

/* The game's row a row shown stands for: the same before Graphics, one lower after it. */
static uint32_t game_row(uint32_t shown) { return shown > MENU_ROW_GRAPHICS ? shown - 1 : shown; }

/* The game's own sound for changing a value. It is called at the entry of a function of
 * the game's, so that function must find its registers as they were. */
static void play_sound(void) {
    const uint32_t sounds = guest_call(GET_MENU_SOUND, 0, 0, 0, 0);
    guest_call(PLAY_MENU_SOUND, sounds, 0, 0, 0);
}

/* One step along a list of `count`, and round again. */
static int stepped(int value, int count, int step) {
    return ((value + step) % count + count) % count;
}

/* A row's value one step on: Right forward, Left back, as the player sees them listed. Rows of two
 * values only turn over. */
static void change(int row, int step) {
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
        /* The next or the one before in lang\languages.txt, and round again. */
        language_apply(ui_text_code(stepped(ui_text_find(g_options.language), ui_text_languages(), step)));
        break;
    case ROW_ASK:
        g_settings.language_prompt = !g_settings.language_prompt;
        g_options.ask_language = g_settings.language_prompt;
        break;
    case ROW_CONFIRM:
        g_options.confirm_cross = g_options.confirm_cross != 1;
        controls_apply();
        break;
    case ROW_ICONS:
        g_options.button_style = stepped(g_options.button_style, OPTIONS_STYLE_COUNT, step);
        controls_style_changed();
        write_label(ROW_CONFIRM);
        break;
    case ROW_NOTES:
        extras_step_note_style(step);
        break;
    case ROW_SINGERS:
        g_options.free_singers = !g_options.free_singers;
        break;
    case ROW_VOCAL:
        g_options.keep_vocal = !g_options.keep_vocal;
        fprintf(stderr, "options: mute on miss %s\n", g_options.keep_vocal ? "off" : "on");
        break;
    case ROW_BUTTONS:
        controls_change_buttons();
        g_held = psp_window_buttons();
        break;
    case ROW_DPAD: {
        /* Stick, d-pad, both, off, and round again. */
        static const int ORDER[4] = {OPTIONS_HIT_STICK, OPTIONS_HIT_DPAD, OPTIONS_HIT_BOTH, OPTIONS_HIT_OFF};
        int at = 0;
        for (int i = 0; i < 4; i++)
            if (ORDER[i] == g_options.dpad_notes) at = i;
        g_options.dpad_notes = ORDER[stepped(at, 4, step)];
        break;
    }
    default:
        return;
    }
    write_label(row);
    options_save(g_data_dir);
    play_sound();
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
    case G_BLUR:
        g_options.blur = !g_options.blur;
        psp_render_gl_set_blur(g_options.blur);
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
    /* Buttons is a button: the player's confirm button opens its screen. The buttons read here
     * are before the swap that makes it the game's Circle, so it is Cross if Cross was chosen. */
    if (!g_graphics && row == ROW_BUTTONS) {
        if (pressed & (g_options.confirm_cross == 1 ? B_CROSS_KEY : B_CIRCLE_KEY)) change((int)row, 1);
        return;
    }
    if (!(pressed & (B_LEFT | B_RIGHT))) return;
    if (g_graphics) {
        if (row < G_COUNT) change_graphics((int)row, pressed & B_LEFT ? -1 : 1);
    } else if (row >= ROW_FIRST && row < ROW_COUNT) {
        change((int)row, pressed & B_LEFT ? -1 : 1);
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
        if (row < G_COUNT) fill_lines(lines, g_graphics_lines[row]);
        return;
    }
    if (row < ROW_FIRST) {
        /* Rows of two lines, so this is the size of the font in use. */
        const int gap = (int)psp_read32(lines + LINE_SIZE + 8) - (int)psp_read32(lines + 8);
        if (psp_read32(lines + LINE_SIZE) && gap > 0) g_line_height = gap;
        return;
    }
    if (row >= ROW_COUNT) return;
    fill_lines(lines, g_lines[row]);
}

/* Choosing a row of the main menu. The graphics row goes as the Options row does, on the other
 * list, so all the game does from here on is what it knows how to do. */
static int g_back_to_row;   /* a row past Graphics to come back to, 0 for none */

static void on_menu_confirm(void) {
    const uint32_t menu = psp_cpu.r[PSP_REG_A0], row = psp_cpu.r[PSP_REG_A1];
    /* A row past Graphics is the game's one above it; it leaves the menu for a scene of its own,
     * and coming back finds the row it was chosen from. */
    if (row > MENU_ROW_GRAPHICS) {
        psp_cpu.r[PSP_REG_A1] = game_row(row);
        psp_write32(menu + MENU_ROW, game_row(row));
        g_back_to_row = (int)row;
        return;
    }
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
    else if (g_back_to_row && psp_read32(MENU_ROW_KEPT) == game_row((uint32_t)g_back_to_row)) psp_write32(MENU_ROW_KEPT, (uint32_t)g_back_to_row);
    g_back_to_graphics = 0;
    g_back_to_row = 0;
}

/* Entering the menu, it has just read its row to ask how to come in: from rows 0, 1, 4 and 5 as it
 * is, and from any other with a white fade, as those leave for scenes of their own. This one is
 * a window, as Options is. */
static void on_menu_row_read(void) {
    const uint32_t row = psp_cpu.r[PSP_REG_V1];
    psp_cpu.r[PSP_REG_V1] = row == MENU_ROW_GRAPHICS ? MENU_ROW_OPTIONS : game_row(row);
}

/* The game's tables have nothing for this row, and it has filled the lines from what follows. */
static void on_menu_draw(void) {
    const uint32_t menu = psp_cpu.r[PSP_REG_A0], row = psp_read32(menu + MENU_ROW);
    note_display_mode();
    if (row == MENU_ROW_GRAPHICS) {
        fill_lines(menu + MENU_LINES, g_menu_lines);
    } else if (row > MENU_ROW_GRAPHICS && row < MENU_ROWS) {
        /* The game filled the lines of the row below this one's: those of the row it stands for. */
        const uint32_t from = MENU_ROW_LINES + game_row(row) * LINE_COUNT * 4;
        uint32_t words[LINE_COUNT];
        int count = 0;
        for (uint32_t i = 0; i < LINE_COUNT; i++) {
            words[i] = psp_read32(from + i * 4);
            if (words[i]) count = (int)i + 1;
        }
        for (uint32_t i = 0; i < LINE_COUNT; i++) {
            const uint32_t line = menu + MENU_LINES + i * LINE_SIZE;
            const int used = (int)i < count;
            psp_write32(line, used ? words[i] : 0);
            if (!used) continue;
            psp_write32(line + 4, LINE_X);
            psp_write32(line + 8, (uint32_t)(LINES_MIDDLE_Y - (g_line_height / 2) * count + g_line_height * (int)i));
        }
    }
}

static void on_window_enter(void) {
    /* The row it was left on may not be on this list. */
    if (g_list_changed) psp_write32(psp_cpu.r[PSP_REG_A0] + WINDOW_ROW, 0);
    g_list_changed = 0;
    write_all_labels();
}

/* What the window puts at its top, just given by the game's function. */
static void on_window_title(void) {
    if (g_graphics) psp_cpu.r[PSP_REG_V0] = g_menu_label;
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

/* Each row's two lines, the main menu's row and its lines, in the language being played. The
 * second line of the timing row is the last song's, once one has been measured. */
static void write_all_lines(void) {
    for (int row = ROW_FIRST; row < ROW_COUNT; row++) {
        write_line(g_lines[row][0], row_word("extras", ROW_KEY[row], "line1"));
        write_line(g_lines[row][1], row_word("extras", ROW_KEY[row], "line2"));
    }
    for (int row = 0; row < G_COUNT; row++) {
        write_line(g_graphics_lines[row][0], row_word("graphics", G_KEY[row], "line1"));
        write_line(g_graphics_lines[row][1], row_word("graphics", G_KEY[row], "line2"));
    }
    write_line(g_menu_label, word("graphics.menu.name"));
    write_line(g_menu_lines[0], word("graphics.menu.line1"));
    write_line(g_menu_lines[1], word("graphics.menu.line2"));
}

void extras_language_changed(void) {
    write_all_lines();
    write_all_labels();
}

/* Room in guest memory for a line, or for a row's two. Zero if there is none. */
static uint32_t new_line(void) {
    const uint32_t at = psp_sysmem_alloc(LINE_ROOM, 1);
    if (at) psp_write8(at, 0);
    return at;
}

static int new_lines(uint32_t lines[2]) {
    lines[0] = new_line();
    lines[1] = new_line();
    return lines[0] && lines[1];
}

static void leave_main_menu_alone(void) {
    fprintf(stderr, "extras: no memory for the graphics rows, leaving the main menu as it is\n");
    g_graphics_labels[0] = 0;
}

void extras_start(const char *data_dir) {
    snprintf(g_data_dir, sizeof g_data_dir, "%s", data_dir);
    /* A choice with nothing behind it, as on a computer the dictionary would not install on. */
    if (!translation_has_song_romaji()) snprintf(g_options.song_names, sizeof g_options.song_names, "japanese");
    if (!translation_has_lyric_romaji()) snprintf(g_options.lyrics, sizeof g_options.lyrics, "japanese");
    apply_scripts();
    for (int row = ROW_FIRST; row < ROW_COUNT; row++) {
        g_labels[row] = psp_sysmem_alloc(LABEL_SIZE, 1);
        if (!g_labels[row] || !new_lines(g_lines[row])) {
            fprintf(stderr, "extras: no memory for the rows, leaving the window as it is\n");
            return;
        }
    }
    for (int row = 0; row < G_COUNT; row++) {
        g_graphics_labels[row] = psp_sysmem_alloc(LABEL_SIZE, 1);
        if (!g_graphics_labels[row] || !new_lines(g_graphics_lines[row])) {
            leave_main_menu_alone();
            break;
        }
    }
    if (g_graphics_labels[0]) {
        g_menu_label = new_line();
        if (!g_menu_label || !new_lines(g_menu_lines)) {
            g_menu_label = 0;
            leave_main_menu_alone();
        }
    }
    write_all_lines();
    for (int row = ROW_FIRST; row < ROW_COUNT; row++) write_label(row);
    psp_register(LABELS_ADDRESS, labels);
    psp_write32(LIST_LABEL_FUNCTION, LABELS_ADDRESS);
    psp_write32(LIST_ROW_COUNT, ROW_COUNT);
    if (!g_menu_label) return;
    for (int row = 0; row < G_COUNT; row++) write_graphics_label(row);
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

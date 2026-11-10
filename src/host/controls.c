/* Which button does what.
 *
 * The buttons themselves are kept by the window, and read before the game sees them. Which
 * button confirms is different: the game's menus want Circle, and the notes of a song want
 * each button for itself, so Circle and Cross are swapped only when no song is being played.
 * The game calls SONG_UPDATE on every frame from the start of a song until after its results,
 * pause menu included, and the object it gets says which of those it is.
 *
 * The screens read the keyboard and controllers directly, so a mistake made in them can
 * still be put right in them.
 */

#include "controls.h"

#include "audio_out.h"
#include "guest.h"
#include "extras.h"
#include "presence.h"

#include "language.h"
#include "module_sort.h"
#include "options.h"
#include "screen_kit.h"
#include "song_sort.h"
#include "timing.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define SONG_UPDATE 0x08864C90u
/* In the object it gets, each nonzero when: there is a song; the pause menu is open, in either
 * of its two forms; the fade out of the song is running; a question from that menu is open; the
 * notes are over and the results are coming. */
#define SONG_ACTIVE 0u
#define SONG_PAUSED 1u
#define SONG_PAUSED_TOO 2u
#define SONG_LEAVING 3u
#define SONG_ASKING 8u
#define SONG_OVER 0xBB2Au
#define SONG_GONE_MS 150u

enum { REG_A0 = 4 };
enum { B_CIRCLE = 0x2000, B_CROSS = 0x4000, B_TRIANGLE = 0x1000, B_SQUARE = 0x8000 };
enum { B_UP = 0x0010, B_RIGHT_WAY = 0x0020, B_DOWN = 0x0040, B_LEFT_WAY = 0x0080, B_DPAD = 0x00F0 };
enum { B_START = 0x0008, B_L = 0x0100, B_R = 0x0200 };

/* The controller's own names for what the screens read, as the window numbers them. */
enum { PAD_SOUTH = 0, PAD_EAST = 1, PAD_UP = 11, PAD_DOWN = 12, PAD_LEFT = 13, PAD_RIGHT = 14 };

#define CAPTURE_MS 5000

static const char *g_disc;
static volatile ULONGLONG g_song_seen;

static int key_down(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

/* Whether a key has just gone down, with the game's window the one in front. */
static int pressed_in_window(int vk, int *was_down) {
    HWND window = (HWND)psp_window_handle();
    const int down = window && GetForegroundWindow() == window && key_down(vk);
    const int pressed = down && !*was_down;
    *was_down = down;
    return pressed;
}

/* With DIVA_COMPARE set, Space goes between the pieces drawn from vectors and the game's own,
 * to check one against the other. The window's title says which is showing. */
static void compare_with_space(void) {
    static int compare = -1, was_down = 1, off;
    if (compare < 0) compare = getenv("DIVA_COMPARE") != NULL;
    if (!compare || !pressed_in_window(VK_SPACE, &was_down)) return;
    off = !off;
    language_show_vectors(!off);
    SetWindowTextA((HWND)psp_window_handle(), off ? "Project DIVA Recomp - ORIGINAL" : "Project DIVA Recomp - VECTOR");
    fprintf(stderr, "controls: showing %s\n", off ? "the game's own pieces" : "pieces from vectors");
}

/* With DIVA_WIDE set, F2 goes through the ways the image can follow the window's shape. */
static void compare_image_shapes(void) {
    static const char *const NAME[] = {
        "Project DIVA Recomp - BARS", "Project DIVA Recomp - CROP 3D", "Project DIVA Recomp - EXTEND"
    };
    static int wide = -1, was_down = 1, trace_was_down = 1;
    if (wide < 0) wide = getenv("DIVA_WIDE") != NULL;
    if (!wide) return;
    /* DIVA_TRACE_EVERY, a test switch: a frame written down every that many seconds, by itself. */
    static int every = -1;
    static ULONGLONG next_trace;
    if (every < 0) every = getenv("DIVA_TRACE_EVERY") ? atoi(getenv("DIVA_TRACE_EVERY")) : 0;
    if (every > 0 && GetTickCount64() >= next_trace) {
        if (next_trace) psp_render_gl_trace_frame();
        next_trace = GetTickCount64() + (ULONGLONG)every * 1000u;
    }
    if (pressed_in_window(VK_F3, &trace_was_down)) psp_render_gl_trace_frame();
    /* F8 goes through the styles of button signs, to see each at once. Not saved. */
    static int style_was_down = 1;
    if (pressed_in_window(VK_F8, &style_was_down)) {
        static const char *const STYLE[OPTIONS_STYLE_COUNT] = {"PLAYSTATION", "XBOX", "NINTENDO"};
        g_options.button_style = (g_options.button_style + 1) % OPTIONS_STYLE_COUNT;
        controls_style_changed();
        char title[160];
        snprintf(title, sizeof title, "Project DIVA Recomp - buttons %s, %s confirms", STYLE[g_options.button_style],
                 g_options.confirm_cross == 1 ? "Cross" : "Circle");
        SetWindowTextA((HWND)psp_window_handle(), title);
        fprintf(stderr, "controls: %s\n", title);
    }
    /* F9 the same for the notes. Not saved either. */
    static int notes_was_down = 1;
    if (pressed_in_window(VK_F9, &notes_was_down)) {
        static const char *const NOTES[OPTIONS_NOTES_COUNT] = {"PLAYSTATION", "XBOX", "NINTENDO", "KEYBOARD"};
        g_options.note_style = (g_options.note_style + 1) % OPTIONS_NOTES_COUNT;
        language_notes_changed();
        char title[160];
        snprintf(title, sizeof title, "Project DIVA Recomp - notes %s", NOTES[g_options.note_style]);
        SetWindowTextA((HWND)psp_window_handle(), title);
        fprintf(stderr, "controls: %s\n", title);
    }
    if (!pressed_in_window(VK_F2, &was_down)) return;
    g_options.wide_view = (g_options.wide_view + 1) % 3;
    psp_render_gl_set_wide_view(g_options.wide_view);
    SetWindowTextA((HWND)psp_window_handle(), NAME[g_options.wide_view]);
    fprintf(stderr, "controls: image shape %d\n", g_options.wide_view);
}

/* A message that waits for a button goes at the first press after all are up, as the one that
 * brought it on may still be held. */
static void press_clears_held_message(uint32_t buttons) {
    static int all_up;
    if (!buttons) {
        all_up = 1;
    } else if (all_up) {
        all_up = 0;
        psp_render_gl_clear_held_message();
    }
}

/* ---- quick restart ----------------------------------------------------------------- *
 *
 * L and R pressed together in a song, or the key chosen for it, play it again from the start: the
 * game is given what a player would press, Start for its pause menu, down to RETRY, the game's
 * confirm button, and the same again for the question that follows, each step waiting for the song
 * to show it has come. Nothing the player holds reaches the game meanwhile. */
static volatile int g_song_paused, g_song_asking;
/* The pause menu's list is the first of the window manager's, and it keeps the row it is on at 16:
 * 0 RESUME, 1 RETRY, 2 RETURN. Asked for and set from the song's own thread. */
#define WINDOW_LIST 0x08AFC55Cu
#define LIST_ROW 16u
#define ROW_RETRY 1u
static volatile int g_restart_put_cursor, g_restart_cursor_put;
static int g_restart_step;
static ULONGLONG g_restart_at;

#define RESTART_PRESS_MS 90
#define RESTART_GIVE_UP_MS 1500

static void restart_next(int step) {
    g_restart_step = step;
    g_restart_at = GetTickCount64();
    fprintf(stderr, "controls: quick restart, step %d\n", step);
}

/* Takes the buttons as the game would get them; gives back what it is to get instead. */
static uint32_t quick_restart(uint32_t buttons, int in_song) {
    static uint32_t held;
    static int key_was_down = 1;
    static uint32_t pad_held;
    const uint32_t mask = (uint32_t)g_options.restart_pad, pad = psp_window_pad_raw();
    /* The controller's buttons the player chose, all of them down at once, or else L and R. */
    const int both = mask ? (pad & mask) == mask : (buttons & (B_L | B_R)) == (B_L | B_R);
    const int both_before = mask ? (pad_held & mask) == mask : (held & (B_L | B_R)) == (B_L | B_R);
    held = buttons;
    pad_held = pad;
    const int key = g_options.restart_key > 0 && pressed_in_window(g_options.restart_key, &key_was_down);
    if (!g_restart_step) {
        if (in_song && ((both && !both_before) || key)) restart_next(1);
        else return buttons;
    }
    const ULONGLONG since = GetTickCount64() - g_restart_at;
    const uint32_t press = since < RESTART_PRESS_MS;
    switch (g_restart_step) {
    case 1: /* Start, until the pause menu is open. */
        if (g_song_paused && since > 200) restart_next(2);
        else if (since > RESTART_GIVE_UP_MS) g_restart_step = 0;
        return press ? B_START : 0;
    case 2: /* The menu is still coming in. Then its cursor is put on RETRY. */
        if (since > 350) {
            g_restart_cursor_put = 0;
            g_restart_put_cursor = 1;
            restart_next(3);
        }
        return 0;
    case 3: /* RETRY, as the cursor was put there; if it could not be, down from RESUME. */
        if (g_restart_cursor_put && since > 100) restart_next(4);
        else if (since > 300) {
            g_restart_put_cursor = 0;
            if (since > 500) restart_next(4);
            return since < 300 + RESTART_PRESS_MS ? B_DOWN : 0;
        }
        return 0;
    case 4: /* RETRY. */
        if (since > 250) restart_next(5);
        return press ? B_CIRCLE : 0;
    case 5: /* The question, if one comes: its own confirm button says yes. */
        if (g_song_asking && since > 300) restart_next(6);
        else if (since > 1000) g_restart_step = 0;
        return 0;
    default:
        if (since > 300) g_restart_step = 0;
        return press ? B_CIRCLE : 0;
    }
}

static uint32_t swap_confirm(uint32_t buttons) {
    press_clears_held_message(buttons);
    /* In the song list Start switches practice (see practice.c), and the game does not get it. */
    if (song_sort_list_in_front()) buttons &= ~(uint32_t)B_START;
    module_sort_input(buttons);
    compare_with_space();
    compare_image_shapes();
    const int in_song = GetTickCount64() - g_song_seen < SONG_GONE_MS;
    /* What it gives is already as the game takes it, its confirm button Circle. */
    if (g_restart_step || in_song) {
        const uint32_t given = quick_restart(buttons, in_song);
        if (g_restart_step || given != buttons) return given;
    }
    /* In a song, each way of the d-pad or the stick, as the player chose, is the button on that
     * side: up Triangle, right Circle, down Cross, left Square, as in the games after this one. */
    if (in_song && g_options.dpad_notes && (buttons & B_DPAD)) {
        const uint32_t from_stick = psp_window_stick_buttons() & buttons & B_DPAD, from_dpad = buttons & B_DPAD & ~from_stick;
        const uint32_t ways = (g_options.dpad_notes & OPTIONS_HIT_DPAD ? from_dpad : 0) | (g_options.dpad_notes & OPTIONS_HIT_STICK ? from_stick : 0);
        static const uint32_t WAY[4][2] = {{B_UP, B_TRIANGLE}, {B_RIGHT_WAY, B_CIRCLE}, {B_DOWN, B_CROSS}, {B_LEFT_WAY, B_SQUARE}};
        for (int i = 0; i < 4; i++)
            if (ways & WAY[i][0]) buttons |= WAY[i][1];
        buttons &= ~ways;
    }
    if (g_options.confirm_cross != 1 || in_song) return buttons;
    const uint32_t circle = buttons & B_CIRCLE, cross = buttons & B_CROSS;
    buttons &= ~(uint32_t)(B_CIRCLE | B_CROSS);
    if (circle) buttons |= B_CROSS;
    if (cross) buttons |= B_CIRCLE;
    return buttons;
}

void controls_apply(void) {
    psp_ctrl_set_filter(swap_confirm);
    language_confirm_changed();
}

void controls_style_changed(void) {
    language_confirm_changed();
    extras_language_changed();
}

void controls_start(const char *disc) {
    g_disc = disc;
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        if (g_options.keys[i] >= 0) psp_window_set_key(i, g_options.keys[i]);
        if (g_options.keys2[i] >= 0) psp_window_set_key2(i, g_options.keys2[i]);
        if (g_options.pads[i] >= 0) psp_window_set_pad(i, (uint32_t)g_options.pads[i]);
    }
    controls_apply();
}

int controls_hook(uint32_t addr) {
    if (addr != SONG_UPDATE) return 0;
    const uint32_t song = psp_cpu.r[REG_A0];
    const ULONGLONG now = GetTickCount64();
    const int was_playing = now - g_song_seen < SONG_GONE_MS;
    const int playing = psp_read8(song + SONG_ACTIVE) && !psp_read8(song + SONG_OVER) && !psp_read8(song + SONG_PAUSED) &&
                        !psp_read8(song + SONG_PAUSED_TOO) && !psp_read8(song + SONG_LEAVING) && !psp_read8(song + SONG_ASKING);
    /* Paused as well, so that the picture does not jump. Not the fade out or what follows it. */
    const int on = psp_read8(song + SONG_ACTIVE) && !psp_read8(song + SONG_OVER) && !psp_read8(song + SONG_LEAVING);
    if (on) psp_render_gl_song_frame();
    presence_song_frame(on);
    if (on) audio_out_song_frame();
    /* Only the changes, to see in the log that this is what it is thought to be. */
    if (playing != was_playing) fprintf(stderr, "controls: %s\n", playing ? "a song is playing" : "the song stopped");
    if (was_playing && !playing) timing_song_stopped();
    /* A pause menu or the results get their confirm button on the very frame they begin. */
    g_song_seen = playing ? now : 0;
    g_song_paused = psp_read8(song + SONG_PAUSED) || psp_read8(song + SONG_PAUSED_TOO);
    g_song_asking = psp_read8(song + SONG_ASKING) != 0;
    /* For the quick restart: the pause menu's cursor on RETRY. The game's function is called as at
     * the entry of this one, so the registers it finds are put back after. */
    if (g_restart_put_cursor && g_song_paused) {
        const uint32_t list = guest_call(WINDOW_LIST, GAME_WINDOW_MANAGER, 0, 0, 0);
        g_restart_put_cursor = 0;
        if (guest_in_ram(list) && psp_read32(list + LIST_ROW) <= 2) {
            psp_write32(list + LIST_ROW, ROW_RETRY);
            g_restart_cursor_put = 1;
        }
        fprintf(stderr, "controls: quick restart, pause list at %08X, cursor %s\n", list, g_restart_cursor_put ? "on RETRY" : "not found");
    }
    return 1;
}

/* ---- what the screens read ---------------------------------------------------------- */

typedef struct {
    int up, down, left, right, ok, back;
} menu_input;

/* Whether the last thing pressed in a screen was on a controller rather than the keyboard, so that
 * its hints name the buttons the player has in hand. */
static int g_from_pad;

/* On a controller the button under Cross confirms if the player chose Cross, and the other one goes
 * back; Circle the other way round. `split` is zero where either confirms, before that choice. */
static menu_input read_menu(int split) {
    const uint32_t pad = psp_window_pad_raw();
    const int cross = g_options.confirm_cross == 1;
    const int pad_ok = split ? (pad >> (cross ? PAD_SOUTH : PAD_EAST) & 1) : (pad >> PAD_SOUTH & 1) || (pad >> PAD_EAST & 1);
    const int pad_back = split && (pad >> (cross ? PAD_EAST : PAD_SOUTH) & 1);
    menu_input in;
    in.up = key_down(VK_UP) || (pad >> PAD_UP & 1);
    in.down = key_down(VK_DOWN) || (pad >> PAD_DOWN & 1);
    in.left = key_down(VK_LEFT) || (pad >> PAD_LEFT & 1);
    in.right = key_down(VK_RIGHT) || (pad >> PAD_RIGHT & 1);
    in.ok = key_down(VK_RETURN) || pad_ok;
    in.back = key_down(VK_ESCAPE) || pad_back;
    if (pad) {
        g_from_pad = 1;
    } else {
        static const int KEYS[] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_ESCAPE};
        for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
            if (key_down(KEYS[i])) g_from_pad = 0;
        }
    }
    return in;
}

/* For the other screens shown over the game (system_dialogs.c): the same reading, the confirm button
 * as the player chose it, and which they used last. */
controls_input controls_screen_input(void) {
    const menu_input in = read_menu(1);
    return (controls_input){in.up, in.down, in.left, in.right, in.ok, in.back};
}
int controls_screen_from_pad(void) { return g_from_pad; }
const char *controls_disc(void) { return g_disc; }

/* ---- which button confirms ---------------------------------------------------------- */

static const wchar_t *const ASK_QUESTION[UI_COUNT] = {L"Which button confirms?", L"\x6c7a\x5b9a\x30dc\x30bf\x30f3\x3092\x9078\x3093\x3067\x304f\x3060\x3055\x3044", L"Welche Taste best\x00e4tigt?"};
static const wchar_t *const ASK_NOTE[UI_COUNT] = {L"You can change this later in Options.", L"\x5f8c\x3067\x30aa\x30d7\x30b7\x30e7\x30f3\x304b\x3089\x5909\x66f4\x3067\x304d\x307e\x3059\x3002", L"Sp\x00e4ter in den Optionen \x00e4nderbar."};
static const wchar_t *const ASK_CIRCLE[UI_COUNT] = {L"East, as the game was made", L"\x53f3\x30dc\x30bf\x30f3\xff08\x30aa\x30ea\x30b8\x30ca\x30eb\xff09", L"Ost, wie im Original"};
static const wchar_t *const ASK_CROSS[UI_COUNT] = {L"South", L"\x4e0b\x30dc\x30bf\x30f3", L"S\x00fc" L"d"};
static const wchar_t *const WORD_SELECT[UI_COUNT] = {L"Select", L"\x6c7a\x5b9a", L"Ausw\x00e4hlen"};
static const wchar_t *const WORD_MOVE[UI_COUNT] = {L"Move", L"\x79fb\x52d5", L"Bewegen"};

#define ASK_X 24
#define ASK_Y 104
#define ASK_W 400
#define ASK_H 52
#define ASK_GAP 6
#define ASK_PAD 8
#define ASK_HERO_X 690
#define ASK_HERO_Y 210
#define ASK_HERO_R 72
#define ASK_CARD_X 500
#define ASK_CARD_Y 352
#define ASK_CARD_W 436
#define ASK_CARD_H 104

static void draw_ask(int selected, int ui) {
    kit_clear(ASK_QUESTION[ui]);
    const wchar_t *const rows[2] = {ASK_CIRCLE[ui], ASK_CROSS[ui]};
    const int glyphs[2] = {KIT_GLYPH_CIRCLE, KIT_GLYPH_CROSS};
    kit_panel(ASK_X, ASK_Y, ASK_W, ASK_PAD * 2 + 2 * ASK_H + ASK_GAP);
    const int row_x = ASK_X + ASK_PAD, row_w = ASK_W - ASK_PAD * 2;
    for (int i = 0; i < 2; i++) {
        const int y = ASK_Y + ASK_PAD + i * (ASK_H + ASK_GAP), on = i == selected;
        if (on) kit_selected(row_x, y, row_w, ASK_H);
        kit_glyph(row_x + 30, y + ASK_H / 2, 14, glyphs[i], on ? KIT_INK : KIT_TEXT, on ? KIT_INK : KIT_ACCENT);
        kit_text(row_x + 58, y + ASK_H / 2, 18, 1, on ? KIT_INK : KIT_TEXT, rows[i], KIT_LEFT);
    }
    if (selected == 0) kit_rule(row_x + 16, ASK_Y + ASK_PAD + ASK_H + ASK_GAP / 2, row_w - 32);

    const int hero_x = kit_right(ASK_HERO_X), card_x = kit_right(ASK_CARD_X);
    kit_halo(hero_x, ASK_HERO_Y, ASK_HERO_R * 2);
    kit_glyph(hero_x, ASK_HERO_Y, ASK_HERO_R, glyphs[selected], KIT_TEXT, KIT_ACCENT);

    kit_panel(card_x, ASK_CARD_Y, ASK_CARD_W, ASK_CARD_H);
    kit_mark(card_x + 24, ASK_CARD_Y + 22);
    kit_text(card_x + 24, ASK_CARD_Y + 48, 14, 0, KIT_SOFT, rows[selected], KIT_LEFT);
    kit_text(card_x + 24, ASK_CARD_Y + 76, 16, 0, KIT_TEXT, ASK_NOTE[ui], KIT_LEFT);

    int right = kit_width() - 32;
    /* Either face button confirms here, as the choice is not made yet: the game's own is shown. */
    right = g_from_pad ? kit_hint(right, KIT_GLYPH_CIRCLE, WORD_SELECT[ui]) : kit_hint_key(right, L"Enter", WORD_SELECT[ui]);
    kit_hint(right, KIT_GLYPH_DPAD, WORD_MOVE[ui]);
}

/* ---- how the buttons and the notes look ------------------------------------------------ */

static const wchar_t *const ICONS_QUESTION[UI_COUNT] = {
    L"Which button icons?", L"\x30dc\x30bf\x30f3\x306e\x8868\x793a\x3092\x9078\x3093\x3067\x304f\x3060\x3055\x3044", L"Welche Tastensymbole?"};
static const wchar_t *const NOTES_QUESTION[UI_COUNT] = {
    L"How should the notes look?", L"\x30ce\x30fc\x30c4\x306e\x898b\x305f\x76ee\x3092\x9078\x3093\x3067\x304f\x3060\x3055\x3044",
    L"Wie sollen die Noten aussehen?"};
static const wchar_t *const WORD_ORIGINAL[UI_COUNT] = {L"PlayStation, as the game was made", L"PlayStation\xff08\x30aa\x30ea\x30b8\x30ca\x30eb\xff09",
                                                       L"PlayStation, wie im Original"};
static const wchar_t *const WORD_KEYS[UI_COUNT] = {L"Keyboard arrows", L"\x30ad\x30fc\x30dc\x30fc\x30c9", L"Tastatur-Pfeile"};
static const wchar_t *const WORD_OK[UI_COUNT] = {L"Confirm", L"\x6c7a\x5b9a", L"Best\x00e4tigen"};
static const wchar_t *const WORD_GO_BACK[UI_COUNT] = {L"Back", L"\x623b\x308b", L"Zur\x00fc\x0063k"};
static const wchar_t *const ICONS_NOTE[UI_COUNT] = {
    L"Xbox and Nintendo pads have A and B the other way round.", L"Xbox\x3068Nintendo\x3067\x306f A \x3068 B \x304c\x9006\x3067\x3059\x3002",
    L"Bei Xbox und Nintendo sind A und B vertauscht."};
static const wchar_t *const NOTES_NOTE[UI_COUNT] = {
    L"As the notes and their targets will look in songs.", L"\x66f2\x306e\x4e2d\x3067\x306e\x30ce\x30fc\x30c4\x3068\x30bf\x30fc\x30b2\x30c3\x30c8\x3067\x3059\x3002",
    L"So sehen Noten und Ziele in den Liedern aus."};

/* The pictures the renderer draws the notes from, for the style being looked at. */
static kit_picture g_note_pictures[4], g_targets, g_hand;
static int g_pictures_style = -1;

static void read_note_pictures(int style) {
    if (style == g_pictures_style) return;
    g_pictures_style = style;
    for (int i = 0; i < 4; i++) kit_picture_free(&g_note_pictures[i]);
    kit_picture_free(&g_targets);
    static const char *const FOLDER[OPTIONS_NOTES_COUNT] = {"", "\\notes\\xbox", "\\notes\\nintendo", "\\notes\\keyboard"};
    /* Triangle, Circle, Cross and Square, as the game's textures are named. */
    static const char *const NOTE[4] = {"0FD2D4FB", "AC18437B", "17820130", "D50BEC6E"};
    char path[1300];
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof path, "%s\\..\\textures_hd%s\\%s.rgba", language_data_dir(), FOLDER[style], NOTE[i]);
        kit_picture_read(&g_note_pictures[i], path, 2);
    }
    if (style) snprintf(path, sizeof path, "%s\\..\\textures_hd%s\\targets.rgba", language_data_dir(), FOLDER[style]);
    else snprintf(path, sizeof path, "%s\\..\\textures_hd\\F9253859.rgba", language_data_dir());
    kit_picture_read(&g_targets, path, style ? 3 : 2);
    /* The game's own sheet has the hand between its targets: taken out, so that no piece of it
     * shows with them. */
    if (!style && g_targets.px) {
        const int u = g_targets.w / 128;
        for (int y = 40 * u; y < 86 * u && y < g_targets.h; y++)
            for (int x = 63 * u; x < 84 * u && x < g_targets.w; x++) g_targets.px[(size_t)y * g_targets.w + x] = 0;
    }
    if (!g_hand.px) {
        snprintf(path, sizeof path, "%s\\..\\textures_hd\\F9253859.rgba", language_data_dir());
        kit_picture_read(&g_hand, path, 2);
    }
}

/* Four targets in a row, each with its note coming to it and the clock's hand, as in a song. */
static void draw_notes_preview(int cx, int cy, int style) {
    read_note_pictures(style);
    /* Quarters of the targets' sheet, by column and row, for Triangle, Circle, Cross and Square. */
    static const int QUARTER[4][2] = {{1, 0}, {0, 0}, {1, 1}, {0, 1}};
    /* On the game's own sheet, its parts and the ground around them that is not drawn. */
    static const uint32_t GROUND[2] = {0xB9B6D7, 0x101022};
    const double side = 84, gap = 32, left = cx - (4 * side + 3 * gap) / 2;
    for (int i = 0; i < 4; i++) {
        const double x = left + i * (side + gap), y = cy - side / 2;
        if (g_targets.px) {
            /* The game draws its own sheet upside down; the styles' sheets are made the right way up. */
            const double q = g_targets.w / 2.0, top = QUARTER[i][1] * q;
            if (style) kit_picture_draw(&g_targets, QUARTER[i][0] * q, top, q, q, x, y, side, side, NULL, 0);
            else kit_picture_draw(&g_targets, QUARTER[i][0] * q, top + q, q, -q, x, y, side, side, GROUND, 2);
        }
        /* The hand, on the game's sheet from 49 to 80.5 of 128 down and 68 to 78 across, turning about
         * the square at its foot, at 73, 75: as long as the target is from its middle to its edge. */
        if (g_hand.px) {
            const double u = g_hand.w / 128.0, k = side * 0.40 / 26.0;
            kit_picture_draw(&g_hand, 66 * u, 47 * u, 14 * u, 36 * u, x + side / 2 - 7 * k, y + side / 2 - 28 * k, 14 * k, 36 * k, GROUND, 2);
        }
    }
    /* The notes, as large as their targets, each coming down to its own, all in one row. Their
     * symbols are the middle 34 of their 64 texels, filling 25 of them. */
    for (int i = 0; i < 4; i++) {
        const kit_picture *note = &g_note_pictures[i];
        if (!note->px) continue;
        const double u = note->w / 64.0, size = side * 0.92 * 34.0 / 25.0;
        const double mx = left + i * (side + gap) + side / 2, my = cy - 1.35 * side;
        kit_picture_draw(note, 15.5 * u, 17 * u, 34 * u, 34 * u, mx - size / 2, my - size / 2, size, size, NULL, 0);
    }
}

static void draw_styles(int page, int selected, int ui) {
    const int notes = page == 2;
    kit_clear(notes ? NOTES_QUESTION[ui] : ICONS_QUESTION[ui]);
    const wchar_t *rows[4] = {WORD_ORIGINAL[ui], L"Xbox", L"Nintendo", WORD_KEYS[ui]};
    const int count = notes ? 4 : 3;
    kit_panel(ASK_X, ASK_Y, ASK_W, ASK_PAD * 2 + count * ASK_H + (count - 1) * ASK_GAP);
    const int row_x = ASK_X + ASK_PAD, row_w = ASK_W - ASK_PAD * 2;
    for (int i = 0; i < count; i++) {
        const int y = ASK_Y + ASK_PAD + i * (ASK_H + ASK_GAP), on = i == selected;
        if (on) kit_selected(row_x, y, row_w, ASK_H);
        else if (i + 1 < count && i + 1 != selected) kit_rule(row_x + 16, y + ASK_H + ASK_GAP / 2, row_w - 32);
        kit_text(row_x + 24, y + ASK_H / 2, 18, 1, on ? KIT_INK : KIT_TEXT, rows[i], KIT_LEFT);
    }
    const int hero_x = kit_right(ASK_HERO_X), card_x = kit_right(ASK_CARD_X);
    kit_halo(hero_x, ASK_HERO_Y, ASK_HERO_R * 2);
    if (notes) {
        draw_notes_preview(hero_x, ASK_HERO_Y + 80, selected);
    } else {
        /* What confirms and what goes back, as they will be drawn. */
        const int cross = g_options.confirm_cross == 1;
        const int ok = cross ? KIT_GLYPH_CROSS : KIT_GLYPH_CIRCLE, back = cross ? KIT_GLYPH_CIRCLE : KIT_GLYPH_CROSS;
        kit_glyph(hero_x - 90, ASK_HERO_Y, 56, ok, KIT_TEXT, KIT_ACCENT);
        kit_glyph(hero_x + 90, ASK_HERO_Y, 56, back, KIT_TEXT, KIT_ACCENT);
        kit_text(hero_x - 90, ASK_HERO_Y + 82, 16, 1, KIT_TEXT, WORD_OK[ui], KIT_CENTRE);
        kit_text(hero_x + 90, ASK_HERO_Y + 82, 16, 1, KIT_TEXT, WORD_GO_BACK[ui], KIT_CENTRE);
    }
    kit_panel(card_x, ASK_CARD_Y, ASK_CARD_W, ASK_CARD_H);
    kit_mark(card_x + 24, ASK_CARD_Y + 22);
    kit_text(card_x + 24, ASK_CARD_Y + 48, 14, 0, KIT_SOFT, notes ? NOTES_NOTE[ui] : ICONS_NOTE[ui], KIT_LEFT);
    kit_text(card_x + 24, ASK_CARD_Y + 76, 16, 0, KIT_TEXT, ASK_NOTE[ui], KIT_LEFT);

    int right = kit_width() - 32;
    const int cross = g_options.confirm_cross == 1;
    if (g_from_pad) {
        right = kit_hint(right, cross ? KIT_GLYPH_CIRCLE : KIT_GLYPH_CROSS, WORD_GO_BACK[ui]);
        right = kit_hint(right, cross ? KIT_GLYPH_CROSS : KIT_GLYPH_CIRCLE, WORD_SELECT[ui]);
    } else {
        right = kit_hint_key(right, L"Esc", WORD_GO_BACK[ui]);
        right = kit_hint_key(right, L"Enter", WORD_SELECT[ui]);
    }
    kit_hint(right, KIT_GLYPH_DPAD, WORD_MOVE[ui]);
}

/* The buttons' icons, then the confirm button, drawn in the icons chosen, then the notes, going
 * back a step with Back. Each page is what it asks: 0 the confirm button, 1 the icons, 2 the notes. */
void controls_ask_confirm(void) {
    if (!kit_open(g_disc)) {
        if (g_options.confirm_cross < 0) g_options.confirm_cross = 0;
        return;
    }
    const int ui = language_ui();
    static const int ASKS[3] = {1, 0, 2};
    int page = 0;
    int chosen[3] = {g_options.confirm_cross == 1, g_options.button_style, g_options.note_style};
    static const int COUNT[3] = {2, OPTIONS_STYLE_COUNT, OPTIONS_NOTES_COUNT};
    menu_input was = {1, 1, 1, 1, 1, 1};
    int redraw = 1;
    for (;;) {
        const int asks = ASKS[page];
        if (redraw) {
            g_options.confirm_cross = chosen[0];
            g_options.button_style = chosen[1];
            g_options.note_style = chosen[2];
            if (asks == 0) draw_ask(chosen[0], ui);
            else draw_styles(asks, chosen[asks], ui);
            redraw = 0;
        }
        kit_frame();
        const int from_pad = g_from_pad;
        /* Either face button confirms until the confirm button is chosen. */
        const menu_input in = read_menu(page > 1);
        const int up = in.up && !was.up, down = in.down && !was.down, ok = in.ok && !was.ok, back = in.back && !was.back;
        was = in;
        if (up) chosen[asks] = (chosen[asks] + COUNT[asks] - 1) % COUNT[asks];
        if (down) chosen[asks] = (chosen[asks] + 1) % COUNT[asks];
        if (ok || (back && page > 0)) {
            page += ok ? 1 : -1;
            if (page > 2) break;
            kit_wait_for_release();
            was = (menu_input){1, 1, 1, 1, 1, 1};
        }
        redraw = up || down || ok || back || from_pad != g_from_pad || kit_resized();
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    for (int i = 0; i < 4; i++) kit_picture_free(&g_note_pictures[i]);
    kit_picture_free(&g_targets);
    kit_picture_free(&g_hand);
    g_pictures_style = -1;
    g_options.confirm_cross = chosen[0];
    g_options.button_style = chosen[1];
    g_options.note_style = chosen[2];
    controls_apply();
    language_notes_changed();
}

/* ---- the buttons ---------------------------------------------------------------------- */

/* In the window's order. */
static const wchar_t *const BUTTON_NAME[UI_COUNT][PSP_BIND_COUNT] = {
    [UI_ENGLISH] = {L"Up", L"Right", L"Down", L"Left", L"South", L"East", L"West", L"North", L"L", L"R", L"Start", L"Select"},
    [UI_JAPANESE] = {L"\x4e0a", L"\x53f3", L"\x4e0b", L"\x5de6", L"\x4e0b\x30dc\x30bf\x30f3", L"\x53f3\x30dc\x30bf\x30f3", L"\x5de6\x30dc\x30bf\x30f3", L"\x4e0a\x30dc\x30bf\x30f3", L"L", L"R", L"\x30b9\x30bf\x30fc\x30c8", L"\x30bb\x30ec\x30af\x30c8"},
    [UI_GERMAN] = {L"Hoch", L"Rechts", L"Runter", L"Links", L"S\x00fc" L"d", L"Ost", L"West", L"Nord", L"L", L"R", L"Start", L"Select"},
};
static const wchar_t *const WORD_KEYBOARD[UI_COUNT] = {L"Keyboard", L"\x30ad\x30fc\x30dc\x30fc\x30c9", L"Tastatur"};
static const wchar_t *const WORD_KEYBOARD2[UI_COUNT] = {L"Second key", L"\x7b2c\xff12\x30ad\x30fc", L"Zweite Taste"};
static const wchar_t *const WORD_CONTROLLER[UI_COUNT] = {L"Controller", L"\x30b3\x30f3\x30c8\x30ed\x30fc\x30e9\x30fc", L"Controller"};
static const wchar_t *const WORD_RESET[UI_COUNT] = {L"Back to how they were", L"\x521d\x671f\x8a2d\x5b9a\x306b\x623b\x3059", L"Zur\x00fc\x0063ksetzen"};
static const wchar_t *const WORD_DONE[UI_COUNT] = {L"Done", L"\x7d42\x4e86", L"Fertig"};
static const wchar_t *const WORD_PRESS[UI_COUNT] = {L"Press the new one, or wait to keep it as it is", L"\x65b0\x3057\x3044\x30dc\x30bf\x30f3\x3092\x62bc\x3057\x3066\x304f\x3060\x3055\x3044", L"Neue Taste dr\x00fc\x0063ken, oder warten"};
static const wchar_t *const WORD_PRESS2[UI_COUNT] = {L"Press the new one, or the same one to take it off", L"\x65b0\x3057\x3044\x30ad\x30fc\x3092\x62bc\x3057\x3066\x304f\x3060\x3055\x3044\xff08\x540c\x3058\x30ad\x30fc\x3067\x89e3\x9664\xff09", L"Neue Taste dr\x00fc\x0063ken, gleiche Taste entfernt sie"};
static const wchar_t *const WORD_TITLE[UI_COUNT] = {L"Controls", L"\x30dc\x30bf\x30f3\x8a2d\x5b9a", L"Steuerung"};
static const wchar_t *const WORD_BUTTON[UI_COUNT] = {L"Button", L"\x30dc\x30bf\x30f3", L"Taste"};
static const wchar_t *const WORD_CHANGE[UI_COUNT] = {L"Change", L"\x5909\x66f4", L"\x00c4ndern"};
static const wchar_t *const WORD_RESTART[UI_COUNT] = {L"Quick restart", L"\x30af\x30a4\x30c3\x30af\x30ea\x30b9\x30bf\x30fc\x30c8", L"Schnellneustart"};
static const wchar_t *const WORD_BACK[UI_COUNT] = {L"Back", L"\x623b\x308b", L"Zur\x00fc\x0063k"};

/* Where the buttons are, not what is printed on them, so that it holds for every controller. */
static const struct {
    int id;
    const wchar_t *name;
} PAD_NAMES[] = {
    {0, L"Bottom (A)"}, {1, L"Right (B)"}, {2, L"Left (X)"}, {3, L"Top (Y)"}, {4, L"Back"}, {5, L"Guide"},
    {6, L"Start"}, {7, L"Left stick"}, {8, L"Right stick"}, {9, L"LB"}, {10, L"RB"}, {11, L"D-pad up"},
    {12, L"D-pad down"}, {13, L"D-pad left"}, {14, L"D-pad right"}, {15, L"Extra"}, {PSP_PAD_LT, L"LT"}, {PSP_PAD_RT, L"RT"},
};

static void key_name(int vk, wchar_t *out, int size) {
    if (!vk) {
        wcscpy(out, L"-");
        return;
    }
    LONG scan = (LONG)MapVirtualKeyW((UINT)vk, MAPVK_VK_TO_VSC) << 16;
    switch (vk) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT:
    case VK_HOME: case VK_END: case VK_INSERT: case VK_DELETE:
        scan |= 1 << 24;
        break;
    default:
        break;
    }
    if (!GetKeyNameTextW(scan, out, size)) swprintf(out, (size_t)size, L"Key %d", vk);
}

static void pad_name(uint32_t mask, wchar_t *out, size_t size) {
    size_t used = 0;
    out[0] = 0;
    for (size_t i = 0; i < sizeof PAD_NAMES / sizeof PAD_NAMES[0]; i++) {
        if (!(mask >> PAD_NAMES[i].id & 1)) continue;
        used += (size_t)swprintf(out + used, size - used, used ? L" + %ls" : L"%ls", PAD_NAMES[i].name);
    }
    if (!used) wcscpy(out, L"-");
}

/* The PSP's buttons, then the quick restart, which has a key of its own and L and R together on a
 * controller, then the two rows that end the list. */
#define ROWS_TOTAL (PSP_BIND_COUNT + 3)
#define ROW_RESTART PSP_BIND_COUNT
#define ROW_RESET (PSP_BIND_COUNT + 1)
#define ROW_DONE (PSP_BIND_COUNT + 2)
#define LIST_X 24
#define LIST_W 912
#define LIST_Y 98
#define LIST_TOP (LIST_Y + 6)
#define LIST_ROW_H 22
#define LIST_ROW_GAP 2
/* Between the buttons and the two rows that end the list. */
#define LIST_END_GAP 8
#define HEAD_Y 84
#define NAME_X 48
#define CELL_PAD 12
#define CELL_TEXT 15
#define END_W 300
/* What can be changed on a row. */
enum { COLUMN_KEY, COLUMN_KEY2, COLUMN_PAD, COLUMN_COUNT };
static const int CELL_X[COLUMN_COUNT] = {236, 452, 668};
static const int CELL_W[COLUMN_COUNT] = {204, 204, 256};

static int row_y(int i) {
    return LIST_TOP + i * (LIST_ROW_H + LIST_ROW_GAP) + (i >= ROW_RESET ? LIST_END_GAP : 0);
}

static void draw_buttons(int row, int column, int waiting, int ui) {
    /* The table runs across the whole width of the window, its columns spread in step. */
    const int list_w = kit_width() - 2 * LIST_X;
    int cell_x[COLUMN_COUNT], cell_w[COLUMN_COUNT];
    for (int c = 0; c < COLUMN_COUNT; c++) {
        cell_x[c] = LIST_X + (CELL_X[c] - LIST_X) * list_w / LIST_W;
        cell_w[c] = CELL_W[c] * list_w / LIST_W;
    }
    kit_clear(WORD_TITLE[ui]);
    const wchar_t *const heads[COLUMN_COUNT] = {WORD_KEYBOARD[ui], WORD_KEYBOARD2[ui], WORD_CONTROLLER[ui]};
    kit_text(NAME_X, HEAD_Y, 13, 1, KIT_SOFT, WORD_BUTTON[ui], KIT_LEFT);
    for (int c = 0; c < COLUMN_COUNT; c++) {
        kit_text(cell_x[c] + CELL_PAD, HEAD_Y, 13, 1, c == column ? KIT_ACCENT : KIT_SOFT, heads[c], KIT_LEFT);
    }
    kit_panel(LIST_X, LIST_Y, list_w, row_y(ROW_DONE) + LIST_ROW_H + 6 - LIST_Y);
    for (int i = 0; i < ROWS_TOTAL; i++) {
        const int y = row_y(i), mid = y + LIST_ROW_H / 2, on = i == row;
        if (i == ROW_RESET || i == ROW_DONE) {
            if (on) kit_selected((kit_width() - END_W) / 2, y, END_W, LIST_ROW_H);
            kit_text(kit_width() / 2, mid, CELL_TEXT, 1, on ? KIT_INK : KIT_TEXT, i == ROW_RESET ? WORD_RESET[ui] : WORD_DONE[ui], KIT_CENTRE);
            continue;
        }
        if (on) kit_faint(LIST_X + 8, y, list_w - 16, LIST_ROW_H);
        else if (i + 1 != row && i + 1 < ROW_RESET) kit_rule(LIST_X + 24, y + LIST_ROW_H + LIST_ROW_GAP / 2, list_w - 48);
        kit_text(NAME_X, mid, CELL_TEXT, 1, KIT_TEXT, i == ROW_RESTART ? WORD_RESTART[ui] : BUTTON_NAME[ui][i], KIT_LEFT);
        /* The face buttons, in the window's order, with their signs drawn after the name. */
        static const int FACE[4] = {KIT_GLYPH_CROSS, KIT_GLYPH_CIRCLE, KIT_GLYPH_SQUARE, KIT_GLYPH_TRIANGLE};
        if (i >= 4 && i < 8) {
            kit_glyph(NAME_X + kit_text_width(CELL_TEXT, 1, BUTTON_NAME[ui][i]) + 18, mid, 9, FACE[i - 4], KIT_SOFT, KIT_ACCENT);
        }
        for (int c = 0; c < COLUMN_COUNT; c++) {
            wchar_t name[64];
            if (i == ROW_RESTART) {
                /* Its key, and its controller buttons, L and R together until others are chosen. */
                if (c == COLUMN_KEY) key_name(g_options.restart_key, name, 64);
                else if (c == COLUMN_PAD && g_options.restart_pad) pad_name((uint32_t)g_options.restart_pad, name, 64);
                else wcscpy(name, c == COLUMN_PAD ? L"L + R" : L"-");
            } else if (c == COLUMN_PAD) {
                pad_name(psp_window_get_pad(i), name, 64);
            } else {
                key_name(c == COLUMN_KEY ? psp_window_get_key(i) : psp_window_get_key2(i), name, 64);
            }
            uint32_t ink = KIT_SOFT;
            if (on && c == column && waiting) {
                kit_waiting(cell_x[c], y + 1, cell_w[c], LIST_ROW_H - 2);
                ink = KIT_ACCENT;
            } else if (on && c == column) {
                kit_selected(cell_x[c], y + 1, cell_w[c], LIST_ROW_H - 2);
                ink = KIT_INK;
            } else if (on) {
                ink = KIT_TEXT;
            }
            kit_text(cell_x[c] + CELL_PAD, mid, CELL_TEXT, on && c == column, ink, name, KIT_LEFT);
        }
    }
    if (waiting) {
        kit_hint_note(column == COLUMN_KEY2 ? WORD_PRESS2[ui] : WORD_PRESS[ui]);
    } else {
        int right = kit_width() - 32;
        if (g_from_pad) {
            const int cross = g_options.confirm_cross == 1;
            right = kit_hint(right, cross ? KIT_GLYPH_CIRCLE : KIT_GLYPH_CROSS, WORD_BACK[ui]);
            right = kit_hint(right, cross ? KIT_GLYPH_CROSS : KIT_GLYPH_CIRCLE, WORD_CHANGE[ui]);
        } else {
            right = kit_hint_key(right, L"Esc", WORD_BACK[ui]);
            right = kit_hint_key(right, L"Enter", WORD_CHANGE[ui]);
        }
        kit_hint(right, KIT_GLYPH_DPAD, WORD_MOVE[ui]);
    }
}

static int key_of(int button, int second) {
    return second ? psp_window_get_key2(button) : psp_window_get_key(button);
}

static void put_key(int button, int second, int vk) {
    if (second) psp_window_set_key2(button, vk);
    else psp_window_set_key(button, vk);
}

/* The other button that had this key gets what it replaced, so that no key does two things. If it
 * replaced nothing, that button goes on with the key it has left, and if it has no other, the key
 * stays where it is, so that no button is ever left without one. The same second key again takes
 * it off. */
static void set_key(int button, int second, int vk) {
    const int had = key_of(button, second);
    if (vk == had) {
        if (second) put_key(button, 1, 0);
        return;
    }
    if (vk == key_of(button, !second)) return;
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        if (i == button) continue;
        for (int slot = 0; slot < 2; slot++) {
            if (key_of(i, slot) != vk) continue;
            if (had) {
                put_key(i, slot, had);
            } else if (slot) {
                put_key(i, 1, 0);
            } else if (key_of(i, 1)) {
                put_key(i, 0, key_of(i, 1));
                put_key(i, 1, 0);
            } else {
                return;
            }
        }
    }
    put_key(button, second, vk);
}

static void set_pad(int button, int id) {
    const uint32_t had = psp_window_get_pad(button), mask = (uint32_t)1 << id;
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        const uint32_t other = psp_window_get_pad(i);
        if (i != button && (other & mask)) {
            const uint32_t left = other & ~mask;
            psp_window_set_pad(i, left ? left : had);
        }
    }
    psp_window_set_pad(button, mask);
}

/* The quick restart's key: one no button has, or none again with the same key. */
static void capture_restart_key(void) {
    kit_wait_for_release();
    const ULONGLONG until = GetTickCount64() + CAPTURE_MS;
    while (GetTickCount64() < until) {
        kit_frame();
        for (int vk = 0x08; vk <= 0xFE; vk++) {
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || !key_down(vk)) continue;
            if (vk == VK_ESCAPE) return;
            int taken = 0;
            for (int i = 0; i < PSP_BIND_COUNT; i++) taken |= psp_window_get_key(i) == vk || psp_window_get_key2(i) == vk;
            if (!taken) g_options.restart_key = vk == g_options.restart_key ? 0 : vk;
            return;
        }
        Sleep(16);
    }
}

/* The quick restart's controller buttons: all those held together before they are let go. The
 * same again goes back to L and R. */
static void capture_restart_pad(void) {
    kit_wait_for_release();
    const ULONGLONG until = GetTickCount64() + CAPTURE_MS;
    uint32_t chosen = 0;
    while (GetTickCount64() < until) {
        kit_frame();
        if (key_down(VK_ESCAPE)) return;
        const uint32_t pad = psp_window_pad_raw();
        chosen |= pad;
        if (chosen && !pad) {
            g_options.restart_pad = (int)chosen == g_options.restart_pad ? 0 : (int)chosen;
            return;
        }
        Sleep(16);
    }
}

/* Waits for something to be pressed, and gives it to the button. Esc or waiting changes nothing. */
static void capture(int button, int column) {
    kit_wait_for_release();
    const ULONGLONG until = GetTickCount64() + CAPTURE_MS;
    while (GetTickCount64() < until) {
        kit_frame();
        if (column != COLUMN_PAD) {
            /* Past the mouse buttons, and without Shift and such, which have a left and a right. */
            for (int vk = 0x08; vk <= 0xFE; vk++) {
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || !key_down(vk)) continue;
                if (vk != VK_ESCAPE) set_key(button, column == COLUMN_KEY2, vk);
                return;
            }
        } else {
            if (key_down(VK_ESCAPE)) return;
            const uint32_t pad = psp_window_pad_raw();
            for (int id = 0; id < 32; id++) {
                if (pad >> id & 1) {
                    set_pad(button, id);
                    return;
                }
            }
        }
        Sleep(16);
    }
}

static void remember_buttons(void) {
    for (int i = 0; i < PSP_BIND_COUNT; i++) {
        g_options.keys[i] = psp_window_get_key(i);
        g_options.keys2[i] = psp_window_get_key2(i);
        g_options.pads[i] = (int)psp_window_get_pad(i);
    }
}

void controls_change_buttons(void) {
    fprintf(stderr, "controls: the Controls screen opens\n");
    if (!kit_open(g_disc)) {
        fprintf(stderr, "controls: the Controls screen could not open\n");
        return;
    }
    const int ui = language_ui();
    int row = 0, column = 0;
    menu_input was = {1, 1, 1, 1, 1, 1};
    draw_buttons(row, column, 0, ui);
    for (;;) {
        kit_frame();
        const int from_pad = g_from_pad;
        const menu_input in = read_menu(1);
        int changed = 1;
        if (in.up && !was.up) row = (row + ROWS_TOTAL - 1) % ROWS_TOTAL;
        else if (in.down && !was.down) row = (row + 1) % ROWS_TOTAL;
        else if (in.left && !was.left) column = (column + COLUMN_COUNT - 1) % COLUMN_COUNT;
        else if (in.right && !was.right) column = (column + 1) % COLUMN_COUNT;
        else changed = 0;
        const int ok = in.ok && !was.ok, back = in.back && !was.back;
        was = in;
        if (back || (ok && row == ROW_DONE)) break;
        if (ok && row == ROW_RESET) {
            psp_window_reset_buttons();
            g_options.restart_key = 0;
            g_options.restart_pad = 0;
            changed = 1;
        } else if (ok && row == ROW_RESTART) {
            if (column != COLUMN_KEY2) {
                draw_buttons(row, column, 1, ui);
                if (column == COLUMN_KEY) capture_restart_key();
                else capture_restart_pad();
                kit_wait_for_release();
                was = read_menu(1);
            }
            changed = 1;
        } else if (ok) {
            draw_buttons(row, column, 1, ui);
            capture(row, column);
            kit_wait_for_release();
            was = read_menu(1);
            changed = 1;
        }
        if (kit_resized() || changed || from_pad != g_from_pad) draw_buttons(row, column, 0, ui);
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    remember_buttons();
    fprintf(stderr, "controls: the Controls screen closed\n");
}

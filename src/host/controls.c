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
#include "presence.h"

#include "language.h"
#include "options.h"
#include "screen_kit.h"
#include "timing.h"

#include <psprecomp/cpu.h>
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
enum { B_CIRCLE = 0x2000, B_CROSS = 0x4000 };

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
    if (pressed_in_window(VK_F3, &trace_was_down)) psp_render_gl_trace_frame();
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

static uint32_t swap_confirm(uint32_t buttons) {
    press_clears_held_message(buttons);
    compare_with_space();
    compare_image_shapes();
    if (g_options.confirm_cross != 1 || GetTickCount64() - g_song_seen < SONG_GONE_MS) return buttons;
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
    /* Only the changes, to see in the log that this is what it is thought to be. */
    if (playing != was_playing) fprintf(stderr, "controls: %s\n", playing ? "a song is playing" : "the song stopped");
    if (was_playing && !playing) timing_song_stopped();
    /* A pause menu or the results get their confirm button on the very frame they begin. */
    g_song_seen = playing ? now : 0;
    return 1;
}

/* ---- what the screens read ---------------------------------------------------------- */

typedef struct {
    int up, down, left, right, ok, back;
} menu_input;

static menu_input read_menu(void) {
    const uint32_t pad = psp_window_pad_raw();
    menu_input in;
    in.up = key_down(VK_UP) || (pad >> PAD_UP & 1);
    in.down = key_down(VK_DOWN) || (pad >> PAD_DOWN & 1);
    in.left = key_down(VK_LEFT) || (pad >> PAD_LEFT & 1);
    in.right = key_down(VK_RIGHT) || (pad >> PAD_RIGHT & 1);
    in.ok = key_down(VK_RETURN) || (pad >> PAD_SOUTH & 1) || (pad >> PAD_EAST & 1);
    in.back = key_down(VK_ESCAPE);
    return in;
}

/* ---- which button confirms ---------------------------------------------------------- */

static const wchar_t *const ASK_QUESTION[UI_COUNT] = {L"Which button confirms?", L"\x6c7a\x5b9a\x30dc\x30bf\x30f3\x3092\x9078\x3093\x3067\x304f\x3060\x3055\x3044", L"Welche Taste best\x00e4tigt?"};
static const wchar_t *const ASK_NOTE[UI_COUNT] = {L"You can change this later in Options.", L"\x5f8c\x3067\x30aa\x30d7\x30b7\x30e7\x30f3\x304b\x3089\x5909\x66f4\x3067\x304d\x307e\x3059\x3002", L"Sp\x00e4ter in den Optionen \x00e4nderbar."};
static const wchar_t *const ASK_CIRCLE[UI_COUNT] = {L"\x25cb  Circle, as the game was made", L"\x25cb  \x4e38\x30dc\x30bf\x30f3\xff08\x30aa\x30ea\x30b8\x30ca\x30eb\xff09", L"\x25cb  Kreis, wie im Original"};
static const wchar_t *const ASK_CROSS[UI_COUNT] = {L"\x00d7  Cross", L"\x00d7  \x30d0\x30c4\x30dc\x30bf\x30f3", L"\x00d7  Kreuz"};

#define ASK_X 180
#define ASK_W 600
#define ASK_H 70
#define ASK_GAP 18
#define ASK_TOP 250

static void draw_ask(int selected, int ui) {
    kit_clear();
    kit_text(KIT_CANVAS_W / 2, 136, 33, 1, 0xF5F8FC, ASK_QUESTION[ui], 1);
    kit_text(KIT_CANVAS_W / 2, 182, 19, 0, 0xC3D7E4, ASK_NOTE[ui], 1);
    const wchar_t *const rows[2] = {ASK_CIRCLE[ui], ASK_CROSS[ui]};
    for (int i = 0; i < 2; i++) {
        const int y = ASK_TOP + i * (ASK_H + ASK_GAP);
        kit_row(ASK_X, y, ASK_W, ASK_H, 16, i == selected);
        kit_text(ASK_X + 40, y + 18, 30, 1, 0xF5F8FC, rows[i], 0);
    }
}

void controls_ask_confirm(void) {
    if (!kit_open(g_disc)) {
        g_options.confirm_cross = 0;
        return;
    }
    const int ui = language_ui();
    int selected = g_options.confirm_cross == 1;
    menu_input was = {1, 1, 1, 1, 1, 1};
    draw_ask(selected, ui);
    for (;;) {
        kit_frame();
        const menu_input in = read_menu();
        if ((in.up && !was.up) || (in.down && !was.down)) {
            selected = !selected;
            draw_ask(selected, ui);
        }
        const int ok = in.ok && !was.ok;
        was = in;
        if (ok) break;
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    g_options.confirm_cross = selected;
    controls_apply();
}

/* ---- the buttons ---------------------------------------------------------------------- */

/* In the window's order. */
static const wchar_t *const BUTTON_NAME[UI_COUNT][PSP_BIND_COUNT] = {
    [UI_ENGLISH] = {L"Up", L"Right", L"Down", L"Left", L"Cross \x00d7", L"Circle \x25cb", L"Square \x25a1", L"Triangle \x25b3", L"L", L"R", L"Start", L"Select"},
    [UI_JAPANESE] = {L"\x4e0a", L"\x53f3", L"\x4e0b", L"\x5de6", L"\x00d7", L"\x25cb", L"\x25a1", L"\x25b3", L"L", L"R", L"\x30b9\x30bf\x30fc\x30c8", L"\x30bb\x30ec\x30af\x30c8"},
    [UI_GERMAN] = {L"Hoch", L"Rechts", L"Runter", L"Links", L"Kreuz \x00d7", L"Kreis \x25cb", L"Quadrat \x25a1", L"Dreieck \x25b3", L"L", L"R", L"Start", L"Select"},
};
static const wchar_t *const WORD_KEYBOARD[UI_COUNT] = {L"Keyboard", L"\x30ad\x30fc\x30dc\x30fc\x30c9", L"Tastatur"};
static const wchar_t *const WORD_KEYBOARD2[UI_COUNT] = {L"Second key", L"\x7b2c\xff12\x30ad\x30fc", L"Zweite Taste"};
static const wchar_t *const WORD_CONTROLLER[UI_COUNT] = {L"Controller", L"\x30b3\x30f3\x30c8\x30ed\x30fc\x30e9\x30fc", L"Controller"};
static const wchar_t *const WORD_RESET[UI_COUNT] = {L"Back to how they were", L"\x521d\x671f\x8a2d\x5b9a\x306b\x623b\x3059", L"Zur\x00fc\x0063ksetzen"};
static const wchar_t *const WORD_DONE[UI_COUNT] = {L"Done", L"\x7d42\x4e86", L"Fertig"};
static const wchar_t *const WORD_PRESS[UI_COUNT] = {L"Press the new one, or wait to keep it as it is", L"\x65b0\x3057\x3044\x30dc\x30bf\x30f3\x3092\x62bc\x3057\x3066\x304f\x3060\x3055\x3044", L"Neue Taste dr\x00fc\x0063ken, oder warten"};
static const wchar_t *const WORD_PRESS2[UI_COUNT] = {L"Press the new one, or the same one to take it off", L"\x65b0\x3057\x3044\x30ad\x30fc\x3092\x62bc\x3057\x3066\x304f\x3060\x3055\x3044\xff08\x540c\x3058\x30ad\x30fc\x3067\x89e3\x9664\xff09", L"Neue Taste dr\x00fc\x0063ken, gleiche Taste entfernt sie"};
static const wchar_t *const WORD_HELP[UI_COUNT] = {L"Arrows or d-pad: move    Enter, A or B: change", L"\x65b9\x5411\x30ad\x30fc\xff1a\x79fb\x52d5    Enter\x30fb\x30dc\x30bf\x30f3\xff1a\x5909\x66f4", L"Pfeile oder Steuerkreuz: bewegen    Enter, A oder B: \x00e4ndern"};

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

#define ROWS_TOTAL (PSP_BIND_COUNT + 2)
#define ROW_RESET PSP_BIND_COUNT
#define ROW_DONE (PSP_BIND_COUNT + 1)
#define LIST_X 90
#define LIST_W 780
#define LIST_TOP (KIT_TOP + 34)
#define LIST_ROW_H 22
#define LIST_ROW_GAP 2
#define NAME_X 16
#define KEY_X 190
#define KEY2_X 340
#define PAD_X 490
/* What can be changed on a row. */
enum { COLUMN_KEY, COLUMN_KEY2, COLUMN_PAD, COLUMN_COUNT };

static void draw_buttons(int row, int column, int waiting, int ui) {
    kit_clear();
    kit_text(LIST_X + KEY_X, KIT_TOP + 8, 20, 1, column == COLUMN_KEY ? 0x6EFAB9 : 0xC3D7E4, WORD_KEYBOARD[ui], 0);
    kit_text(LIST_X + KEY2_X, KIT_TOP + 8, 20, 1, column == COLUMN_KEY2 ? 0x6EFAB9 : 0xC3D7E4, WORD_KEYBOARD2[ui], 0);
    kit_text(LIST_X + PAD_X, KIT_TOP + 8, 20, 1, column == COLUMN_PAD ? 0x6EFAB9 : 0xC3D7E4, WORD_CONTROLLER[ui], 0);
    for (int i = 0; i < ROWS_TOTAL; i++) {
        const int y = LIST_TOP + i * (LIST_ROW_H + LIST_ROW_GAP);
        kit_row(LIST_X, y, LIST_W, LIST_ROW_H, 6, i == row);
        if (i == ROW_RESET || i == ROW_DONE) {
            kit_text(LIST_X + LIST_W / 2, y + 2, 17, 1, 0xF5F8FC, i == ROW_RESET ? WORD_RESET[ui] : WORD_DONE[ui], 1);
            continue;
        }
        wchar_t name[64];
        kit_text(LIST_X + NAME_X, y + 2, 17, 1, 0xF5F8FC, BUTTON_NAME[ui][i], 0);
        key_name(psp_window_get_key(i), name, 64);
        kit_text(LIST_X + KEY_X, y + 2, 17, 0, i == row && column == COLUMN_KEY ? 0xFFFFFF : 0xC3D7E4, name, 0);
        key_name(psp_window_get_key2(i), name, 64);
        kit_text(LIST_X + KEY2_X, y + 2, 17, 0, i == row && column == COLUMN_KEY2 ? 0xFFFFFF : 0xC3D7E4, name, 0);
        pad_name(psp_window_get_pad(i), name, 64);
        kit_text(LIST_X + PAD_X, y + 2, 17, 0, i == row && column == COLUMN_PAD ? 0xFFFFFF : 0xC3D7E4, name, 0);
    }
    kit_text(KIT_CANVAS_W / 2, KIT_BOTTOM + 12, 20, waiting, waiting ? 0x6EFAB9 : 0xC3D7E4, !waiting ? WORD_HELP[ui] : column == COLUMN_KEY2 ? WORD_PRESS2[ui] : WORD_PRESS[ui], 1);
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
    if (!kit_open(g_disc)) return;
    const int ui = language_ui();
    int row = 0, column = 0;
    menu_input was = {1, 1, 1, 1, 1, 1};
    draw_buttons(row, column, 0, ui);
    for (;;) {
        kit_frame();
        const menu_input in = read_menu();
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
            changed = 1;
        } else if (ok) {
            draw_buttons(row, column, 1, ui);
            capture(row, column);
            kit_wait_for_release();
            was = read_menu();
            changed = 1;
        }
        if (changed) draw_buttons(row, column, 0, ui);
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    remember_buttons();
}

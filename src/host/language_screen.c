/* The question of which language to play in, before the game's title.
 *
 * The game is held while this shows, in a window of its own over the game's, drawn by
 * screen_kit.c. To add a language, add a line to LANGUAGES and a function for its flag.
 *
 * Set language_prompt = 0 to see it only when no choice has been saved.
 */

#include "language_screen.h"

#include "options.h"
#include "screen_kit.h"

#include <psprecomp/window.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

enum { B_START = 0x0008, B_UP = 0x0010, B_DOWN = 0x0040, B_CIRCLE = 0x2000, B_CROSS = 0x4000 };

#define LIST_X 24
#define LIST_Y 104
#define LIST_W 400
#define ROW_H 52
#define ROW_GAP 6
#define ROW_PAD 8
#define FLAG_R 15
#define HERO_X 690
#define HERO_Y 196
#define HERO_R 72
#define CARD_X 500
#define CARD_Y 316
#define CARD_W 436
#define CARD_H 164

static void set(double *r, double *g, double *b, int cr, int cg, int cb) {
    *r = cr;
    *g = cg;
    *b = cb;
}

/* x and y run from -1 to 1 across the flag. */
static void flag_uk(double x, double y, double *r, double *g, double *b) {
    const double ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    const double d1 = x - y < 0 ? y - x : x - y, d2 = x + y < 0 ? -(x + y) : x + y;
    set(r, g, b, 1, 33, 105);
    if ((ax - ay < 0 ? ay - ax : ax - ay) < 0.22) set(r, g, b, 255, 255, 255);
    if (d1 < 0.09 || d2 < 0.09) set(r, g, b, 200, 16, 46);
    if (ax < 0.33 || ay < 0.33) set(r, g, b, 255, 255, 255);
    if (ax < 0.2 || ay < 0.2) set(r, g, b, 200, 16, 46);
}

static void flag_japan(double x, double y, double *r, double *g, double *b) {
    if (x * x + y * y < 0.3) set(r, g, b, 188, 0, 45);
    else set(r, g, b, 245, 245, 245);
}

static void flag_germany(double x, double y, double *r, double *g, double *b) {
    (void)x;
    if (y < -1.0 / 3) set(r, g, b, 10, 10, 10);
    else if (y < 1.0 / 3) set(r, g, b, 221, 0, 0);
    else set(r, g, b, 255, 206, 0);
}

static const struct {
    const char *code;
    const wchar_t *name;
    kit_flag_fn flag;
    const wchar_t *question;
    const wchar_t *note[4];
    const wchar_t *select, *move;
} LANGUAGES[] = {
    {"en", L"English", flag_uk, L"Select a language",
     {L"Menus and dialogue will use this", L"language. Songs keep their", L"Japanese audio.", L"You can change it later in Options."},
     L"Select", L"Move"},
    {"ja", L"日本語", flag_japan, L"言語を選択してください",
     {L"メニューと会話の言語を設定します。", L"楽曲の音声は日本語のままです。", L"", L"オプションで後から変更できます。"},
     L"決定", L"移動"},
    {"de", L"Deutsch", flag_germany, L"Sprache wählen",
     {L"Menüs und Dialoge nutzen diese", L"Sprache. Die Lieder bleiben", L"auf Japanisch.", L"Später unter Optionen änderbar."},
     L"Auswählen", L"Bewegen"},
};
#define NUM_LANGUAGES ((int)(sizeof LANGUAGES / sizeof LANGUAGES[0]))

static void draw_screen(int selected) {
    kit_clear(LANGUAGES[selected].question);
    /* The same question in another language, for whoever cannot read this one. */
    kit_text(LIST_X + 16, 82, 14, 0, KIT_SOFT, LANGUAGES[selected == 0 ? 1 : 0].question, KIT_LEFT);

    kit_panel(LIST_X, LIST_Y, LIST_W, ROW_PAD * 2 + NUM_LANGUAGES * (ROW_H + ROW_GAP) - ROW_GAP);
    const int row_x = LIST_X + ROW_PAD, row_w = LIST_W - ROW_PAD * 2;
    for (int i = 0; i < NUM_LANGUAGES; i++) {
        const int y = LIST_Y + ROW_PAD + i * (ROW_H + ROW_GAP), on = i == selected;
        if (on) kit_selected(row_x, y, row_w, ROW_H);
        else if (i + 1 != selected && i + 1 < NUM_LANGUAGES) kit_rule(row_x + 16, y + ROW_H + ROW_GAP / 2, row_w - 32);
        kit_flag(row_x + 30, y + ROW_H / 2, FLAG_R, LANGUAGES[i].flag);
        kit_text(row_x + 58, y + ROW_H / 2, 19, 1, on ? KIT_INK : KIT_TEXT, LANGUAGES[i].name, KIT_LEFT);
    }

    /* The list keeps to the left edge, and what it shows to the right one. */
    const int hero_x = kit_right(HERO_X), card_x = kit_right(CARD_X);
    kit_halo(hero_x, HERO_Y, HERO_R * 2);
    kit_flag(hero_x, HERO_Y, HERO_R, LANGUAGES[selected].flag);

    kit_panel(card_x, CARD_Y, CARD_W, CARD_H);
    kit_mark(card_x + 24, CARD_Y + 22);
    kit_text(card_x + 24, CARD_Y + 48, 14, 0, KIT_SOFT, LANGUAGES[selected].name, KIT_LEFT);
    for (int i = 0; i < 4; i++) {
        kit_text(card_x + 24, CARD_Y + 78 + i * 22, 16, 0, KIT_TEXT, LANGUAGES[selected].note[i], KIT_LEFT);
    }

    int right = kit_width() - 32;
    /* The button the player confirms with, Circle until they have chosen. */
    right = kit_hint(right, g_options.confirm_cross == 1 ? KIT_GLYPH_CROSS : KIT_GLYPH_CIRCLE, LANGUAGES[selected].select);
    kit_hint(right, KIT_GLYPH_DPAD, LANGUAGES[selected].move);
}

const char *language_screen_run(const char *current, const char *disc) {
    int selected = 0;
    for (int i = 0; i < NUM_LANGUAGES; i++) {
        if (!strcmp(LANGUAGES[i].code, current)) selected = i;
    }
    if (!kit_open(disc)) return current;
    fprintf(stderr, "language screen: shown\n");

    draw_screen(selected);
    /* A key already down from skipping an intro is not an answer. */
    uint32_t held = ~(uint32_t)0;
    int chosen = 0;
    while (!chosen) {
        kit_frame();
        const uint32_t buttons = psp_window_buttons();
        const uint32_t pressed = buttons & ~held;
        held = buttons;
        int changed = 0;
        if (pressed & B_UP) {
            selected = (selected + NUM_LANGUAGES - 1) % NUM_LANGUAGES;
            changed = 1;
        }
        if (pressed & B_DOWN) {
            selected = (selected + 1) % NUM_LANGUAGES;
            changed = 1;
        }
        if (changed || kit_resized()) draw_screen(selected);
        if (pressed & (B_START | B_CIRCLE | B_CROSS)) chosen = 1;
        Sleep(16);
    }
    /* The press that answered is not for the title screen that comes next. */
    kit_wait_for_release();
    kit_close();
    return LANGUAGES[selected].code;
}

/* The system's list of saves and its keyboard. See system_dialogs.h. */
#include "system_dialogs.h"

#include "controls.h"
#include "edit_share.h"
#include "language.h"
#include "options.h"
#include "screen_kit.h"

#include <psprecomp/hle.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

/* ---- words ------------------------------------------------------------------------------- */
static const wchar_t *const TITLE_SAVE[UI_COUNT] = {L"Save", L"\x30bb\x30fc\x30d6", L"Speichern"};
static const wchar_t *const TITLE_LOAD[UI_COUNT] = {L"Load", L"\x30ed\x30fc\x30c9", L"Laden"};
static const wchar_t *const TITLE_DELETE[UI_COUNT] = {L"Delete", L"\x524a\x9664", L"L\x00f6schen"};
static const wchar_t *const NEW_DATA[UI_COUNT] = {L"New data", L"\x65b0\x3057\x3044\x30c7\x30fc\x30bf", L"Neue Daten"};
static const wchar_t *const ASK_OVERWRITE[UI_COUNT] = {L"Save over this data?", L"\x3053\x306e\x30c7\x30fc\x30bf\x306b\x4e0a\x66f8\x304d\x3057\x307e\x3059\x304b\xff1f",
                                                       L"Diese Daten \x00fc\x0062\x0065rschreiben?"};
static const wchar_t *const ASK_DELETE[UI_COUNT] = {L"Delete this data?", L"\x3053\x306e\x30c7\x30fc\x30bf\x3092\x524a\x9664\x3057\x307e\x3059\x304b\xff1f",
                                                    L"Diese Daten l\x00f6schen?"};
static const wchar_t *const WORD_YES[UI_COUNT] = {L"Yes", L"\x306f\x3044", L"Ja"};
static const wchar_t *const WORD_NO[UI_COUNT] = {L"No", L"\x3044\x3044\x3048", L"Nein"};
static const wchar_t *const WORD_CHOOSE[UI_COUNT] = {L"Select", L"\x6c7a\x5b9a", L"Ausw\x00e4hlen"};
static const wchar_t *const WORD_BACK[UI_COUNT] = {L"Back", L"\x623b\x308b", L"Zur\x00fc\x0063k"};
static const wchar_t *const WORD_MOVE[UI_COUNT] = {L"Move", L"\x79fb\x52d5", L"Bewegen"};
static const wchar_t *const TITLE_KEYBOARD[UI_COUNT] = {L"Enter text", L"\x6587\x5b57\x5165\x529b", L"Texteingabe"};
static const wchar_t *const WORD_SPACE[UI_COUNT] = {L"Space", L"\x30b9\x30da\x30fc\x30b9", L"Leerzeichen"};
static const wchar_t *const WORD_ERASE[UI_COUNT] = {L"Erase", L"\x4e00\x6587\x5b57\x524a\x9664", L"Zeichen l\x00f6schen"};
static const wchar_t *const WORD_DONE[UI_COUNT] = {L"Done", L"\x5b8c\x4e86", L"Fertig"};
static const wchar_t *const WORD_EXPORT[UI_COUNT] = {L"Export song", L"\x66f2\x3092\x66f8\x304d\x51fa\x3057", L"Lied exportieren"};
static const wchar_t *const WORD_TYPE[UI_COUNT] = {L"Type", L"\x5165\x529b", L"Tippen"};

/* ---- what both screens share --------------------------------------------------------------- */
static int confirm_glyph(void) { return g_options.confirm_cross == 1 ? KIT_GLYPH_CROSS : KIT_GLYPH_CIRCLE; }
static int back_glyph(void) { return g_options.confirm_cross == 1 ? KIT_GLYPH_CIRCLE : KIT_GLYPH_CROSS; }

/* The buttons at the bottom right, by what was pressed last. Gives back where the next goes. */
static int hints_choose_back(int ui) {
    int right = kit_width() - 32;
    if (controls_screen_from_pad()) {
        right = kit_hint(right, back_glyph(), WORD_BACK[ui]);
        right = kit_hint(right, confirm_glyph(), WORD_CHOOSE[ui]);
    } else {
        right = kit_hint_key(right, L"Esc", WORD_BACK[ui]);
        right = kit_hint_key(right, L"Enter", WORD_CHOOSE[ui]);
    }
    return kit_hint(right, KIT_GLYPH_DPAD, WORD_MOVE[ui]);
}

static void wide(const char *utf8, wchar_t *out, int cap) {
    out[0] = 0;
    if (utf8 && *utf8 && !MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap)) out[0] = 0;
    out[cap - 1] = 0;
}

/* The first line of a save's detail, which may hold several. */
static void first_line(const char *utf8, wchar_t *out, int cap) {
    wide(utf8, out, cap);
    for (wchar_t *p = out; *p; p++) {
        if (*p == L'\n' || *p == L'\r') {
            *p = 0;
            break;
        }
    }
}

/* What a row shows of a save. Rhythm Game Edit's detail is four lines, a label over each name:
 * 『楽曲名』, the song, 『エディット名』, the Edit's name. Its name goes in large and the song under
 * it. Any other save shows its title and the first line of its detail. */
static void save_names(const psp_save_slot *s, wchar_t *name, int name_cap, wchar_t *small, int small_cap) {
    wchar_t all[1100], *line[8] = {0};
    int lines = 0;
    wide(s->detail, all, 1100);
    for (wchar_t *p = all; *p && lines < 8;) {
        line[lines++] = p;
        while (*p && *p != L'\n' && *p != L'\r') p++;
        while (*p == L'\n' || *p == L'\r') *p++ = 0;
    }
    if (lines >= 4 && line[0][0] == 0x300e && line[2][0] == 0x300e) {   /* 『 */
        wcsncpy(name, line[3], name_cap - 1);
        name[name_cap - 1] = 0;
        wcsncpy(small, line[1], small_cap - 1);
        small[small_cap - 1] = 0;
        return;
    }
    wide(s->title[0] ? s->title : s->name, name, name_cap);
    first_line(s->detail, small, small_cap);
}

/* ---- the list of saves --------------------------------------------------------------------- */
#define LIST_X 72
#define LIST_Y 92
#define LIST_W 816
#define ROW_H 62
#define ROW_GAP 6
#define ROWS_SHOWN 5   /* ends at 438, with room for the arrow under it, above the strip at 492 */
#define MOST_ROWS 1001

static int g_rows[MOST_ROWS];     /* each row's slot, -1 for new data */
static int g_row_count;
static wchar_t g_message[400];    /* what came of an export or an import, under the list */

static void draw_list(int purpose, const psp_save_slot *slots, int top, int chosen, int ui) {
    const wchar_t *const *title = purpose == PSP_SAVE_CHOOSE_SAVE ? TITLE_SAVE : purpose == PSP_SAVE_CHOOSE_LOAD ? TITLE_LOAD : TITLE_DELETE;
    kit_clear(title[ui]);
    const int w = kit_width() - 2 * LIST_X;
    kit_panel(LIST_X, LIST_Y, w, ROWS_SHOWN * (ROW_H + ROW_GAP) + ROW_GAP);
    for (int r = top; r < g_row_count && r < top + ROWS_SHOWN; r++) {
        const int y = LIST_Y + ROW_GAP + (r - top) * (ROW_H + ROW_GAP), on = r == chosen;
        const int x = LIST_X + 12, rw = w - 24;
        if (on) kit_selected(x, y, rw, ROW_H);
        const uint32_t ink = on ? KIT_INK : KIT_TEXT, soft = on ? KIT_INK : KIT_SOFT;
        if (g_rows[r] < 0) {
            kit_text(x + 24, y + ROW_H / 2, 20, 1, ink, NEW_DATA[ui], KIT_LEFT);
            continue;
        }
        const psp_save_slot *s = &slots[g_rows[r]];
        wchar_t name[160], detail[200], when[40];
        save_names(s, name, 160, detail, 200);
        when[0] = 0;
        if (s->time) {
            const time_t t = (time_t)s->time;
            struct tm local;
            localtime_s(&local, &t);
            wcsftime(when, 40, L"%Y-%m-%d  %H:%M", &local);
        }
        kit_text(x + 24, y + 20, 18, 1, ink, name, KIT_LEFT);
        kit_text(x + 24, y + 44, 14, 0, soft, detail, KIT_LEFT);
        kit_text(x + rw - 24, y + 20, 14, 0, soft, when, KIT_RIGHT);
    }
    /* Arrows where there is more above or below. */
    if (top > 0) kit_text(kit_width() / 2, LIST_Y - 12, 14, 1, KIT_ACCENT, L"\x25b2", KIT_CENTRE);
    if (top + ROWS_SHOWN < g_row_count)
        kit_text(kit_width() / 2, LIST_Y + ROWS_SHOWN * (ROW_H + ROW_GAP) + ROW_GAP + 14, 14, 1, KIT_ACCENT, L"\x25bc", KIT_CENTRE);
    if (g_message[0]) kit_text(kit_width() / 2, 470, 14, 1, KIT_ACCENT, g_message, KIT_CENTRE);
    const int right = hints_choose_back(ui);
    /* A song's Edits, and its MP3 if it is the player's own, can be exported from the lists of
     * Rhythm Game Edit's data. */
    if (purpose != PSP_SAVE_CHOOSE_SAVE && chosen < g_row_count && g_rows[chosen] >= 0 && !strncmp(slots[g_rows[chosen]].name, "EDIT", 4)) {
        if (controls_screen_from_pad()) kit_hint(right, KIT_GLYPH_TRIANGLE, WORD_EXPORT[ui]);
        else kit_hint_key(right, L"E", WORD_EXPORT[ui]);
    }
}

/* Triangle on a controller or E on the keyboard, just pressed. */
static int export_pressed(void) {
    static int was = 1;
    const int now = (psp_window_pad_raw() >> 3 & 1) || (GetAsyncKeyState('E') & 0x8000);
    const int pressed = now && !was;
    was = now;
    return pressed;
}

/* Yes or no over the list. Gives back 1 for yes. */
static int ask_yes_no(const wchar_t *question, int ui) {
    int yes = 0;
    controls_input was = {1, 1, 1, 1, 1, 1};
    int redraw = 1;
    for (;;) {
        if (redraw) {
            const int w = 520, h = 190, x = (kit_width() - w) / 2, y = 170;
            kit_panel(x, y, w, h);
            kit_text(x + w / 2, y + 52, 20, 1, KIT_TEXT, question, KIT_CENTRE);
            for (int i = 0; i < 2; i++) {
                const int bx = x + 60 + i * 210, by = y + 104, on = (i == 0) == yes;
                if (on) kit_selected(bx, by, 190, 52);
                else kit_faint(bx, by, 190, 52);
                kit_text(bx + 95, by + 26, 18, 1, on ? KIT_INK : KIT_TEXT, i == 0 ? WORD_YES[ui] : WORD_NO[ui], KIT_CENTRE);
            }
            redraw = 0;
        }
        kit_frame();
        const controls_input in = controls_screen_input();
        if ((in.left && !was.left) || (in.right && !was.right)) {
            yes = !yes;
            redraw = 1;
        }
        const int ok = in.ok && !was.ok, back = in.back && !was.back;
        was = in;
        if (ok || back) {
            kit_wait_for_release();
            return ok && yes;
        }
        if (kit_resized()) redraw = 1;
        Sleep(16);
    }
}

static int choose_save(int purpose, const psp_save_slot *slots, int count, const char *new_title, const char *new_detail) {
    (void)new_title;
    (void)new_detail;
    /* The game's own save names three slots, MAIN00 to MAIN02, and keeps to the first: it is saved
     * and loaded as before, with no list. The list is for Rhythm Game Edit's data. */
    int main_save = 1;
    for (int i = 0; i < count && main_save; i++) main_save = strncmp(slots[i].name, "MAIN", 4) == 0;
    if (main_save) return PSP_SAVE_CHOOSE_DEFAULT;
    /* Before the list to load from, what waits in the Import folder is brought in; if anything came,
     * the slots are read again and this is asked again, with what came of it shown. */
    static int keep_message;
    if (!keep_message) g_message[0] = 0;
    keep_message = 0;
    if (purpose == PSP_SAVE_CHOOSE_LOAD) {
        wchar_t said[400];
        const int brought = edit_share_import(slots, count, said, 400);
        if (said[0]) swprintf(g_message, 400, L"%ls", said);
        if (brought > 0) {
            keep_message = 1;
            return PSP_SAVE_CHOOSE_AGAIN;
        }
    }
    /* To save: new data, in the first slot left empty, then what is there to save over. To load or
     * delete: only what holds data. */
    g_row_count = 0;
    if (purpose == PSP_SAVE_CHOOSE_SAVE) {
        for (int i = 0; i < count; i++) {
            if (!slots[i].used) {
                g_rows[g_row_count++] = -1 - i;
                break;
            }
        }
    }
    /* The list to delete from is of every folder of the game's: only Rhythm Game Edit's data is
     * kept in it, so that neither the player's progress (MAIN) nor this port's options (RECOMP)
     * can be lost from its screen. */
    for (int i = 0; i < count && g_row_count < MOST_ROWS; i++)
        if (slots[i].used && (purpose != PSP_SAVE_CHOOSE_DELETE || strncmp(slots[i].name, "EDIT", 4) == 0)) g_rows[g_row_count++] = i;
    if (!g_row_count) return PSP_SAVE_CHOOSE_DEFAULT;
    if (!kit_open(controls_disc())) {
        fprintf(stderr, "saves: the list could not open, so the first one is taken\n");
        return g_rows[0] < 0 ? -1 - g_rows[0] : g_rows[0];
    }
    /* New data is shown by its own row, its slot kept apart. */
    int new_slot = -1;
    if (g_row_count && g_rows[0] < 0) {
        new_slot = -1 - g_rows[0];
        g_rows[0] = -1;
    }
    const int ui = language_ui();
    int chosen = 0, top = 0, result = -1;
    controls_input was = {1, 1, 1, 1, 1, 1};
    int redraw = 1;
    fprintf(stderr, "saves: the list opens, %d rows\n", g_row_count);
    export_pressed();
    for (;;) {
        if (redraw) {
            if (chosen < top) top = chosen;
            if (chosen >= top + ROWS_SHOWN) top = chosen - ROWS_SHOWN + 1;
            draw_list(purpose, slots, top, chosen, ui);
            redraw = 0;
        }
        kit_frame();
        const int from_pad = controls_screen_from_pad();
        const controls_input in = controls_screen_input();
        if (in.up && !was.up) chosen = (chosen + g_row_count - 1) % g_row_count, redraw = 1;
        if (in.down && !was.down) chosen = (chosen + 1) % g_row_count, redraw = 1;
        const int ok = in.ok && !was.ok, back = in.back && !was.back;
        was = in;
        if (back) break;
        if (export_pressed() && purpose != PSP_SAVE_CHOOSE_SAVE && g_rows[chosen] >= 0 && !strncmp(slots[g_rows[chosen]].name, "EDIT", 4)) {
            edit_share_export(slots, count, g_rows[chosen], g_message, 400);
            redraw = 1;
        }
        if (ok) {
            kit_wait_for_release();
            const int slot = g_rows[chosen] < 0 ? new_slot : g_rows[chosen];
            int sure = 1;
            if (purpose == PSP_SAVE_CHOOSE_SAVE && g_rows[chosen] >= 0) sure = ask_yes_no(ASK_OVERWRITE[ui], ui);
            else if (purpose == PSP_SAVE_CHOOSE_DELETE) sure = ask_yes_no(ASK_DELETE[ui], ui);
            if (sure) {
                result = slot;
                break;
            }
            was = (controls_input){1, 1, 1, 1, 1, 1};
            redraw = 1;
        }
        if (kit_resized() || from_pad != controls_screen_from_pad()) redraw = 1;
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    fprintf(stderr, "saves: the list closes, %s\n", result < 0 ? "backed out" : slots[result].name);
    return result;
}

/* ---- the keyboard --------------------------------------------------------------------------- */
static const char *const KEY_ROWS[] = {
    "ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "abcdefghijklm", "nopqrstuvwxyz", "0123456789-_.", "!?&'()+,:;=@#",
};
#define KEY_LINES (int)(sizeof KEY_ROWS / sizeof KEY_ROWS[0])
#define KEY_COLUMNS 13
#define SPECIALS 3        /* space, erase, done, on the last line */
#define KEY_W 56
#define KEY_H 40
#define KEY_GAP 5
#define KEYS_Y 162   /* the last line ends at 472, above the strip of hints at 492 */

static void draw_keyboard(const wchar_t *description, const wchar_t *text, int limit, int line, int column, int ui) {
    kit_clear(TITLE_KEYBOARD[ui]);
    const int full_w = KEY_COLUMNS * (KEY_W + KEY_GAP) - KEY_GAP, x0 = (kit_width() - full_w) / 2;
    kit_text(x0, 82, 16, 0, KIT_SOFT, description, KIT_LEFT);
    kit_panel(x0, 98, full_w, 52);
    wchar_t shown[300];
    swprintf(shown, 300, L"%ls_", text);
    kit_text(x0 + 20, 124, 22, 1, KIT_TEXT, shown, KIT_LEFT);
    wchar_t count[32];
    swprintf(count, 32, L"%d / %d", (int)wcslen(text), limit);
    kit_text(x0 + full_w - 20, 124, 14, 0, KIT_SOFT, count, KIT_RIGHT);
    for (int l = 0; l < KEY_LINES; l++) {
        for (int c = 0; c < KEY_COLUMNS && KEY_ROWS[l][c]; c++) {
            const int x = x0 + c * (KEY_W + KEY_GAP), y = KEYS_Y + l * (KEY_H + KEY_GAP), on = l == line && c == column;
            if (on) kit_selected(x, y, KEY_W, KEY_H);
            else kit_faint(x, y, KEY_W, KEY_H);
            const wchar_t ch[2] = {(wchar_t)KEY_ROWS[l][c], 0};
            kit_text(x + KEY_W / 2, y + KEY_H / 2, 18, 1, on ? KIT_INK : KIT_TEXT, ch, KIT_CENTRE);
        }
    }
    const wchar_t *const words[SPECIALS] = {WORD_SPACE[ui], WORD_ERASE[ui], WORD_DONE[ui]};
    const int sw = (full_w - (SPECIALS - 1) * KEY_GAP) / SPECIALS, y = KEYS_Y + KEY_LINES * (KEY_H + KEY_GAP);
    for (int i = 0; i < SPECIALS; i++) {
        const int x = x0 + i * (sw + KEY_GAP), on = line == KEY_LINES && column == i;
        if (on) kit_selected(x, y, sw, KEY_H);
        else kit_faint(x, y, sw, KEY_H);
        kit_text(x + sw / 2, y + KEY_H / 2, 18, 1, on ? KIT_INK : KIT_TEXT, words[i], KIT_CENTRE);
    }
    int right = hints_choose_back(ui);
    if (!controls_screen_from_pad()) kit_hint_key(right, L"A-Z", WORD_TYPE[ui]);
}

/* A key of the computer's keyboard just pressed, as the character it types, 0 if none. Backspace
 * comes as '\b'. */
static wchar_t typed(void) {
    static BYTE was[256];
    BYTE now[256];
    for (int vk = 0; vk < 256; vk++) now[vk] = (GetAsyncKeyState(vk) & 0x8000) ? 0x80 : 0;
    wchar_t result = 0;
    const int shift = now[VK_SHIFT] != 0;
    for (int vk = 0; vk < 256 && !result; vk++) {
        if (!now[vk] || was[vk]) continue;
        if (vk >= 'A' && vk <= 'Z') result = (wchar_t)(shift ? vk : vk + 32);
        else if (vk >= '0' && vk <= '9' && !shift) result = (wchar_t)vk;
        else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) result = (wchar_t)('0' + vk - VK_NUMPAD0);
        else if (vk == VK_SPACE) result = L' ';
        else if (vk == VK_BACK) result = L'\b';
        else if (vk == VK_OEM_MINUS) result = shift ? L'_' : L'-';
        else if (vk == VK_OEM_PERIOD) result = L'.';
    }
    memcpy(was, now, sizeof was);
    return result;
}

static int keyboard(const uint16_t *description16, const uint16_t *initial, int limit, uint16_t *out) {
    wchar_t description[256], text[257];
    int n = 0;
    for (; description16[n] && n < 255; n++) description[n] = (wchar_t)description16[n];
    description[n] = 0;
    n = 0;
    for (; initial[n] && n < limit && n < 256; n++) text[n] = (wchar_t)initial[n];
    text[n] = 0;
    if (!kit_open(controls_disc())) {
        fprintf(stderr, "keyboard: could not open, so the text stays as it was\n");
        return 0;
    }
    const int ui = language_ui();
    int line = 0, column = 0, entered = 0;
    controls_input was = {1, 1, 1, 1, 1, 1};
    typed();
    int redraw = 1;
    for (;;) {
        if (redraw) {
            draw_keyboard(description, text, limit, line, column, ui);
            redraw = 0;
        }
        kit_frame();
        const int from_pad = controls_screen_from_pad();
        const controls_input in = controls_screen_input();
        const int columns = line == KEY_LINES ? SPECIALS : KEY_COLUMNS;
        if (in.up && !was.up) line = (line + KEY_LINES) % (KEY_LINES + 1), redraw = 1;
        if (in.down && !was.down) line = (line + 1) % (KEY_LINES + 1), redraw = 1;
        if (in.left && !was.left) column = (column + columns - 1) % columns, redraw = 1;
        if (in.right && !was.right) column = (column + 1) % columns, redraw = 1;
        if (line == KEY_LINES && column >= SPECIALS) column = SPECIALS - 1;
        const int ok = in.ok && !was.ok, back = in.back && !was.back;
        was = in;
        int len = (int)wcslen(text);
        /* The computer's keyboard types straight in; Enter finishes and Esc backs out. */
        const wchar_t key = typed();
        if (key == L'\b' && len) text[--len] = 0, redraw = 1;
        else if (key && key != L'\b' && len < limit) text[len++] = key, text[len] = 0, redraw = 1;
        if (!controls_screen_from_pad() && (GetAsyncKeyState(VK_RETURN) & 0x8000) && ok) {
            entered = 1;
            break;
        }
        if (back) {
            /* On a controller, back erases, and backs out once there is nothing left. */
            if (controls_screen_from_pad() && len) text[--len] = 0, redraw = 1;
            else break;
        }
        if (ok) {
            if (line < KEY_LINES) {
                if (len < limit) text[len++] = (wchar_t)KEY_ROWS[line][column], text[len] = 0;
            } else if (column == 0) {
                if (len < limit) text[len++] = L' ', text[len] = 0;
            } else if (column == 1) {
                if (len) text[--len] = 0;
            } else {
                entered = 1;
                break;
            }
            redraw = 1;
        }
        if (kit_resized() || from_pad != controls_screen_from_pad()) redraw = 1;
        Sleep(16);
    }
    kit_wait_for_release();
    kit_close();
    if (entered) {
        /* The game asks for its Japanese keyboard, which gives letters, digits and signs at full
         * width, and it keeps only those: an ASCII letter is left out of the name it makes. */
        int i = 0;
        for (; text[i] && i < limit; i++) {
            const wchar_t c = text[i];
            out[i] = c == L' ' ? 0x3000 : (c > 0x20 && c < 0x7F) ? (uint16_t)(c + 0xFEE0) : (uint16_t)c;
        }
        out[i] = 0;
    }
    fprintf(stderr, "keyboard: %s\n", entered ? "text entered" : "backed out");
    return entered;
}

/* ---- the system's messages ------------------------------------------------------------------ */
/* The game asks for the system's message only for an error, by its number, as the Memory Stick
 * being full. It is told over the game, as "Game saved" is, held until a button is pressed; the
 * game goes on as after its OK. */
static const wchar_t *const SAID_FULL[UI_COUNT] = {L"The Memory Stick is full.", L"\x30e1\x30e2\x30ea\x30fc\x30b9\x30c6\x30a3\x30c3\x30af\x306e\x7a7a\x304d\x304c\x3042\x308a\x307e\x305b\x3093\x3002",
                                                   L"Der Memory Stick ist voll."};
static const wchar_t *const SAID_BROKEN[UI_COUNT] = {L"The save data is damaged.", L"\x30bb\x30fc\x30d6\x30c7\x30fc\x30bf\x304c\x58ca\x308c\x3066\x3044\x307e\x3059\x3002",
                                                     L"Der Spielstand ist besch\x00e4" L"digt."};
static const wchar_t *const SAID_NO_DATA[UI_COUNT] = {L"There is no save data.", L"\x30bb\x30fc\x30d6\x30c7\x30fc\x30bf\x304c\x3042\x308a\x307e\x305b\x3093\x3002",
                                                      L"Kein Spielstand vorhanden."};
static const wchar_t *const SAID_ACCESS[UI_COUNT] = {L"The Memory Stick could not be read or written.",
                                                     L"\x30e1\x30e2\x30ea\x30fc\x30b9\x30c6\x30a3\x30c3\x30af\x306b\x30a2\x30af\x30bb\x30b9\x3067\x304d\x307e\x305b\x3093\x3002",
                                                     L"Der Memory Stick konnte nicht gelesen oder beschrieben werden."};
static const wchar_t *const SAID_ERROR[UI_COUNT] = {L"An error occurred.", L"\x30a8\x30e9\x30fc\x304c\x767a\x751f\x3057\x307e\x3057\x305f\x3002",
                                                    L"Ein Fehler ist aufgetreten."};
static const wchar_t *const SAID_PRESS[UI_COUNT] = {L"Press a button.", L"\x30dc\x30bf\x30f3\x3092\x62bc\x3057\x3066\x304f\x3060\x3055\x3044\x3002",
                                                    L"Taste dr\x00fc" L"cken."};

static int message(uint32_t error, const char *text, uint32_t options) {
    (void)options;
    const int ui = language_ui();
    wchar_t said[700];
    if (text && text[0]) {
        wchar_t t[600];
        wide(text, t, 600);
        swprintf(said, 700, L"%ls %ls", t, SAID_PRESS[ui]);
    } else {
        /* The save errors go by their last byte: 0x8011 03 then what was done and what went wrong. */
        const int save = (error & 0xFFFFFF00u) == 0x80110300u, what = (int)(error & 0x0F);
        const wchar_t *why = SAID_ERROR[ui];
        if (save && what == 3) why = SAID_FULL[ui];
        else if (save && what == 6) why = SAID_BROKEN[ui];
        else if (save && (what == 7 || what == 9)) why = SAID_NO_DATA[ui];
        else if (save) why = SAID_ACCESS[ui];
        swprintf(said, 700, L"%ls (%08X) %ls", why, (unsigned)error, SAID_PRESS[ui]);
    }
    char utf8[2100];
    WideCharToMultiByte(CP_UTF8, 0, said, -1, utf8, sizeof utf8, NULL, NULL);
    psp_render_gl_show_held_message(utf8);
    return PSP_MSG_YES;
}

void system_dialogs_start(void) {
    psp_savedata_set_chooser(choose_save);
    psp_utility_set_osk_handler(keyboard);
    psp_utility_set_message_handler(message);
    /* For a test: DIVA_TEST_MESSAGE=<error number in hex> tells it at the start. */
    const char *test = getenv("DIVA_TEST_MESSAGE");
    if (test) message((uint32_t)strtoul(test, NULL, 16), "", 0);
}

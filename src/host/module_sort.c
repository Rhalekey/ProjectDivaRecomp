/* The modules (Miku's looks) in another order, chosen with Select on their screen: as the game has
 * them, by name, those obtained first, or those last played in first.
 *
 * The modules' screen is a scene of Miku's Room (MyRoom::CMyRoom::CModule). Its list is one of
 * the game's window lists (see song_sort.c), made as the screen opens from a table of the modules
 * in order (MODULE_ORDER), a row for each place, the row's number its module and its words the
 * module's name. Every part of the game turns a place into a module through that table, the
 * pictures beside the list too. The scene's own functions run only as it opens and closes, so the
 * list is looked for each time the game reads the buttons (module_sort_input, from controls.c),
 * found with memory alone as the game finds it; its rows are put in the order chosen, by their
 * modules, the cursor kept on its module, and the table is written in the same order.
 *
 * Addresses are for ULJM05472 v1.01, which Tools\build.ps1 checks.
 */
#include "module_sort.h"
#include "guest.h"

#include "song_sort.h"
#include "translation.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MODULE_ORDER 0x08B2727Cu    /* 53 words: the module at each place */
#define MODULE_NAMES 0x08B25B68u    /* 53 words: each module's name */
#define MODULES 53
#define SAVE 0x08C2C4A0u
#define HAS_ITEM 0x08A8C600u        /* (save, item): nonzero if it has been obtained */
#define FIRST_MODULE_ITEM 122u
#define LIST_COUNT 4u
#define LIST_ROW 16u
#define LIST_ROWS 20u
#define ROW_SIZE 24u
#define ROW_VALUE 12u
#define MOST_ROWS 64

enum { B_SELECT = 0x0001 };
enum { SORT_GAME, SORT_NAME, SORT_OBTAINED, SORT_RECENT, SORT_COUNT };
static const char *const SORT_SHOWN[SORT_COUNT] = {"Modules in the game's order", "Modules by name", "Modules obtained first",
                                                   "Modules last played first"};
static long long g_played[MODULES];   /* when a song was last played in each (song_sort.c keeps it) */

static int g_sort, g_have_game_order;
static int g_game_order[MODULES];
static int g_obtained[MODULES];

/* What the order is worked out from, once when it changes: each module's name as shown, whether
 * it has been obtained, and its place in the game's order. */
static char g_name[MODULES][128];
static int g_place[MODULES];

static void read_keys(void) {
    for (int module = 0; module < MODULES; module++) {
        char original[128];
        const uint32_t at = psp_read32(MODULE_NAMES + (uint32_t)module * 4);
        size_t n = 0;
        while (at && n < sizeof original - 1 && psp_read8(at + (uint32_t)n)) {
            original[n] = (char)psp_read8(at + (uint32_t)n);
            n++;
        }
        original[n] = 0;
        const char *shown = translation_shown(original);
        snprintf(g_name[module], sizeof g_name[module], "%s", shown ? shown : original);
        g_obtained[module] = guest_call(HAS_ITEM, SAVE, FIRST_MODULE_ITEM + (uint32_t)module, 0, 0) != 0;
        g_played[module] = song_sort_module_played(module);
        g_place[module] = MODULES;
    }
    for (int i = 0; i < MODULES; i++)
        if (g_game_order[i] >= 0 && g_game_order[i] < MODULES) g_place[g_game_order[i]] = i;
}

/* Out of the game's order, those not obtained yet go after the others, in the game's order. */
static int before(int a, int b) {
    if (g_sort != SORT_GAME && g_obtained[a] != g_obtained[b]) return g_obtained[a] > g_obtained[b];
    if (g_sort == SORT_NAME && g_obtained[a]) {
        const char *x = g_name[a], *y = g_name[b];
        for (; *x && *y; x++, y++) {
            const int cx = tolower((unsigned char)*x), cy = tolower((unsigned char)*y);
            if (cx != cy) return cx < cy;
        }
        if (*x != *y) return !*x;
    }
    /* Most recently played in first; those never played in after, in the game's order. */
    if (g_sort == SORT_RECENT && g_obtained[a] && g_played[a] != g_played[b]) return g_played[a] > g_played[b];
    return g_place[a] < g_place[b];
}

/* The three orders, worked out together as the screen opens (the game's functions can be called
 * there), so that Select can change between them on any frame with memory alone. */
static int g_orders[SORT_COUNT][MODULES];

static void work_out_orders(void) {
    read_keys();
    const int chosen = g_sort;
    for (int s = 0; s < SORT_COUNT; s++) {
        g_sort = s;
        int *wanted = g_orders[s];
        for (int i = 0; i < MODULES; i++) wanted[i] = g_game_order[i];
        for (int i = 1; i < MODULES; i++) {
            const int module = wanted[i];
            int j = i - 1;
            while (j >= 0 && before(module, wanted[j])) {
                wanted[j + 1] = wanted[j];
                j--;
            }
            wanted[j + 1] = module;
        }
    }
    g_sort = chosen;
}

/* The list's rows put in the order wanted, by their modules; rows that are no module (number -1)
 * stay where they are. False if the list is not, or no longer, the modules'. */
static int sort_rows(uint32_t list, const int *wanted) {
    if (!guest_in_ram(list)) return 0;
    const int count = (int)psp_read32(list + LIST_COUNT);
    const uint32_t rows = psp_read32(list + LIST_ROWS);
    if (count < 2 || count > MOST_ROWS || !guest_in_ram(rows)) return 0;
    int modules[MOST_ROWS], places[MOST_ROWS], n = 0;
    uint8_t seen[MODULES] = {0};
    for (int i = 0; i < count; i++) {
        const int module = (int)psp_read32(rows + (uint32_t)i * ROW_SIZE + ROW_VALUE);
        if (module == -1) continue;
        if (module < 0 || module >= MODULES || seen[module]) return 0;
        seen[module] = 1;
        const uint32_t words = psp_read32(rows + (uint32_t)i * ROW_SIZE);
        if (!guest_in_ram(words)) return 0;
        places[n] = i;
        modules[n++] = module;
    }
    if (n < 8) return 0;
    int order[MOST_ROWS], k = 0;
    for (int i = 0; i < MODULES; i++)
        for (int j = 0; j < n; j++)
            if (modules[j] == wanted[i]) order[k++] = modules[j];
    if (k != n || !memcmp(order, modules, sizeof modules[0] * (size_t)n)) return 1;
    uint8_t copy[MOST_ROWS][ROW_SIZE];
    for (int i = 0; i < n; i++)
        for (uint32_t b = 0; b < ROW_SIZE; b++) copy[i][b] = psp_read8(rows + (uint32_t)places[i] * ROW_SIZE + b);
    const uint32_t cursor = psp_read32(list + LIST_ROW);
    int on = -1;
    for (int i = 0; i < n; i++)
        if ((uint32_t)places[i] == cursor) on = modules[i];
    for (int i = 0; i < n; i++) {
        int from = 0;
        while (modules[from] != order[i]) from++;
        for (uint32_t b = 0; b < ROW_SIZE; b++) psp_write8(rows + (uint32_t)places[i] * ROW_SIZE + b, copy[from][b]);
        if (order[i] == on) psp_write32(list + LIST_ROW, (uint32_t)places[i]);
    }
    return 1;
}

/* The modules' list while their screen is open; 0 when it is not. */
static uint32_t g_list;
static int g_orders_ready;

/* The window manager's lists found as the game finds them, with memory alone (08AFC84C, 08AFC55C,
 * 08AB4508, 08ABFC78), to look on any frame: two windows of 360 bytes, each in use if its first
 * byte is set, with an id at 4 and at 284 a table of its lists. */
#define WINDOW_SIZE 360u
#define WINDOW_ID 4u
#define WINDOW_LISTS 284u
#define WINDOW_SCENE_LIST 292u
#define MANAGER_FRONT 720u


static uint32_t window_list(uint32_t id, uint32_t k) {
    int slot = 1;
    for (; slot >= 0; slot--) {
        const uint32_t window = GAME_WINDOW_MANAGER + (uint32_t)slot * WINDOW_SIZE;
        if (psp_read8(window) && psp_read32(window + WINDOW_ID) == id) break;
    }
    if (slot < 0 || k > 16) return 0;
    const uint32_t lists = psp_read32(GAME_WINDOW_MANAGER + (uint32_t)slot * WINDOW_SIZE + WINDOW_LISTS);
    return guest_in_ram(lists) ? psp_read32(lists + k * 4) : 0;
}

static uint32_t scene_list(void) {
    const uint32_t front = psp_read32(GAME_WINDOW_MANAGER + MANAGER_FRONT);
    if (front > 1) return 0;
    const uint32_t window = GAME_WINDOW_MANAGER + front * WINDOW_SIZE;
    return window_list(psp_read32(window + WINDOW_ID), psp_read32(window + WINDOW_SCENE_LIST));
}

/* Whether a list is the modules': each row a module, none twice, its words that module's name. */
static int is_module_list(uint32_t list) {
    if (!guest_in_ram(list)) return 0;
    const int count = (int)psp_read32(list + LIST_COUNT);
    const uint32_t rows = psp_read32(list + LIST_ROWS);
    if (count < 8 || count > MOST_ROWS || !guest_in_ram(rows)) return 0;
    uint8_t seen[MODULES] = {0};
    int n = 0;
    for (int i = 0; i < count; i++) {
        const int module = (int)psp_read32(rows + (uint32_t)i * ROW_SIZE + ROW_VALUE);
        if (module == -1) continue;
        if (module < 0 || module >= MODULES || seen[module]) return 0;
        seen[module] = 1;
        if (psp_read32(rows + (uint32_t)i * ROW_SIZE) != psp_read32(MODULE_NAMES + (uint32_t)module * 4)) return 0;
        n++;
    }
    return n >= 8;
}

static uint32_t find_module_list(void) {
    const uint32_t list = scene_list();
    if (is_module_list(list)) return list;
    const uint32_t front = window_list(0, 0);
    return is_module_list(front) ? front : 0;
}

/* The rows, then the table that turns a place into a module, in the order chosen. */
static int apply(void) {
    if (!sort_rows(g_list, g_orders[g_sort])) {
        g_list = 0;
        return 0;
    }
    for (int i = 0; i < MODULES; i++)
        if ((int)psp_read32(MODULE_ORDER + (uint32_t)i * 4) != g_orders[g_sort][i]) psp_write32(MODULE_ORDER + (uint32_t)i * 4, (uint32_t)g_orders[g_sort][i]);
    return 1;
}

void module_sort_input(uint32_t buttons) {
    /* The list looked for on each read: when it appears, the orders are worked out (this runs on
     * the game's thread, as it reads the buttons, so its functions can be called), for what was
     * obtained or played since, and it is put in the order chosen. */
    const uint32_t list = find_module_list();
    if (list != g_list) {
        g_list = list;
        if (list) {
            if (!g_have_game_order) {
                for (int i = 0; i < MODULES; i++) g_game_order[i] = (int)psp_read32(MODULE_ORDER + (uint32_t)i * 4);
                g_have_game_order = 1;
            }
            work_out_orders();
            g_orders_ready = 1;
            apply();
        }
    }
    if (!g_orders_ready) return;
    /* Its hint in the footer (extra 0 in pieces.txt). */
    if (g_list) psp_render_gl_footer_extras(FOOTER_SORT);
    static uint32_t held;
    const int pressed = (buttons & B_SELECT) && !(held & B_SELECT);
    held = buttons;
    if (!pressed || !g_list) return;
    const int was = g_sort;
    g_sort = (g_sort + 1) % SORT_COUNT;
    if (!apply()) {
        g_sort = was;
        return;
    }
    psp_render_gl_show_message(SORT_SHOWN[g_sort]);
    fprintf(stderr, "module sort: %s\n", SORT_SHOWN[g_sort]);
}

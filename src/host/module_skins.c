/* Module skins: looks from the mods folder, added to the module screen's list after the game's own.
 *
 *     mods\modules\<name>\body.igb    the body, as the game's skinNN.igb
 *     mods\modules\<name>\head.igb    the head, as skinNN_head.igb (optional: the base's own if left out)
 *     mods\modules\<name>\skin.ini    optional: "base = N", the module it is worn over (0 if left out)
 *
 * A skin is worn over a module of the game's, its base: the game believes that module worn, with
 * its motions and everything else, and only its two models are taken from the skin. They are added
 * to DivaDataList.afs as entries past its end (mods.c), and the game's one loader of models (LOADER)
 * is given their numbers in place of the base's: 142 and 308 past the module, body and head.
 *
 * The list: Module Convert's list has a row for each of the game's 53 modules, its number the
 * module's, in a list with room for 64. As module_sort.c finds it on the screen, the skins' rows are
 * added after, each with its base's number, and the row the cursor is on is followed. The screen
 * changes Miku only into a module other than the one worn (CHANGE_ASKED): a skin over it, or the
 * module itself under a skin, is let through as a change. The screen then puts on the module
 * (ROOM_WEARS, whose loading comes to LOADER), and the skin of the cursor's row is loaded in its
 * place. The skin is kept as worn if its base is worn as the screen closes, and in OPTIONS.INI by
 * its name. Miku's Room makes its own list of the modules (AFTER_ROWS, MODULE_AT, DRESS_MIKU).
 *
 * Addresses are for ULJM05472 v1.01, which Tools\build.ps1 checks.
 */
#include "module_skins.h"

#include "guest.h"
#include "language.h"
#include "mods.h"
#include "options.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define MODULE_AT 0x088628A8u       /* (place): the module at that place of the module screen */
#define MODULE_ORDER 0x08B2727Cu
#define MODULES 53
#define AFTER_ROWS 0x08A8C1A8u      /* in the module screen's opening, after its 53 rows are made */
#define LIST_HOLDER 0x08848C84u     /* () -> what LIST_OF takes */
#define LIST_OF 0x08AFC55Cu         /* (holder, 0, 0): the module screen's list */
#define ADD_ROW 0x08AF2840u         /* (list, words, value): a row, if the list has room */
#define DRESS_MIKU 0x08A94C70u      /* (miku, module) */
#define DRESSED_AS 0x10D18u         /* where a Miku keeps the module she is dressed in */
#define LOADER 0x0885BBD0u          /* (loader, body entry, motion entry, head entry, ...) */
#define BODY_ENTRY 142u
#define HEAD_ENTRY 308u
#define CHANGE_ASKED 0x08AC1F0Cu    /* in the module screen's (screen, module): its check against the one worn */
#define SAVE 0x08C2C4A0u
#define WORN 4u

/* Where MODULE_AT is asked from on the module screen as a row is confirmed, and DRESS_MIKU after. */
#define CHOSEN_AT_1 0x08A8C2E0u
#define CHOSEN_AT_2 0x08A8C320u
#define CHOSEN_AT_3 0x08A8C40Cu
#define DRESSED_CHOSEN_1 0x08A8C2ECu
#define DRESSED_CHOSEN_2 0x08A8C424u
#define DRESSED_CANCEL 0x08A8C368u
#define LOADED_FOR_MENU 0x08A94DA4u

#define MAX_SKINS 11                /* the list's room past the game's 53 rows */
#define LIST_COUNT 4u
#define LIST_ROW 16u
#define LIST_ROWS 20u
#define ROW_SIZE 24u

typedef struct {
    char name[48];
    int base;
    int body, head;                 /* their places among the archive's added entries, or -1 */
    uint32_t words;                 /* the name, in the game's memory, for its row */
} skin;

static skin g_skins[MAX_SKINS];
static int g_count;
static int g_chosen = -1;           /* on the module screen: the skin of the row confirmed last */
static int g_worn = -1;             /* the skin worn, while its base is */
static int g_dressed = -1;          /* the skin the menu's Miku was last dressed in */
static int g_on_screen;
static int g_cursor = -1;           /* on the module screen: the skin of the row the cursor is on */

static int exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

int module_skins_scan(const char *mods_dir) {
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s\\modules\\*", mods_dir);
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.' || found.cFileName[0] == '_') continue;
        if (g_count == MAX_SKINS) {
            fprintf(stderr, "module skins: only %d fit in the list, \"%s\" left out\n", MAX_SKINS, found.cFileName);
            continue;
        }
        char dir[1100], body[1200], head[1200], ini[1200];
        snprintf(dir, sizeof dir, "%s\\modules\\%s", mods_dir, found.cFileName);
        snprintf(body, sizeof body, "%s\\body.igb", dir);
        snprintf(head, sizeof head, "%s\\head.igb", dir);
        snprintf(ini, sizeof ini, "%s\\skin.ini", dir);
        if (!exists(body)) continue;
        skin *s = &g_skins[g_count];
        snprintf(s->name, sizeof s->name, "%s", found.cFileName);
        s->base = 0;
        FILE *f = fopen(ini, "r");
        if (f) {
            char line[256];
            int n;
            while (fgets(line, sizeof line, f))
                if (sscanf(line, " base = %d", &n) == 1 && n >= 0 && n < MODULES) s->base = n;
            fclose(f);
        }
        char entry[40];
        snprintf(entry, sizeof entry, "skinmod%02d.igb", g_count);
        s->body = mods_add_entry(entry, body);
        snprintf(entry, sizeof entry, "skinmod%02d_head.igb", g_count);
        s->head = exists(head) ? mods_add_entry(entry, head) : -1;
        if (s->body < 0) continue;
        fprintf(stderr, "module skins: \"%s\" over module %d%s\n", s->name, s->base, s->head >= 0 ? "" : ", its head the base's");
        g_count++;
    } while (FindNextFileA(h, &found));
    FindClose(h);
    /* The skin worn last time, by name. */
    for (int i = 0; i < g_count; i++)
        if (g_options.module_skin[0] && _stricmp(g_options.module_skin, g_skins[i].name) == 0) g_worn = i;
    return g_count;
}

static int place_of(int module) {
    for (int p = 0; p < MODULES; p++)
        if ((int)psp_read32(MODULE_ORDER + (uint32_t)p * 4) == module) return p;
    return 0;
}

static int worn_over(int module) {
    if (g_worn < 0) return -1;
    return g_skins[g_worn].base == module && (int)psp_read32(SAVE + WORN) == module ? g_worn : -1;
}

static void keep_worn(int s) {
    if (s == g_worn) return;
    g_worn = s;
    snprintf(g_options.module_skin, sizeof g_options.module_skin, "%s", s >= 0 ? g_skins[s].name : "");
    options_save(language_data_dir());
    fprintf(stderr, "module skins: wearing %s\n", s >= 0 ? g_skins[s].name : "the module itself");
}

/* Its row's words, in the game's memory once it runs: ASCII, which the game's letters have. */
static uint32_t words_of(skin *s) {
    if (s->words) return s->words;
    const size_t len = strlen(s->name);
    s->words = psp_sysmem_alloc((uint32_t)len + 1, 1);
    for (size_t i = 0; s->words && i <= len; i++) {
        const unsigned char c = (unsigned char)s->name[i];
        psp_write8(s->words + (uint32_t)i, c && (c < 32 || c > 126) ? '?' : c);
    }
    return s->words;
}

int module_skins_row(uint32_t words) {
    for (int i = 0; i < g_count; i++)
        if (words && g_skins[i].words == words) return i;
    return -1;
}

/* The skins' rows, after the game's, each with its base's number as the game's rows have theirs. */
static void add_rows(uint32_t list) {
    if (!g_count || !guest_in_ram(list)) return;
    const uint32_t count = psp_read32(list + LIST_COUNT), rows = psp_read32(list + LIST_ROWS);
    if (count > 64 || !guest_in_ram(rows)) return;
    for (uint32_t i = 0; i < count; i++)
        if (module_skins_row(psp_read32(rows + i * ROW_SIZE)) >= 0) return;   /* added already */
    for (int i = 0; i < g_count; i++) {
        const uint32_t words = words_of(&g_skins[i]);
        if (!words || !guest_call(ADD_ROW, list, words, (uint32_t)g_skins[i].base, 0)) {
            fprintf(stderr, "module skins: no room in the list (%u rows) for \"%s\"\n", psp_read32(list + LIST_COUNT), g_skins[i].name);
            break;
        }
    }
    fprintf(stderr, "module skins: rows added, the list has %u\n", psp_read32(list + LIST_COUNT));
}

void module_skins_screen(uint32_t list) {
    const int open = list != 0;
    if (open && !g_on_screen) {
        add_rows(list);
        g_chosen = worn_over((int)psp_read32(SAVE + WORN));
    }
    if (open) {
        /* The skin of the row the cursor is on, if it is one. */
        const uint32_t cursor = psp_read32(list + LIST_ROW), rows = psp_read32(list + LIST_ROWS);
        g_cursor = cursor < psp_read32(list + LIST_COUNT) && guest_in_ram(rows) ? module_skins_row(psp_read32(rows + cursor * ROW_SIZE)) : -1;
    }
    if (open == g_on_screen) return;
    g_on_screen = open;
    if (open) return;
    /* Closed: what was confirmed is worn, if its base is. */
    const int worn = (int)psp_read32(SAVE + WORN);
    keep_worn(g_chosen >= 0 && g_skins[g_chosen].base == worn ? g_chosen : -1);
}

static void on_module_at(void) {
    const uint32_t place = psp_cpu.r[PSP_REG_A0], from = psp_cpu.r[PSP_REG_RA];
    int s = -1;
    if (place >= MODULES && place < (uint32_t)(MODULES + g_count)) {
        s = (int)place - MODULES;
        psp_cpu.r[PSP_REG_A0] = (uint32_t)place_of(g_skins[s].base);
    }
    if (from == CHOSEN_AT_1 || from == CHOSEN_AT_2 || from == CHOSEN_AT_3) g_chosen = s;
}

/* On the module screen, Miku is dressed in the row confirmed: a skin's if the cursor is on one.
 * Elsewhere, in the skin worn if she is dressed in its base. */
static void on_dress(void) {
    const uint32_t miku = psp_cpu.r[PSP_REG_A0], from = psp_cpu.r[PSP_REG_RA];
    const int module = (int)psp_cpu.r[PSP_REG_A1];
    int s;
    if (g_on_screen) s = g_chosen = g_cursor;
    else if (from == DRESSED_CHOSEN_1 || from == DRESSED_CHOSEN_2) s = g_chosen;
    else if (from == DRESSED_CANCEL) s = g_chosen = worn_over(module);
    else s = worn_over(module);
    if (s >= 0 && g_skins[s].base != module) s = -1;
    if (g_on_screen) g_chosen = s;
    /* She is not dressed again in the module she wears; a change of skin alone must be. */
    if (s != g_dressed && guest_in_ram(miku + DRESSED_AS)) psp_write32(miku + DRESSED_AS, 0xFFFFFFFFu);
    if (s >= 0) fprintf(stderr, "module skins: Miku dressed in module %d%s%s (from %08X)\n", module, s >= 0 ? ", skin " : "", s >= 0 ? g_skins[s].name : "", from);
    g_dressed = s;
}

static void on_load(void) {
    const uint32_t from = psp_cpu.r[PSP_REG_RA];
    const int module = (int)psp_cpu.r[PSP_REG_A1] - (int)BODY_ENTRY;
    int s;
    if (g_on_screen) {
        /* The module screen puts on the row confirmed (ROOM_WEARS): a skin's if the cursor is on one. */
        s = g_cursor >= 0 && g_skins[g_cursor].base == module ? g_cursor : -1;
        g_chosen = s;
        g_dressed = s;
    } else {
        s = from == LOADED_FOR_MENU ? g_dressed : worn_over(module);
    }
    if (s < 0 || g_skins[s].base != module) return;
    const int body = mods_added_entry(g_skins[s].body), head = mods_added_entry(g_skins[s].head);
    if (body >= 0) psp_cpu.r[PSP_REG_A1] = (uint32_t)body;
    if (head >= 0) psp_cpu.r[PSP_REG_A3] = (uint32_t)head;
    static int told = -1;
    if (told != s) {
        told = s;
        fprintf(stderr, "module skins: models of \"%s\", entries %d and %d\n", g_skins[s].name, body, head);
    }
}

/* The module screen changes Miku only into a module other than the one worn (CHANGE_ASKED, the
 * module in s0 and the one worn just read in v0). A skin over the module worn, or the module itself
 * when a skin over it is worn, is a change too. */
static void on_change_asked(void) {
    const int module = (int)psp_cpu.r[PSP_REG_S0], worn = (int)psp_cpu.r[PSP_REG_V0];
    if (!g_on_screen || module != worn) return;
    const int wearing = worn_over(worn);
    const int wanted = g_cursor >= 0 && g_skins[g_cursor].base == module ? g_cursor : -1;
    if (wanted == wearing) return;
    psp_cpu.r[PSP_REG_V0] = 0xFFFFFFFEu;
    fprintf(stderr, "module skins: changing into %s\n", wanted >= 0 ? g_skins[wanted].name : "the module itself");
}

int module_skins_hook(uint32_t addr) {
    switch (addr) {
    case CHANGE_ASKED: on_change_asked(); return 1;
    case MODULE_AT: on_module_at(); return 1;
    case AFTER_ROWS: add_rows(guest_call(LIST_OF, guest_call(LIST_HOLDER, 0, 0, 0, 0), 0, 0, 0)); return 1;
    case DRESS_MIKU: on_dress(); return 1;
    case LOADER: on_load(); return 1;
    default: return 0;
    }
}

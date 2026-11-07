/* A bot that goes through the game by itself, to see what is left in Japanese.
 *
 *   set DIVA_BOT=1    the songs, then the editor
 *   set DIVA_BOT=2    the editor only
 *   set DIVA_BOT=3    each module in the first song, then the wardrobe's list
 *   set DIVA_BOT=4    the wardrobe's list only
 *
 * First it plays every song of Free Play to its end on the hardest level, pressing nothing
 * while each plays. Life stays full: what changes life is told to start from the top each
 * time, and no change is big enough to empty it.
 *
 * Then it maps the editor. A screen is known by the texts drawn on it. On each screen it tries
 * each button once, and remembers where each went. When a screen has nothing left to try it
 * goes, by what it has learned, to the nearest one that has. When none has, the game ends.
 * Should it end up in the main menu it goes straight back, as nothing out there is mapped.
 * Screens that differ only in pictures look the same to it, which is what the presses that
 * move and then confirm are for.
 *
 * What it does and each different text drawn goes to bot_log.txt, beside the rest of this
 * project's local files. Lyrics are left out, being known by their place in the executable.
 * The save it plays with can end up changed. It costs nothing unless set.
 */

#include "bot.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TEXT_DRAW   0x0883D598u
#define LIFE_CHANGE 0x0886B304u
/* Life is a field of the song's object, past the first 64 KB. */
#define LIFE_FIELD  (0x10000u - 17432u)
#define LIFE_FULL   255u
#define LEVELS_TO_TRY 3
/* Which module is worn is a field of the save, as an index into the game's list of them. */
#define SAVE_OBJECT 0x08C2C4A0u
#define MODULE_WORN (SAVE_OBJECT + 4u)
#define MODULES 53
#define MODULE_SONG_MS 30000u
#define WARDROBE_STEP_MS 1500
#define LYRICS_FIRST 0x08B27878u
#define LYRICS_END   0x08B2FD84u
#define LYRIC_HASHES 4096
#define MAX_SEEN 64
#define MAX_TEXT 200
#define EVER_SIZE 16384

#define B_UP       0x0010u
#define B_RIGHT    0x0020u
#define B_DOWN     0x0040u
#define B_LEFT     0x0080u
#define B_L        0x0100u
#define B_R        0x0200u
#define B_START    0x0008u
#define B_TRIANGLE 0x1000u
#define B_CIRCLE   0x2000u
#define B_CROSS    0x4000u
#define B_SQUARE   0x8000u

#define PRESS_MS 80
#define BETWEEN_MS 220
#define RECENT_MS 700
#define START_WAIT_MS 2000
#define SONG_START_WAIT_MS 4000
#define LONGEST_SONG_MS (12u * 60u * 1000u)
#define MOST_SONGS 80
#define SCREEN_SETTLES_MS 2500
#define TRIES 4
#define TRY_WAIT_MS 2000
#define EXE_FIRST 0x08804000u
#define EXE_END   0x08B80000u
#define MAX_ON_SCREEN 256
#define MAX_SCREENS 8192
#define LOOK_MS 400
#define AFTER_PRESS_MS 300
#define NO_TEXT_WAIT_MS 20000
#define STEADY_LOOKS 8
#define SONG_QUIET_MS 6000
#define SONG_RECHECK_MS 20000
#define EDITOR_TRIES 20
#define MAIN_MENU_LINES 10
#define LONGEST_MAP_MS (12u * 60u * 60u * 1000u)
/* A list can go on while looking the same, as its numbers do not count. */
#define MOST_REPEATS 400

/* Words of the game that say where it is, read from where the executable keeps them. */
#define TEXT_FREE_PLAY guest_text(0x08B70558u)
#define TEXT_MENU_FREE_PLAY guest_text(0x08B700F8u)
#define TEXT_BACK_TO_EDIT guest_text(0x08B27680u)
#define TEXT_MAIN_MENU guest_text(0x08B70568u)
#define TEXT_EDITOR_LINE guest_text(0x08B701B4u)
#define TEXT_GO_TO_EDITOR guest_text(0x08B7039Cu)
#define TEXT_MODE_SELECT guest_text(0x08B56B24u)
#define TEXT_WARDROBE_LINE guest_text(0x08B70154u)
#define TEXT_PLAY_THIS guest_text(0x08B56B8Cu)

enum { REG_A0 = 4, REG_A1 = 5 };
enum { SEEN_FREE_PLAY, SEEN_MENU_FREE_PLAY, SEEN_PLAY_THIS, SEEN_RETRY, SEEN_RETURN, SEEN_BPM, SEEN_LEVELS, SEEN_PAUSED_EDIT,
       SEEN_MAIN_MENU, SEEN_EDITOR_LINE, SEEN_GO_TO_EDITOR, SEEN_MODE_SELECT, SEEN_WARDROBE_LINE, SEEN_COUNT };

static int g_on;
static FILE *g_log;
static CRITICAL_SECTION g_lock;
static uint32_t g_seen[MAX_SEEN];
static int g_seen_count;
static ULONGLONG g_started;
static volatile ULONGLONG g_last_life_change;
static volatile ULONGLONG g_when_seen[SEEN_COUNT];
static volatile LONG g_new_texts;
static uint32_t g_ever[EVER_SIZE];
static uint32_t g_lyrics[LYRIC_HASHES];
static int g_lyrics_known;
/* While a song is being asked about, its name is the text that is not one of these. */
static volatile int g_looking_for_name;
static volatile uint32_t g_name;
static char g_name_text[MAX_TEXT + 1];
/* The texts of the screen, while it is being looked at. */
static int g_looking;
static uint32_t g_on_screen[MAX_ON_SCREEN];
static int g_on_screen_count;
static uint32_t g_screen_sum;

static uint32_t hash_of(const char *text, size_t *length) {
    uint32_t h = 2166136261u;
    size_t n = 0;
    for (; text[n] && n < MAX_TEXT; n++) h = (h ^ (uint8_t)text[n]) * 16777619u;
    *length = n;
    return h ? h : 1;
}

static int in_set(uint32_t *set, uint32_t size, uint32_t h, int add) {
    for (uint32_t i = h % size, tries = 0; tries < size; i = (i + 1) % size, tries++) {
        if (set[i] == h) return 1;
        if (!set[i]) {
            if (add) set[i] = h;
            return 0;
        }
    }
    return 1;
}

static const char *guest_text(uint32_t addr) {
    const uint32_t at = addr & PSP_ADDR_MASK;
    if (at < PSP_RAM_BASE || at >= PSP_RAM_BASE + PSP_RAM_SIZE - MAX_TEXT) return NULL;
    return (const char *)psp_mem.ram + (at - PSP_RAM_BASE);
}

/* The executable is in memory by the time anything is drawn. */
static void learn_lyrics(void) {
    g_lyrics_known = 1;
    for (uint32_t at = LYRICS_FIRST; at < LYRICS_END;) {
        const char *text = guest_text(at);
        size_t length;
        const uint32_t h = hash_of(text, &length);
        if (length) in_set(g_lyrics, LYRIC_HASHES, h, 1);
        at += (uint32_t)length + 1;
    }
}

static double seconds(void) {
    return (double)(GetTickCount64() - g_started) / 1000.0;
}

static void log_text(const char *text) {
    wchar_t wide[MAX_TEXT + 1];
    char utf8[MAX_TEXT * 3 + 1];
    const int n = MultiByteToWideChar(932, 0, text, -1, wide, MAX_TEXT + 1);
    if (!n || !WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof utf8, NULL, NULL)) return;
    fprintf(g_log, "%8.2f text %s\n", seconds(), utf8);
    fflush(g_log);
}

static void note(const char *what, const char *detail) {
    EnterCriticalSection(&g_lock);
    fprintf(g_log, "%8.2f %s %s\n", seconds(), what, detail ? detail : "");
    fflush(g_log);
    LeaveCriticalSection(&g_lock);
}

/* Whether the question about the song names the hardest level. */
static volatile int g_hard_asked;

static int starts_with(const char *text, const char *start) {
    return !strncmp(text, start, strlen(start));
}

static int is_about_the_song(const char *text) {
    return starts_with(text, "BPM ") || starts_with(text, "Highscore") || !strcmp(text, "100%") ||
           !strcmp(text, TEXT_PLAY_THIS) || (uint8_t)text[0] == 0x81 || text[0] == ' ' ||
           !strcmp(text, "EASY") || !strcmp(text, "NORMAL") || !strcmp(text, "HARD");
}

/* What a text says for telling screens apart. Texts made up while running hold counters and
 * times, which do not make it another screen, so their digits are left out. */
static uint32_t identity_of(uint32_t addr, const char *text, uint32_t h) {
    const uint32_t at = addr & PSP_ADDR_MASK;
    if (at >= EXE_FIRST && at < EXE_END) return h;
    uint32_t without = 2166136261u;
    for (size_t n = 0; text[n] && n < MAX_TEXT; n++) {
        if (text[n] >= '0' && text[n] <= '9') continue;
        without = (without ^ (uint8_t)text[n]) * 16777619u;
    }
    return without ? without : 1;
}

static void note_on_screen(uint32_t identity) {
    if (!g_looking || g_on_screen_count == MAX_ON_SCREEN) return;
    for (int i = 0; i < g_on_screen_count; i++) {
        if (g_on_screen[i] == identity) return;
    }
    g_on_screen[g_on_screen_count++] = identity;
    /* Added up, as the order they are drawn in says nothing. */
    g_screen_sum += identity * 2654435761u;
}

static void note_where(const char *text, uint32_t h) {
    const ULONGLONG now = GetTickCount64();
    if (!strcmp(text, TEXT_FREE_PLAY)) g_when_seen[SEEN_FREE_PLAY] = now;
    else if (!strcmp(text, TEXT_MENU_FREE_PLAY)) g_when_seen[SEEN_MENU_FREE_PLAY] = now;
    else if (!strcmp(text, TEXT_PLAY_THIS)) g_when_seen[SEEN_PLAY_THIS] = now;
    else if (!strcmp(text, TEXT_BACK_TO_EDIT)) g_when_seen[SEEN_PAUSED_EDIT] = now;
    else if (!strcmp(text, TEXT_MAIN_MENU)) g_when_seen[SEEN_MAIN_MENU] = now;
    else if (!strcmp(text, TEXT_EDITOR_LINE)) g_when_seen[SEEN_EDITOR_LINE] = now;
    else if (!strcmp(text, TEXT_GO_TO_EDITOR)) g_when_seen[SEEN_GO_TO_EDITOR] = now;
    else if (!strcmp(text, TEXT_MODE_SELECT)) g_when_seen[SEEN_MODE_SELECT] = now;
    else if (!strcmp(text, TEXT_WARDROBE_LINE)) g_when_seen[SEEN_WARDROBE_LINE] = now;
    else if (!strcmp(text, "RETRY")) g_when_seen[SEEN_RETRY] = now;
    else if (!strcmp(text, "RETURN")) g_when_seen[SEEN_RETURN] = now;
    else if (starts_with(text, "BPM ") && text[4] >= '0' && text[4] <= '9') g_when_seen[SEEN_BPM] = now;
    else if (starts_with(text, "Highscore")) g_when_seen[SEEN_LEVELS] = now;
    if (g_looking_for_name && !strcmp(text, "HARD")) g_hard_asked = 1;
    if (g_looking_for_name && !is_about_the_song(text)) {
        g_name = h;
        strncpy(g_name_text, text, MAX_TEXT);
    }
}

static void on_text(uint32_t addr) {
    const char *text = guest_text(addr);
    if (!text) return;
    if (!g_lyrics_known) learn_lyrics();
    size_t length;
    const uint32_t h = hash_of(text, &length);
    if (!length || length >= MAX_TEXT || in_set(g_lyrics, LYRIC_HASHES, h, 0)) return;
    note_where(text, h);
    EnterCriticalSection(&g_lock);
    if (!in_set(g_ever, EVER_SIZE, h, 1)) InterlockedIncrement(&g_new_texts);
    note_on_screen(identity_of(addr, text, h));
    int known = g_seen_count >= MAX_SEEN;
    for (int i = 0; i < g_seen_count && !known; i++) known = g_seen[i] == h;
    if (!known) {
        g_seen[g_seen_count++] = h;
        log_text(text);
    }
    LeaveCriticalSection(&g_lock);
}

static int seen(int what) {
    return GetTickCount64() - g_when_seen[what] < RECENT_MS;
}

static void press(uint32_t button, const char *name) {
    EnterCriticalSection(&g_lock);
    fprintf(g_log, "%8.2f press %s\n", seconds(), name);
    fflush(g_log);
    g_seen_count = 0;
    LeaveCriticalSection(&g_lock);
    psp_ctrl_force(button);
    Sleep(PRESS_MS);
    psp_ctrl_force(0);
    Sleep(BETWEEN_MS);
}

/* Waits for a thing to be on screen. Zero if it never was. */
static int wait_for(int what, unsigned ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (!seen(what)) {
        if (GetTickCount64() > until) return 0;
        Sleep(50);
    }
    return 1;
}

static int in_song_list(void) {
    return seen(SEEN_FREE_PLAY) && seen(SEEN_BPM);
}

/* Screens take no notice of buttons while they come in, so each press is checked by what
 * turns up, and made again if nothing does. Nonzero if it came. */
static int press_until(int what, uint32_t button, const char *name) {
    for (int i = 0; i < TRIES; i++) {
        if (seen(what)) return 1;
        press(button, name);
        if (wait_for(what, TRY_WAIT_MS)) return 1;
    }
    return 0;
}

/* From the title or the menu to the list of songs, ready for buttons. */
static void go_to_song_list(void) {
    while (!in_song_list()) {
        press(B_CIRCLE, "circle");
        const ULONGLONG until = GetTickCount64() + START_WAIT_MS;
        while (GetTickCount64() < until && !in_song_list()) Sleep(50);
    }
    note("state", "song list");
    Sleep(SCREEN_SETTLES_MS);
}

/* Out of whatever was opened over the list. */
static void back_to_song_list(void) {
    for (int i = 0; i < TRIES && (seen(SEEN_LEVELS) || seen(SEEN_PLAY_THIS)); i++) {
        press(B_CROSS, "cross");
        Sleep(TRY_WAIT_MS);
    }
    Sleep(SCREEN_SETTLES_MS);
}

/* Up to the question whether to play the song chosen in the list. Gives back its name,
 * which is how the list is known to have come around again, or zero if it did not get there. */
static uint32_t ask_for_chosen_song(void) {
    if (!press_until(SEEN_LEVELS, B_CIRCLE, "circle")) {
        note("state", "no levels");
        return 0;
    }
    Sleep(SCREEN_SETTLES_MS);
    /* The question says which level it is, so that one press too few can be made up for. */
    uint32_t name = 0;
    for (int i = 0; i < LEVELS_TO_TRY; i++) {
        press(B_RIGHT, "right");
        Sleep(800);
        if (!press_until(SEEN_PLAY_THIS, B_CIRCLE, "circle")) {
            note("state", "no question about the song");
            return 0;
        }
        /* The list goes, and the song's name is left. */
        Sleep(1500);
        g_name = 0;
        g_hard_asked = 0;
        g_looking_for_name = 1;
        Sleep(500);
        g_looking_for_name = 0;
        name = g_name;
        if (g_hard_asked) break;
        if (i + 1 == LEVELS_TO_TRY) {
            note("state", "not on hard");
            break;
        }
        press(B_CROSS, "cross");
        Sleep(SCREEN_SETTLES_MS);
    }
    if (name) {
        EnterCriticalSection(&g_lock);
        log_text(g_name_text);
        LeaveCriticalSection(&g_lock);
    }
    Sleep(1000);
    return name;
}

/* Gives back when it began. */
static ULONGLONG start_song(void) {
    const ULONGLONG started = GetTickCount64();
    const ULONGLONG life_before = g_last_life_change;
    press(B_CIRCLE, "circle");
    /* A screen or two can stand between the question and the song. Circle does no harm once
     * it has begun, but there is nothing to gain from it. */
    while (g_last_life_change == life_before && !seen(SEEN_RETRY) && GetTickCount64() - started < 60000) {
        Sleep(SONG_START_WAIT_MS);
        if (g_last_life_change == life_before && !seen(SEEN_RETRY)) press(B_CIRCLE, "circle");
    }
    note("state", "song playing");
    return started;
}

static void see_song_through(void) {
    const ULONGLONG started = start_song();
    while (!(seen(SEEN_RETRY) && seen(SEEN_RETURN))) {
        if (GetTickCount64() - started > LONGEST_SONG_MS) {
            note("state", "song never ended");
            return;
        }
        Sleep(100);
    }
    note("state", "song ended");
    /* The window starts on RETRY, with RETURN to its right. Should a press go unnoticed, the
     * song plays again, and there is another go at the end of it. */
    for (int i = 0; i < TRIES; i++) {
        Sleep(SCREEN_SETTLES_MS);
        press(B_RIGHT, "right");
        Sleep(800);
        press(B_CIRCLE, "circle");
        const ULONGLONG until = GetTickCount64() + 30000;
        while (!in_song_list() && GetTickCount64() < until) Sleep(100);
        if (in_song_list()) break;
        note("state", "song again");
        const ULONGLONG longest = GetTickCount64() + LONGEST_SONG_MS;
        while (!(seen(SEEN_RETRY) && seen(SEEN_RETURN)) && GetTickCount64() < longest) Sleep(100);
    }
    Sleep(SCREEN_SETTLES_MS);
}

static void play_all_songs(void) {
    go_to_song_list();
    uint32_t first = 0, last = 0;
    int played = 0;
    for (int i = 0; i < MOST_SONGS; i++) {
        if (!in_song_list()) go_to_song_list();
        if (i) {
            press(B_DOWN, "down");
            Sleep(800);
        }
        const uint32_t name = ask_for_chosen_song();
        if (!name) {
            back_to_song_list();
            continue;
        }
        /* In this order, as a press of down that was missed looks like the list coming around. */
        if (name == last) {
            note("state", "still the same song");
            back_to_song_list();
            continue;
        }
        if (name == first) {
            note("state", "the list came around again");
            back_to_song_list();
            return;
        }
        if (!first) first = name;
        last = name;
        char count[32];
        snprintf(count, sizeof count, "song %d", ++played);
        note("state", count);
        see_song_through();
    }
}

static int wait_for_song_list(unsigned ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    while (!in_song_list()) {
        if (GetTickCount64() > until) return 0;
        Sleep(100);
    }
    return 1;
}

/* The pause menu reads RESUME, RETRY, RETURN from the top, and may ask whether that is meant.
 * What it took is in the log, as this has not been seen working yet. Should it fail, the song
 * ends by itself and its results have RETURN to the right. */
static void leave_song(void) {
    for (int i = 0; i < TRIES && !in_song_list(); i++) {
        if (!(seen(SEEN_RETRY) && seen(SEEN_RETURN))) {
            press(B_START, "start");
            Sleep(RECENT_MS + 100);
        }
        if (!(seen(SEEN_RETRY) && seen(SEEN_RETURN))) continue;
        press(B_DOWN, "down");
        press(B_DOWN, "down");
        press(B_CIRCLE, "circle");
        if (wait_for_song_list(8000)) break;
        note("state", "still in the song, answering");
        press(B_LEFT, "left");
        Sleep(800);
        press(B_CIRCLE, "circle");
        if (wait_for_song_list(8000)) break;
        note("state", "could not leave");
    }
    const ULONGLONG until = GetTickCount64() + LONGEST_SONG_MS;
    while (!in_song_list() && GetTickCount64() < until) {
        if (seen(SEEN_RETRY) && seen(SEEN_RETURN)) {
            Sleep(SCREEN_SETTLES_MS);
            press(B_RIGHT, "right");
            Sleep(800);
            press(B_CIRCLE, "circle");
        }
        Sleep(1000);
    }
    Sleep(SCREEN_SETTLES_MS);
}

/* The song's screens hold the model, and the list's the portraits. The list comes last, as the
 * game ends from there with no way back to find. */
static void walk_the_wardrobe(void) {
    note("state", "going to the wardrobe");
    /* From the title circle gets to the menu; from the list of songs, cross does. */
    for (int i = 0; i < EDITOR_TRIES && !seen(SEEN_MAIN_MENU); i++) {
        press(in_song_list() ? B_CROSS : B_CIRCLE, "to the menu");
        Sleep(TRY_WAIT_MS);
    }
    Sleep(SCREEN_SETTLES_MS);
    /* The menu's description says which line the cursor is on. */
    for (int i = 0; i < MAIN_MENU_LINES && !seen(SEEN_WARDROBE_LINE); i++) {
        press(i < MAIN_MENU_LINES / 2 ? B_DOWN : B_RIGHT, "move");
        Sleep(800);
    }
    if (!seen(SEEN_WARDROBE_LINE)) {
        note("state", "no wardrobe line");
        return;
    }
    press(B_CIRCLE, "circle");
    Sleep(SCREEN_SETTLES_MS * 2);
    for (int i = 0; i < MODULES + 5; i++) {
        press(B_DOWN, "down");
        Sleep(WARDROBE_STEP_MS);
    }
}

static void try_every_module(void) {
    go_to_song_list();
    const uint32_t worn_before = psp_read32(MODULE_WORN);
    char detail[64];
    snprintf(detail, sizeof detail, "worn at the start: %u", worn_before);
    note("state", detail);
    for (uint32_t module = 0; module < MODULES; module++) {
        if (!in_song_list()) go_to_song_list();
        psp_write32(MODULE_WORN, module);
        if (!ask_for_chosen_song()) {
            back_to_song_list();
            note("state", "no song to wear it in");
            continue;
        }
        psp_write32(MODULE_WORN, module);
        snprintf(detail, sizeof detail, "module %u", module);
        note("state", detail);
        start_song();
        Sleep(MODULE_SONG_MS);
        leave_song();
    }
    psp_write32(MODULE_WORN, worn_before);
    walk_the_wardrobe();
}

/* What is on screen now, as one number. Zero if no text is. */
static uint32_t look(void) {
    EnterCriticalSection(&g_lock);
    g_on_screen_count = 0;
    g_screen_sum = 0;
    g_looking = 1;
    LeaveCriticalSection(&g_lock);
    Sleep(LOOK_MS);
    EnterCriticalSection(&g_lock);
    g_looking = 0;
    const uint32_t sum = g_on_screen_count ? (g_screen_sum ? g_screen_sum : 1) : 0;
    LeaveCriticalSection(&g_lock);
    return sum;
}

/* Waits out loading and fades. */
static uint32_t look_when_steady(void) {
    const ULONGLONG until = GetTickCount64() + NO_TEXT_WAIT_MS;
    uint32_t last = look();
    while (!last && GetTickCount64() < until) last = look();
    for (int i = 0; i < STEADY_LOOKS; i++) {
        const uint32_t now = look();
        if (now == last) break;
        last = now;
    }
    return last;
}

typedef struct {
    uint32_t first, then;
    const char *name;
} action;

/* In the order they are tried. Back comes last, so a screen is done with before it is left,
 * and down late, so that the first line of a list tells about all the others. */
enum {
    A_CIRCLE, A_SQUARE, A_TRIANGLE, A_START, A_L, A_R, A_LEFT, A_RIGHT, A_UP, A_DOWN,
    A_RIGHT_CIRCLE, A_LEFT_CIRCLE, A_DOWN_CIRCLE, A_UP_CIRCLE, A_CROSS, ACTIONS
};
static const action ACTION[ACTIONS] = {
    [A_CIRCLE] = {B_CIRCLE, 0, "circle"},
    [A_SQUARE] = {B_SQUARE, 0, "square"},
    [A_TRIANGLE] = {B_TRIANGLE, 0, "triangle"},
    [A_START] = {B_START, 0, "start"},
    [A_L] = {B_L, 0, "l"},
    [A_R] = {B_R, 0, "r"},
    [A_LEFT] = {B_LEFT, 0, "left"},
    [A_RIGHT] = {B_RIGHT, 0, "right"},
    [A_UP] = {B_UP, 0, "up"},
    [A_DOWN] = {B_DOWN, 0, "down"},
    [A_RIGHT_CIRCLE] = {B_RIGHT, B_CIRCLE, "right, circle"},
    [A_LEFT_CIRCLE] = {B_LEFT, B_CIRCLE, "left, circle"},
    [A_DOWN_CIRCLE] = {B_DOWN, B_CIRCLE, "down, circle"},
    [A_UP_CIRCLE] = {B_UP, B_CIRCLE, "up, circle"},
    [A_CROSS] = {B_CROSS, 0, "cross"},
};
#define ALL_ACTIONS ((1u << ACTIONS) - 1u)
#define BIT(a) (1u << (a))
/* Buttons that do the same on every line of a list when they do nothing on one of them. */
#define SAME_ALONG_A_LIST (BIT(A_SQUARE) | BIT(A_TRIANGLE) | BIT(A_START) | BIT(A_L) | BIT(A_R) | BIT(A_LEFT) | BIT(A_RIGHT))
/* A line of a list is left through its first line, and moving then confirming is for screens
 * whose cursor is a picture, not for lists whose cursor can be seen. */
#define NOT_FOR_LIST_LINES (BIT(A_CROSS) | BIT(A_RIGHT_CIRCLE) | BIT(A_LEFT_CIRCLE) | BIT(A_DOWN_CIRCLE) | BIT(A_UP_CIRCLE))

typedef struct {
    uint32_t id;
    uint32_t tried;
    uint16_t repeats;
    int16_t went[ACTIONS];
} screen;

static screen g_screens[MAX_SCREENS];
static int g_screen_count;

static int screen_of(uint32_t id) {
    for (int i = 0; i < g_screen_count; i++) {
        if (g_screens[i].id == id) return i;
    }
    if (g_screen_count == MAX_SCREENS) return -1;
    screen *made = &g_screens[g_screen_count];
    made->id = id;
    memset(made->went, 0xFF, sizeof made->went);
    char detail[64];
    snprintf(detail, sizeof detail, "%d %08X", g_screen_count + 1, id);
    note("screen", detail);
    return g_screen_count++;
}

/* What a screen moved to by a direction can take from the one it came from. */
static void learn_from_line_above(int from, int to) {
    screen *a = &g_screens[from];
    screen *b = &g_screens[to];
    b->tried |= NOT_FOR_LIST_LINES;
    for (int i = 0; i < ACTIONS; i++) {
        if (!(SAME_ALONG_A_LIST & BIT(i)) || !(a->tried & BIT(i)) || a->went[i] != from) continue;
        b->tried |= BIT(i);
        b->went[i] = (int16_t)to;
    }
}

static int next_to_try(const screen *s) {
    for (int i = 0; i < ACTIONS; i++) {
        if (!(s->tried & BIT(i))) return i;
    }
    return -1;
}

/* The first press on the shortest known way to a screen with something left to try, or -1. */
static int way_to_something_left(int from) {
    static int16_t queue[MAX_SCREENS];
    static int8_t first[MAX_SCREENS];
    static uint8_t visited[MAX_SCREENS];
    memset(visited, 0, sizeof visited);
    int head = 0, tail = 0;
    queue[tail++] = (int16_t)from;
    visited[from] = 1;
    first[from] = -1;
    while (head < tail) {
        const int at = queue[head++];
        if (at != from && next_to_try(&g_screens[at]) >= 0) return first[at];
        for (int i = 0; i < ACTIONS; i++) {
            const int to = g_screens[at].went[i];
            if (to < 0 || visited[to]) continue;
            visited[to] = 1;
            first[to] = at == from ? (int8_t)i : first[at];
            queue[tail++] = (int16_t)to;
        }
    }
    return -1;
}

static void do_action(int a) {
    press(ACTION[a].first, ACTION[a].name);
    if (ACTION[a].then) press(ACTION[a].then, ACTION[a].name);
    Sleep(AFTER_PRESS_MS);
}

/* A test play of an edit takes no notice of cross. It ends from its pause menu, whose third
 * line goes back to the editor. Other songs have no such line and are left to end. */
static void deal_with_song(void) {
    press(B_START, "start");
    Sleep(RECENT_MS + 100);
    if (!seen(SEEN_PAUSED_EDIT)) {
        press(B_START, "start");
        Sleep(SONG_RECHECK_MS);
        return;
    }
    note("state", "leaving test play");
    press(B_DOWN, "down");
    press(B_DOWN, "down");
    press(B_CIRCLE, "circle");
    Sleep(SCREEN_SETTLES_MS);
}

/* From the title, the main menu or the list of songs. The question comes up over the menu,
 * and there a press sideways could change the answer, so it is looked for first. */
static void go_to_editor(void) {
    note("state", "going to the editor");
    for (int i = 0; i < EDITOR_TRIES && !seen(SEEN_MODE_SELECT); i++) {
        if (seen(SEEN_GO_TO_EDITOR)) {
            press(B_CIRCLE, "circle");
        } else if (in_song_list()) {
            press(B_CROSS, "cross");
        } else if (seen(SEEN_MAIN_MENU)) {
            Sleep(SCREEN_SETTLES_MS);
            for (int j = 0; j < MAIN_MENU_LINES && !seen(SEEN_EDITOR_LINE); j++) {
                press(B_RIGHT, "right");
                Sleep(800);
            }
            press(B_CIRCLE, "circle");
        } else {
            press(B_CIRCLE, "circle");
        }
        Sleep(TRY_WAIT_MS);
    }
    Sleep(SCREEN_SETTLES_MS);
}

static void map_the_editor(void) {
    note("state", "mapping");
    const ULONGLONG until = GetTickCount64() + LONGEST_MAP_MS;
    go_to_editor();
    int at = screen_of(look_when_steady());
    int stuck = 0;
    while (at >= 0 && GetTickCount64() < until) {
        if (GetTickCount64() - g_last_life_change < SONG_QUIET_MS) {
            deal_with_song();
            at = screen_of(look_when_steady());
            continue;
        }
        if (seen(SEEN_RETRY) && seen(SEEN_RETURN)) {
            press(B_RIGHT, "right");
            Sleep(800);
            press(B_CIRCLE, "circle");
            Sleep(SCREEN_SETTLES_MS);
            at = screen_of(look_when_steady());
            continue;
        }
        screen *here = &g_screens[at];
        if (seen(SEEN_MAIN_MENU) && !seen(SEEN_GO_TO_EDITOR)) {
            /* Marked as done, so no way goes through it. */
            here->tried = ALL_ACTIONS;
            go_to_editor();
            at = screen_of(look_when_steady());
            continue;
        }
        int a = next_to_try(here);
        const int fresh = a >= 0;
        if (!fresh) a = way_to_something_left(at);
        if (a < 0) {
            /* Nothing known leads on, but a song or a dialog may be holding things up. */
            if (++stuck > 3) break;
            press(B_CROSS, "cross");
            Sleep(SCREEN_SETTLES_MS);
            at = screen_of(look_when_steady());
            continue;
        }
        stuck = 0;
        const LONG texts_before = g_new_texts;
        do_action(a);
        const int moved_along = a == A_UP || a == A_DOWN || a == A_LEFT || a == A_RIGHT;
        uint32_t now = look();
        if (now != here->id) {
            /* A cursor moving along a list brings no new screen to wait for. */
            if (!moved_along) Sleep(SCREEN_SETTLES_MS);
            now = look_when_steady();
        }
        const int count_before = g_screen_count;
        const int to = screen_of(now);
        if (to < 0) break;
        here->went[a] = (int16_t)to;
        const int list_goes_on = to == at && moved_along && g_new_texts != texts_before && here->repeats < MOST_REPEATS;
        if (list_goes_on) here->repeats++;
        else if (fresh) here->tried |= BIT(a);
        /* Moving showed on screen, so the cursor can be seen and needs no blind confirming. */
        if (to != at && moved_along && a == A_RIGHT) here->tried |= BIT(A_RIGHT_CIRCLE);
        if (to != at && moved_along && a == A_LEFT) here->tried |= BIT(A_LEFT_CIRCLE);
        if (to != at && moved_along && a == A_DOWN) here->tried |= BIT(A_DOWN_CIRCLE);
        if (to != at && moved_along && a == A_UP) here->tried |= BIT(A_UP_CIRCLE);
        if (moved_along && to != at && g_screen_count != count_before) learn_from_line_above(at, to);
        at = to;
    }
    char detail[64];
    snprintf(detail, sizeof detail, "%d screens, %s", g_screen_count, at < 0 ? "no room for more" : "nothing left to try");
    note("state", detail);
}

static unsigned __stdcall run(void *arg) {
    (void)arg;
    const char mode = getenv("DIVA_BOT")[0];
    if (mode == '4') {
        walk_the_wardrobe();
    } else if (mode == '3') {
        try_every_module();
    } else {
        if (mode != '2') play_all_songs();
        map_the_editor();
    }
    note("state", "finished");
    fprintf(stderr, "bot: finished\n");
    fflush(stderr);
    _Exit(0);
}

int bot_is_on(void) {
    const char *mode = getenv("DIVA_BOT");
    return mode && (mode[0] == '1' || mode[0] == '2' || mode[0] == '3' || mode[0] == '4');
}

void bot_start(void) {
    const char *local = getenv("LOCALAPPDATA");
    if (!bot_is_on() || !local) return;
    char path[MAX_PATH];
    snprintf(path, sizeof path, "%s/ProjectDivaRecomp/bot_log.txt", local);
    g_log = fopen(path, "w");
    if (!g_log) {
        fprintf(stderr, "bot: cannot write %s\n", path);
        return;
    }
    InitializeCriticalSection(&g_lock);
    g_started = GetTickCount64();
    g_on = 1;
    _beginthreadex(NULL, 0, run, NULL, 0, NULL);
    fprintf(stderr, "bot: running, log in %s\n", path);
}

void bot_hook(uint32_t addr) {
    /* With DIVA_NO_DEATH set a song cannot be lost, to look at the picture and not play. */
    static int no_death = -1;
    if (no_death < 0) no_death = getenv("DIVA_NO_DEATH") != NULL;
    if (no_death && addr == LIFE_CHANGE) psp_write32(psp_cpu.r[REG_A0] + LIFE_FIELD, LIFE_FULL);
    if (!g_on) return;
    if (addr == LIFE_CHANGE) {
        psp_write32(psp_cpu.r[REG_A0] + LIFE_FIELD, LIFE_FULL);
        g_last_life_change = GetTickCount64();
    } else if (addr == TEXT_DRAW) {
        on_text(psp_cpu.r[REG_A1]);
    }
}

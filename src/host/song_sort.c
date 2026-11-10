/* The Free Play song list in another order, chosen with Select: as the game has it, by name, by
 * best score, by BPM, or last played first.
 *
 * The list is one of the game's window lists (see controls.c for the pause menu's): a count at 4,
 * the row the cursor is on at 16, and at 20 the rows, 24 bytes each, a song's number 12 bytes into
 * a row. The song list scene's update (SONG_LIST_UPDATE) asks the window manager for it each
 * frame; here it is put in order just before, its rows moved and the cursor kept on the same song.
 * Nothing else is changed: everything the game shows of a row it reads from the song's number.
 *
 * L and R change the module worn in the songs, between those obtained, Miku behind the menu
 * changing into it; on Rin's and Len's songs, theirs. The option "Rin & Len songs" can have those
 * sung in the module worn too.
 *
 * Addresses are for ULJM05472 v1.01, which Tools\build.ps1 checks.
 */
#include "song_sort.h"
#include "timing.h"
#include "guest.h"

#include "language.h"
#include "options.h"
#include "practice.h"
#include "translation.h"

#include <psprecomp/cpu.h>
#include <psprecomp/dispatch.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>
#include <psprecomp/window.h>

#include <ctype.h>
#include <direct.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define SONG_LIST_UPDATE 0x08AC0514u
#define SONG_LIST_OF 0x08ABFC78u     /* (manager): the song list */
#define LIST_COUNT 4u
#define LIST_ROW 16u
#define LIST_ROWS 20u
#define ROW_SIZE 24u
#define ROW_SONG 12u
#define MOST_ROWS 64

/* The songs' table: a row of 16 bytes each, the name's address first. */
#define SONG_TABLE 0x08B5BE78u
#define SONG_ENTRY 16u
/* The save's records, as presence.c reads them: from the scene, its game, the scores object. */
#define SCENE_GAME 4u
#define GAME_SCORES 29328u
#define RECORDS_START 48u
#define SONG_RECORDS 1680u
#define LEVEL_RECORD 560u
#define HIGH_SCORE_AT 552u
#define LEVELS 3

/* The modules: their order in their own list, their names, and which is worn. */
#define MODULE_ORDER 0x08B2727Cu
#define MODULE_NAMES 0x08B25B68u
#define MODULES 53
#define SAVE_WORN 4u
#define HAS_ITEM 0x08A8C600u        /* (save, item): nonzero if it has been obtained */
#define FIRST_MODULE_ITEM 122u
#define ROOM_WEARS 0x08AACA24u        /* (room, module): worn, kept in the save, Miku dressed in it */

enum { B_SELECT = 0x0001, B_START = 0x0008, B_L = 0x0100, B_R = 0x0200 };
enum { SORT_GAME, SORT_NAME, SORT_SCORE, SORT_BPM, SORT_RECENT, SORT_COUNT };
/* Shown as the order changes, in English, Japanese and German (UI_ENGLISH, UI_JAPANESE, UI_GERMAN). */
static const char *const SORT_NAME_SHOWN[SORT_COUNT][UI_COUNT] = {
    {"Songs in the game's order", "\xE6\x9B\xB2\xEF\xBC\x9A\xE6\xA8\x99\xE6\xBA\x96\xE3\x81\xAE\xE9\xA0\x86", "Lieder in der Reihenfolge des Spiels"},
    {"Songs by name", "\xE6\x9B\xB2\xEF\xBC\x9A\xE5\x90\x8D\xE5\x89\x8D\xE9\xA0\x86", "Lieder nach Namen"},
    {"Songs by best score", "\xE6\x9B\xB2\xEF\xBC\x9A\xE3\x83\x8F\xE3\x82\xA4\xE3\x82\xB9\xE3\x82\xB3\xE3\x82\xA2\xE9\xA0\x86", "Lieder nach Bestwert"},
    {"Songs by BPM", "\xE6\x9B\xB2\xEF\xBC\x9A" "BPM\xE9\xA0\x86", "Lieder nach BPM"},
    {"Songs last played first", "\xE6\x9B\xB2\xEF\xBC\x9A\xE6\x9C\x80\xE8\xBF\x91\xE3\x83\x97\xE3\x83\xAC\xE3\x82\xA4\xE3\x81\x97\xE3\x81\x9F\xE9\xA0\x86", "Zuletzt gespielte Lieder zuerst"},
};

/* ---- when each song was last played, kept here as the game keeps no such thing: in PLAYED.INI
 * beside OPTIONS.INI, a line "song N = seconds since 1970" for each song played. */
static long long g_played[MOST_ROWS];
static long long g_module_played[MODULES];   /* "module N = ...": the module a song was played in */
static int g_played_read;

static void played_path(char *out, size_t cap, int make) {
    const char *data = language_data_dir();
    static const char *const PARTS[] = {"\\ms", "\\ms\\PSP", "\\ms\\PSP\\SAVEDATA", "\\ms\\PSP\\SAVEDATA\\ULJM05472RECOMP"};
    for (int i = 0; make && i < 4; i++) {
        snprintf(out, cap, "%s%s", data, PARTS[i]);
        _mkdir(out);
    }
    snprintf(out, cap, "%s%s\\PLAYED.INI", data, PARTS[3]);
}

static void read_played(void) {
    g_played_read = 1;
    char path[1200], line[96];
    played_path(path, sizeof path, 0);
    FILE *f = fopen(path, "r");
    if (!f) return;
    int song;
    long long when;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "song %d = %lld", &song, &when) == 2 && song >= 0 && song < MOST_ROWS) g_played[song] = when;
        else if (sscanf(line, "module %d = %lld", &song, &when) == 2 && song >= 0 && song < MODULES) g_module_played[song] = when;
    }
    fclose(f);
}

/* A song begun, in a module: both kept as played now. */
static void song_played(int song, int module) {
    if (song < 0 || song >= MOST_ROWS) return;
    if (!g_played_read) read_played();
    const long long now = (long long)time(NULL);
    g_played[song] = now;
    if (module >= 0 && module < MODULES) g_module_played[module] = now;
    char path[1200];
    played_path(path, sizeof path, 1);
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < MOST_ROWS; i++)
        if (g_played[i]) fprintf(f, "song %d = %lld\n", i, g_played[i]);
    for (int i = 0; i < MODULES; i++)
        if (g_module_played[i]) fprintf(f, "module %d = %lld\n", i, g_module_played[i]);
    fclose(f);
}

long long song_sort_module_played(int module) {
    if (!g_played_read) read_played();
    return module >= 0 && module < MODULES ? g_module_played[module] : 0;
}

/* ---- each song's tempo, as the song list shows it: in a table of 64 bytes a song, from
 * SONG_INFO. Its scripts mark some songs' bars at twice that, so it is read from here. Rin's and
 * Len's versions of two songs are those songs to the game (08862FE0: 32 and 34 the 16th, 33 and 35
 * the 17th), and the list shows those songs' tempos for them. */
#define SONG_INFO 0x08B5A6E0u
#define SONG_INFO_SIZE 64u
#define INFO_BPM 16u

static int g_bpm[MOST_ROWS], g_bpm_read;   /* 0 where it is not known */

static void read_bpms(void) {
    g_bpm_read = 1;
    for (int song = 0; song < MOST_ROWS; song++) {
        const int from = song == 32 || song == 34 ? 16 : song == 33 || song == 35 ? 17 : song;
        const int bpm = (int)psp_read32(SONG_INFO + (uint32_t)from * SONG_INFO_SIZE + INFO_BPM);
        g_bpm[song] = bpm >= 40 && bpm <= 400 ? bpm : 0;
    }
}

/* What a row shows for what is not unlocked yet: "-----". */
#define LOCKED_LABEL 0x08B722A8u

static int g_sort;
static uint32_t g_scores;
static int g_locked[MOST_ROWS];
static uint32_t g_words[MOST_ROWS];   /* each song's row's words: its name as shown */
static int g_cursor_song = -1;   /* the song the list's cursor is on */
static int g_songs_in_front;     /* the window in front was the song list, at the last frame */
static uint32_t g_scene;          /* the song list's scene; its second word is the room */
/* The order the game made the list in, by song, to go back to it. */
static int g_game_order[MOST_ROWS], g_game_count;

static int valid_song(int song) {
    return song >= 0 && song < MOST_ROWS && psp_read32(SONG_TABLE + (uint32_t)song * SONG_ENTRY) >= GUEST_RAM_START;
}

/* What the order is worked out from, read once for each list: each song's name (in romaji if it
 * has one), its best score, and its place in the game's order. */
static char g_name[MOST_ROWS][128];
static uint32_t g_best[MOST_ROWS];
static int g_place[MOST_ROWS];

static void read_keys(const int *songs, int n) {
    for (int i = 0; i < MOST_ROWS; i++) g_place[i] = MOST_ROWS;
    for (int i = 0; i < g_game_count; i++) g_place[g_game_order[i]] = i;
    for (int i = 0; i < n; i++) {
        const int song = songs[i];
        char original[128];
        /* The name as its row shows it, or the table's for one not unlocked (it goes last anyway). */
        const uint32_t at = g_locked[song] ? psp_read32(SONG_TABLE + (uint32_t)song * SONG_ENTRY) : g_words[song];
        size_t k = 0;
        while (k < sizeof original - 1 && psp_read8(at + (uint32_t)k)) {
            original[k] = (char)psp_read8(at + (uint32_t)k);
            k++;
        }
        original[k] = 0;
        const char *romaji = g_sort == SORT_NAME ? translation_song_romaji(original) : NULL;
        snprintf(g_name[song], sizeof g_name[song], "%s", romaji ? romaji : original);
        g_best[song] = 0;
        for (int level = 0; g_scores && level < LEVELS; level++) {
            const uint32_t score = psp_read32(g_scores + RECORDS_START + (uint32_t)song * SONG_RECORDS + (uint32_t)level * LEVEL_RECORD + HIGH_SCORE_AT);
            if (score > g_best[song] && score < 100000000u) g_best[song] = score;
        }
    }
}

/* Whether song a goes before song b in the order chosen. Those not unlocked yet, whose names the
 * list hides, go after the others but in the game's order. */
static int before(int a, int b) {
    if (g_sort != SORT_GAME && g_locked[a] != g_locked[b]) return g_locked[b];
    if (g_sort != SORT_GAME && g_locked[a]) return g_place[a] < g_place[b];
    if (g_sort == SORT_NAME) {
        const char *x = g_name[a], *y = g_name[b];
        for (; *x && *y; x++, y++) {
            const int cx = tolower((unsigned char)*x), cy = tolower((unsigned char)*y);
            if (cx != cy) return cx < cy;
        }
        if (*x != *y) return !*x;
    } else if (g_sort == SORT_SCORE && g_best[a] != g_best[b]) {
        return g_best[a] > g_best[b];
    } else if (g_sort == SORT_RECENT && g_played[a] != g_played[b]) {
        /* Most recent first; those never played after, in the game's order. */
        return g_played[a] > g_played[b];
    } else if (g_sort == SORT_BPM && g_bpm[a] != g_bpm[b]) {
        /* Slowest first; those whose tempo is not known after the others. */
        if (!g_bpm[a] || !g_bpm[b]) return g_bpm[a] != 0;
        return g_bpm[a] < g_bpm[b];
    }
    return g_place[a] < g_place[b];
}

/* Puts the list's songs in order, if it is a list of songs: each row's words the name of its song,
 * or what stands for one not unlocked yet. Another window's list in front, as a question's, is
 * left as it is. Rows that are not a song, as a "none" the game's lists can start with (its number
 * is -1), stay where they are; the songs move among the other places. */
/* Whether the rows are a list of songs: each a song (or -1, a row of its own), none twice, with
 * words, and more of them than a question's list has. Their words come from a table of names of
 * their own (seen in a run), so only that there are some is checked. */
#define FEWEST_SONGS 8

static int is_song_list(uint32_t rows, int count) {
    int songs = 0;
    uint8_t seen[MOST_ROWS] = {0};
    for (int i = 0; i < count; i++) {
        const int song = (int)psp_read32(rows + (uint32_t)i * ROW_SIZE + ROW_SONG);
        if (song == -1) continue;
        if (!valid_song(song) || seen[song]) return 0;
        seen[song] = 1;
        const uint32_t words = psp_read32(rows + (uint32_t)i * ROW_SIZE);
        if (!guest_in_ram(words)) return 0;
        songs++;
    }
    return songs >= FEWEST_SONGS;
}

static void sort_list(uint32_t list) {
    g_songs_in_front = 0;
    if (!guest_in_ram(list)) return;
    const int count = (int)psp_read32(list + LIST_COUNT);
    const uint32_t rows = psp_read32(list + LIST_ROWS);
    if (count < 2 || count > MOST_ROWS || !guest_in_ram(rows) || !is_song_list(rows, count)) return;
    int songs[MOST_ROWS], places[MOST_ROWS], n = 0;
    for (int i = 0; i < count; i++) {
        const int song = (int)psp_read32(rows + (uint32_t)i * ROW_SIZE + ROW_SONG);
        if (song == -1) continue;
        g_words[song] = psp_read32(rows + (uint32_t)i * ROW_SIZE);
        g_locked[song] = g_words[song] == LOCKED_LABEL;
        places[n] = i;
        songs[n++] = song;
    }
    g_songs_in_front = 1;
    /* A list made again, with songs that were not in it: its order is the game's. */
    int same = n == g_game_count;
    for (int i = 0; same && i < n; i++) {
        int found = 0;
        for (int k = 0; k < g_game_count && !found; k++) found = g_game_order[k] == songs[i];
        same = found;
    }
    if (!same) {
        memcpy(g_game_order, songs, sizeof songs[0] * (size_t)n);
        g_game_count = n;
    }
    if (g_sort == SORT_GAME) {
        /* Back to the game's order: nothing to read but where each song was. */
        for (int i = 0; i < MOST_ROWS; i++) g_place[i] = MOST_ROWS;
        for (int i = 0; i < g_game_count; i++) g_place[g_game_order[i]] = i;
    } else {
        if (g_sort == SORT_BPM && !g_bpm_read) read_bpms();
        if (g_sort == SORT_RECENT && !g_played_read) read_played();
        read_keys(songs, n);
    }
    int order[MOST_ROWS];
    for (int i = 0; i < n; i++) order[i] = songs[i];
    for (int i = 1; i < n; i++) {
        const int song = order[i];
        int j = i - 1;
        while (j >= 0 && before(song, order[j])) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = song;
    }
    if (!memcmp(order, songs, sizeof songs[0] * (size_t)n)) return;
    /* The rows moved whole, their marks with them, and the cursor kept on its song. */
    uint8_t copy[MOST_ROWS][ROW_SIZE];
    for (int i = 0; i < n; i++)
        for (uint32_t b = 0; b < ROW_SIZE; b++) copy[i][b] = psp_read8(rows + (uint32_t)places[i] * ROW_SIZE + b);
    const uint32_t cursor = psp_read32(list + LIST_ROW);
    int on = -1;
    for (int i = 0; i < n; i++)
        if ((uint32_t)places[i] == cursor) on = songs[i];
    for (int i = 0; i < n; i++) {
        int from = 0;
        while (songs[from] != order[i]) from++;
        for (uint32_t b = 0; b < ROW_SIZE; b++) psp_write8(rows + (uint32_t)places[i] * ROW_SIZE + b, copy[from][b]);
        if (order[i] == on) psp_write32(list + LIST_ROW, (uint32_t)places[i]);
    }
}

/* Who sings a song. The game makes Kagamine Rin sing songs 32 and 33 and Kagamine Len 34 and 35,
 * in their own modules, whatever is worn (see 088632A0); the others are sung in the module worn,
 * whoever's it is, as the game allows. */
static int song_singer_module(int song) {
    if (g_options.free_singers) return -1;   /* sung in the module worn too (see on_singer_module) */
    if (song == 32 || song == 33) return 40;
    if (song == 34 || song == 35) return 41;
    return -1;
}

/* A module's name as shown, after `before`, as a message: from the game's letters to UTF-8. */
static void show_module(const char *before, int module, const char *after) {
    char name[128];
    const uint32_t from = psp_read32(MODULE_NAMES + (uint32_t)module * 4);
    size_t n = 0;
    while (from && n < sizeof name - 1 && psp_read8(from + (uint32_t)n)) {
        name[n] = (char)psp_read8(from + (uint32_t)n);
        n++;
    }
    name[n] = 0;
    char utf8[384], line[512];
    translation_shown_utf8(name, utf8, sizeof utf8);
    if (!utf8[0]) snprintf(utf8, sizeof utf8, "Module %d", module + 1);
    snprintf(line, sizeof line, "%s%s%s", before, utf8, after);
    psp_render_gl_show_message(line);
}

/* Kagamine Rin's modules, then Len's: their own, then their swimwear. */
static const int SINGER_MODULES[2][2] = {{40, 48}, {41, 49}};

/* ---- Miku behind the menu changing her module, as the modules' screen has her do it (08AC1EE4,
 * 08AC1F64, 08AC1FE0, 08AC20D4, a step on each frame): the room (the scene's second word) plays
 * her changing, she is put in the new module once that is done, shown again once it is loaded,
 * plays her coming back, and stands. Each wait gives up after a while, so she is never left out. */
#define ROOM_EFFECT 28940u        /* in the room: what plays around her as she changes */
#define ROOM_MOTION 29304u        /* in the room: her motions */
#define ROOM_SINGER 96u           /* in the room: her model */
#define EFFECT_START 0x08ABD544u  /* (effect) */
#define EFFECT_END 0x08AAB1C8u    /* (effect, 0) with -1.0 */
#define PLAY_MOTION 0x08AAE670u   /* (room, motion): 7 changing, 8 changed, 9 back, -1 standing */
#define MOTION_DONE 0x08B0D3ACu   /* (motions): nonzero when the one playing is over */
#define SINGER_LOADED 0x08AACAACu /* (room): nonzero once her model is loaded, and shows it */
#define SINGER_STILL 0x08AAE2C8u  /* (room, 0) */
#define SINGER_POSE 0x08AADD34u   /* (room, 0, 1, 0) with 0.0 */
#define SOUNDS 0x08848C3Cu        /* (): the sound effects */
#define PLAY_SOUND 0x08AB179Cu    /* (sounds, sound, 0, 0): 96 as she changes, 97 when she is done */
#define STOP_SOUNDS 0x08AB1920u   /* (sounds) */
#define MOTIONS_RESTART 0x08AAD4F8u  /* (room): its motions' loading begun again */
#define MOTIONS_LOAD 0x08AAD508u     /* (room, group): a step of loading a group; nonzero once done */
#define MOTION_FREE 0x08B0D2F0u      /* (motions, motion) */
#define CHANGING_MOTIONS 7u          /* the group of 7 to 9 */
#define STEP_LOADING 10
#define CHANGE_WAIT_MS 4000

static int g_change_step, g_change_to = -1, g_change_next = -1, g_motions_loaded;
static ULONGLONG g_change_since;

static uint32_t change_room(void) {
    const uint32_t room = g_scene ? psp_read32(g_scene + SCENE_GAME) : 0;
    if (!guest_in_ram(room) || psp_read32(room + GAME_SCORES) != g_scores) return 0;
    return room;
}

static void change_go(int step) {
    g_change_step = step;
    g_change_since = GetTickCount64();
}

/* Starts her changing into a module, or changes what she is changing into. False if there is no
 * room to do it in. */
static int change_into(int module) {
    const uint32_t room = change_room();
    if (!room) return 0;
    if (g_change_step == 0) {
        /* Her changing motions are loaded first, as the modules' screen loads them as it opens
         * (08AC10D8); the song list has none of them. */
        g_change_to = module;
        guest_call_f(MOTIONS_RESTART, room, 0, 0, 0, 0.0f);
        change_go(STEP_LOADING);
    } else if (g_change_step == STEP_LOADING || g_change_step == 1) {
        g_change_to = module;        /* not put on yet */
    } else {
        g_change_next = module;      /* after this change */
        g_change_to = module;
    }
    return 1;
}

/* A step on each of the song list's frames. */
static void change_step(void) {
    if (!g_change_step) return;
    const uint32_t room = change_room();
    if (!room) return;
    const int waited_out = GetTickCount64() - g_change_since > CHANGE_WAIT_MS;
    switch (g_change_step) {
    case STEP_LOADING: /* Once her changing motions are loaded, she starts changing. */
        g_motions_loaded = (guest_call_f(MOTIONS_LOAD, room, CHANGING_MOTIONS, 0, 0, 0.0f) & 0xFF) != 0;
        if (!g_motions_loaded && !waited_out) return;
        guest_call_f(EFFECT_START, room + ROOM_EFFECT, 0, 0, 0, 0.0f);
        guest_call_f(PLAY_SOUND, guest_call_f(SOUNDS, 0, 0, 0, 0, 0.0f), 96, 0, 0, 0.0f);
        guest_call_f(PLAY_MOTION, room, 7, 0, 0, 0.0f);
        change_go(1);
        return;
    case 1: /* Changing: once that is played, into the module. */
        if (!guest_call_f(MOTION_DONE, room + ROOM_MOTION, 0, 0, 0, 0.0f) && !waited_out) return;
        guest_call_f(PLAY_MOTION, room, 8, 0, 0, 0.0f);
        guest_call_f(ROOM_WEARS, room, (uint32_t)g_change_to, 0, 0, 0.0f);
        change_go(2);
        return;
    case 2: { /* Once loaded, shown again, coming back. */
        if (!guest_call_f(SINGER_LOADED, room, 0, 0, 0, 0.0f) && !waited_out) return;
        const uint32_t sounds = guest_call_f(SOUNDS, 0, 0, 0, 0, 0.0f);
        guest_call_f(STOP_SOUNDS, sounds, 0, 0, 0, 0.0f);
        guest_call_f(PLAY_SOUND, sounds, 97, 0, 0, 0.0f);
        guest_call_f(EFFECT_END, room + ROOM_EFFECT, 0, 0, 0, -1.0f);
        const uint32_t singer = psp_read32(room + ROOM_SINGER);
        if (guest_in_ram(singer)) {
            const uint32_t methods = psp_read32(singer + 200);
            const uint32_t show = guest_in_ram(methods) ? psp_read32(methods + 36) : 0;
            if (guest_in_ram(show)) guest_call_f(show, singer, 1, 1, 0, 0.0f);
        }
        guest_call_f(SINGER_STILL, room, 0, 0, 0, 0.0f);
        guest_call_f(SINGER_POSE, room, 0, 1, 0, 0.0f);
        guest_call_f(PLAY_MOTION, room, 9, 0, 0, 0.0f);
        change_go(3);
        return;
    }
    default: /* Back: standing. */
        if (!guest_call_f(MOTION_DONE, room + ROOM_MOTION, 0, 0, 0, 0.0f) && !waited_out) return;
        guest_call_f(PLAY_MOTION, room, (uint32_t)-1, 0, 0, 0.0f);
        g_change_step = 0;
        if (g_change_next >= 0 && g_change_next != (int)psp_read32(g_scores + SAVE_WORN)) {
            /* Again, with the motions still loaded. */
            const int next = g_change_next;
            g_change_next = -1;
            g_change_to = next;
            guest_call_f(EFFECT_START, room + ROOM_EFFECT, 0, 0, 0, 0.0f);
            guest_call_f(PLAY_SOUND, guest_call_f(SOUNDS, 0, 0, 0, 0, 0.0f), 96, 0, 0, 0.0f);
            guest_call_f(PLAY_MOTION, room, 7, 0, 0, 0.0f);
            change_go(1);
        } else {
            g_change_next = g_change_to = -1;
            /* Her changing motions let go, as the modules' screen does as it closes (08AC1460). */
            if (g_motions_loaded)
                for (uint32_t motion = 7; motion <= 9; motion++) guest_call_f(MOTION_FREE, room + ROOM_MOTION, motion, 0, 0, 0.0f);
            g_motions_loaded = 0;
        }
    }
}


/* L and R put on the module before or after the one worn, of those obtained, in the order of the
 * modules' own list (see module_sort.c). The save's scores object keeps the one worn at 4, which
 * the game reads as a song starts to load Miku in it; its name is shown. */
static void next_module(int step) {
    if (!g_scores) return;
    const int singer = song_singer_module(g_cursor_song);
    if (singer >= 0) {
        /* Rin's or Len's song: between their own modules obtained, kept in the options, as the
         * game wears none of them but the first (see on_singer_module). */
        const int who = singer == 40 ? 0 : 1;
        const int now = g_options.singer_module[who] == SINGER_MODULES[who][1];
        const int other = SINGER_MODULES[who][!now];
        if (guest_call(HAS_ITEM, g_scores, FIRST_MODULE_ITEM + (uint32_t)other, 0, 0)) {
            g_options.singer_module[who] = other;
            options_save(language_data_dir());
        }
        show_module("", g_options.singer_module[who], "");
        return;
    }
    /* From the one she is changing into, if she is. */
    const int worn = g_change_to >= 0 ? g_change_to : (int)psp_read32(g_scores + SAVE_WORN);
    int place = 0;
    while (place < MODULES && (int)psp_read32(MODULE_ORDER + (uint32_t)place * 4) != worn) place++;
    if (place == MODULES) place = 0;
    for (int tries = 1; tries < MODULES; tries++) {
        const int at = ((place + step * tries) % MODULES + MODULES) % MODULES;
        const int module = (int)psp_read32(MODULE_ORDER + (uint32_t)at * 4);
        if (module < 0 || module >= MODULES || !guest_call(HAS_ITEM, g_scores, FIRST_MODULE_ITEM + (uint32_t)module, 0, 0)) continue;
        /* Miku behind the menu changes into it as on the modules' screen (see change_step); the
         * save keeps it as she puts it on. */
        if (!change_into(module)) psp_write32(g_scores + SAVE_WORN, (uint32_t)module);
        show_module("", module, "");
        fprintf(stderr, "song list: module %d worn\n", module);
        return;
    }
}

/* The song list's update runs every frame while it is up: a little while without it, it is gone. */
#define LIST_SEEN_MS 200u
static ULONGLONG g_list_seen_ms;

static void on_song_list(void) {
    g_list_seen_ms = GetTickCount64();
    const uint32_t scene = psp_cpu.r[PSP_REG_A0];
    g_scene = scene;
    const uint32_t game = psp_read32(scene + SCENE_GAME);
    g_scores = guest_in_ram(game) ? psp_read32(game + GAME_SCORES) : 0;
    if (g_scores && !guest_in_ram(g_scores)) g_scores = 0;
    change_step();
    /* Select goes to the next order; L and R to another module. */
    static uint32_t held;
    const uint32_t buttons = psp_window_buttons();
    const int pressed = (buttons & B_SELECT) && !(held & B_SELECT);
    const uint32_t shoulders = buttons & (B_L | B_R), fresh = shoulders & ~held;
    const int start = (buttons & B_START) && !(held & B_START);
    held = buttons;
    /* Start switches practice on or off. */
    if (start && g_songs_in_front) practice_toggle();
    if (fresh && shoulders != (B_L | B_R) && g_songs_in_front) next_module(fresh & B_R ? 1 : -1);
    if (pressed) {
        g_sort = (g_sort + 1) % SORT_COUNT;
        psp_render_gl_show_message(SORT_NAME_SHOWN[g_sort][language_ui()]);
        fprintf(stderr, "song sort: %s\n", SORT_NAME_SHOWN[g_sort][UI_ENGLISH]);
    }
    const uint32_t list = guest_call(SONG_LIST_OF, GAME_WINDOW_MANAGER, 0, 0, 0);
    sort_list(list);
    g_cursor_song = -1;
    if (guest_in_ram(list)) {
        const uint32_t cursor = psp_read32(list + LIST_ROW), rows = psp_read32(list + LIST_ROWS);
        if (cursor < psp_read32(list + LIST_COUNT) && cursor < MOST_ROWS && guest_in_ram(rows))
            g_cursor_song = g_songs_in_front ? (int)psp_read32(rows + cursor * ROW_SIZE + ROW_SONG) : -1;
    }
}

/* As a song starts, the game puts Rin or Len in their first module for their songs, and then asks
 * what kind that module is (088632E4), with the song's setup at s0, the module at 32 in it, and
 * the module plus 308 at s3 for what is loaded next. There the one chosen goes in its place. */
#define MODULE_KIND 0x08862198u
#define AFTER_SINGER 0x088632ECu
#define SETUP_MODULE 32u
#define SETUP_SONG 20u
#define MODULE_TO_MODEL 308u

static void put_singer_module(void);

static void on_singer_module(void) {
    if (psp_cpu.r[PSP_REG_RA] != AFTER_SINGER) return;
    put_singer_module();
    /* Every song starts through here: the one chosen is at 20 in its setup, and the module it is
     * played in is the one asked about now; both kept as played. */
    const int song = (int)psp_read32(psp_cpu.r[PSP_REG_S0] + SETUP_SONG);
    song_played(song, (int)psp_cpu.r[PSP_REG_A0]);
    timing_song_started(song);
}

static void put_singer_module(void) {
    const int forced = (int)psp_cpu.r[PSP_REG_A0];
    const int who = forced == 40 ? 0 : forced == 41 ? 1 : -1;
    if (who < 0) return;
    /* With the option on, the module worn, whoever's it is, which the game read just before. */
    const int worn = g_scores ? (int)psp_read32(g_scores + SAVE_WORN) : -1;
    const int chosen = g_options.free_singers && worn >= 0 && worn < MODULES ? worn : g_options.singer_module[who];
    if (chosen == forced || (!g_options.free_singers && chosen != SINGER_MODULES[who][1])) return;
    psp_cpu.r[PSP_REG_A0] = (uint32_t)chosen;
    psp_cpu.r[PSP_REG_S3] = (uint32_t)chosen + MODULE_TO_MODEL;
    psp_write32(psp_cpu.r[PSP_REG_S0] + SETUP_MODULE, (uint32_t)chosen);
    fprintf(stderr, "song: module %d in place of %d\n", chosen, forced);
}

/* Start is practice's while the song list is in front, and the game's does not get it. */
int song_sort_list_in_front(void) { return g_songs_in_front && GetTickCount64() - g_list_seen_ms < LIST_SEEN_MS; }

/* Triangle on a song whose PV is still locked gets only the game's sound for "no" (sound 3 of
 * 08AB179C, the menus' sounds, with the sound in a1). With the song list in front and Triangle
 * held, that sound is told as a message too, as "Game saved" is. */
#define PLAY_MENU_SOUND 0x08AB179Cu
#define SOUND_NO 3u
#define PSP_TRIANGLE 0x1000u
static const char *const PV_LOCKED[UI_COUNT] = {
    [UI_ENGLISH] = "PV not unlocked yet: clear this song with GREAT or PERFECT",
    [UI_JAPANESE] = "\xE3\x81\x93\xE3\x81\xAE\x50\x56\xE3\x81\xAF\xE6\x9C\xAA\xE8\xA7\xA3\xE7\xA6\x81\xEF\xBC\x9A"
                    "\x47\x52\x45\x41\x54\xE4\xBB\xA5\xE4\xB8\x8A\xE3\x81\xA7\xE3\x82\xAF\xE3\x83\xAA\xE3\x82\xA2\xE3\x81\x99"
                    "\xE3\x82\x8B\xE3\x81\xA8\xE8\xA7\xA3\xE7\xA6\x81",
    [UI_GERMAN] = "PV noch gesperrt: Lied mit GREAT oder PERFECT schaffen",
};

static void on_menu_sound(void) {
    if (psp_cpu.r[PSP_REG_A1] != SOUND_NO || !song_sort_list_in_front()) return;
    if (!(psp_window_buttons() & PSP_TRIANGLE)) return;
    psp_render_gl_show_message(PV_LOCKED[language_ui()]);
}

int song_sort_hook(uint32_t addr) {
    if (addr == PLAY_MENU_SOUND) {
        on_menu_sound();
        return 1;
    }
    if (addr == MODULE_KIND) {
        on_singer_module();
        return 1;
    }
    if (addr != SONG_LIST_UPDATE) return 0;
    on_song_list();
    return 1;
}

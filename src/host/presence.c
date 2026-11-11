/* What the player is doing, shown on their Discord profile.
 *
 * Discord's app on the same computer listens on a named pipe, \\.\pipe\discord-ipc-0 to -9. Each
 * message on it is a small header, an opcode and a length, both 32-bit little-endian, then JSON.
 * The first says hello with the application's id; after that, SET_ACTIVITY replaces the status.
 * A thread of its own does all of this, so a missing or slow Discord never holds up the game.
 *
 * What the player is doing is worked out from what the game draws, as nothing else says it:
 *   - which screen: a text only that screen draws ("Menu" on Home, "Room Style" in Miku's Room...);
 *   - a song: its name and level are the lines of the message that asks to play it, and it starts
 *     and stops when the timing code sees it (timing.c);
 *   - high scores: read from the save, for the song chosen in the song list (see presence_chosen).
 */
#include "presence.h"
#include "ui_text.h"

#include <psprecomp/mem.h>

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define PIPES 10
#define RETRY_MS 5000
#define TICK_MS 1000
#define FIELD 128
#define FRAME_MOST 4096
/* A screen's own text has to have been drawn this recently for the screen to be the one shown. */
#define SCREEN_FRESH_MS 600
/* Where the game starts, and the demo goes back to. */
#define TITLE_SCREEN "On the title screen"

enum { OP_HANDSHAKE = 0, OP_FRAME = 1 };

/* ---- what is shown ---------------------------------------------------------- */

static CRITICAL_SECTION g_lock;
static int g_started;
static char g_client_id[32];
static char g_details[FIELD], g_state[FIELD];
static long long g_since;          /* seconds since 1970, for the elapsed time Discord shows */
static volatile LONG g_changed;
static volatile ULONGLONG g_changed_at;
#define STEADY_MS 1500
#define SEND_GAP_MS 4000
/* Discord counts the time from here: when the game started, the same for every screen and song,
 * so it does not start again each time what the player is doing changes. */
static long long g_session_start;

static void show(const char *details, const char *state, long long since) {
    if (!g_started) return;
    EnterCriticalSection(&g_lock);
    if (strcmp(details, g_details) || strcmp(state, g_state) || since != g_since) {
        snprintf(g_details, FIELD, "%s", details);
        snprintf(g_state, FIELD, "%s", state);
        g_since = since;
        g_changed_at = GetTickCount64();
        InterlockedExchange(&g_changed, 1);
    }
    LeaveCriticalSection(&g_lock);
}

static long long now_seconds(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (long long)(t / 10000000ull - 11644473600ull);
}

/* ---- the pipe --------------------------------------------------------------- */

static int write_frame(HANDLE pipe, uint32_t op, const char *json) {
    const uint32_t len = (uint32_t)strlen(json);
    uint8_t head[8];
    memcpy(head, &op, 4);
    memcpy(head + 4, &len, 4);
    DWORD done;
    return WriteFile(pipe, head, 8, &done, NULL) && done == 8 && WriteFile(pipe, json, len, &done, NULL) && done == len ? 0 : -1;
}

/* Reads one message and drops it: Discord answers each, and the pipe must be kept clear. */
static int read_frame(HANDLE pipe) {
    uint8_t head[8];
    DWORD done;
    if (!ReadFile(pipe, head, 8, &done, NULL) || done != 8) return -1;
    uint32_t len;
    memcpy(&len, head + 4, 4);
    char buf[FRAME_MOST];
    while (len > 0) {
        const DWORD want = len < sizeof buf ? len : (DWORD)sizeof buf;
        if (!ReadFile(pipe, buf, want, &done, NULL) || done == 0) return -1;
        len -= done;
    }
    return 0;
}

static HANDLE open_pipe(void) {
    for (int i = 0; i < PIPES; i++) {
        char name[64];
        snprintf(name, sizeof name, "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE pipe = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe != INVALID_HANDLE_VALUE) return pipe;
    }
    return INVALID_HANDLE_VALUE;
}

/* Text for inside a JSON string. The game's own text is Shift-JIS; Discord wants UTF-8. */
static void json_text(char *out, size_t size, const char *game_text) {
    wchar_t wide[FIELD];
    char utf8[FIELD * 3];
    /* The translations' letters the game's font lacks are kept in letters.txt's places. */
    ui_text_from_game(game_text, 1, wide, FIELD);
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, sizeof utf8, NULL, NULL) <= 0) snprintf(utf8, sizeof utf8, "%s", game_text);
    size_t o = 0;
    for (const char *p = utf8; *p && o + 7 < size; p++) {
        const unsigned char ch = (unsigned char)*p;
        if (ch == '"' || ch == '\\') { out[o++] = '\\'; out[o++] = (char)ch; }
        else if (ch < 0x20) o += (size_t)snprintf(out + o, size - o, "\\u%04x", ch);
        else out[o++] = (char)ch;
    }
    out[o] = 0;
}

static int send_activity(HANDLE pipe) {
    char details[FIELD * 4], state[FIELD * 4], json[FRAME_MOST];
    long long since;
    EnterCriticalSection(&g_lock);
    json_text(details, sizeof details, g_details);
    json_text(state, sizeof state, g_state);
    since = g_since;
    LeaveCriticalSection(&g_lock);
    snprintf(json, sizeof json,
             "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%lld\",\"args\":{\"pid\":%lu,\"activity\":{"
             "\"details\":\"%s\"%s%s%s,\"timestamps\":{\"start\":%lld},"
             "\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Project DIVA Recomp\"}}}}",
             (long long)GetTickCount64(), (unsigned long)GetCurrentProcessId(), details,
             state[0] ? ",\"state\":\"" : "", state, state[0] ? "\"" : "", since);
    if (write_frame(pipe, OP_FRAME, json) != 0) return -1;
    return read_frame(pipe);
}

static void song_check(void);

static unsigned __stdcall pipe_thread(void *unused) {
    (void)unused;
    for (;;) {
        HANDLE pipe = open_pipe();
        if (pipe == INVALID_HANDLE_VALUE) {
            Sleep(RETRY_MS);
            continue;
        }
        char hello[128];
        snprintf(hello, sizeof hello, "{\"v\":1,\"client_id\":\"%s\"}", g_client_id);
        if (write_frame(pipe, OP_HANDSHAKE, hello) == 0 && read_frame(pipe) == 0) {
            fprintf(stderr, "presence: connected to Discord\n");
            InterlockedExchange(&g_changed, 1);
            ULONGLONG sent_at = 0;
            for (;;) {
                song_check();
                /* Discord takes about five changes in twenty seconds, and the screen can flicker
                 * between two for a moment: a change goes once it has held, and not too often. */
                const ULONGLONG now = GetTickCount64();
                if (g_changed && now - g_changed_at >= STEADY_MS && now - sent_at >= SEND_GAP_MS) {
                    InterlockedExchange(&g_changed, 0);
                    sent_at = now;
                    if (send_activity(pipe) != 0) break;
                }
                Sleep(TICK_MS / 4);
            }
        }
        fprintf(stderr, "presence: Discord is not answering, trying again later\n");
        CloseHandle(pipe);
        Sleep(RETRY_MS);
    }
    return 0;
}

void presence_start(const char *client_id) {
    if (!client_id || !client_id[0] || g_started) return;
    snprintf(g_client_id, sizeof g_client_id, "%s", client_id);
    InitializeCriticalSection(&g_lock);
    g_started = 1;
    g_since = g_session_start = now_seconds();
    snprintf(g_details, FIELD, TITLE_SCREEN);
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, pipe_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
}

/* ---- what the player is doing ---------------------------------------------- */

typedef struct {
    uint32_t text;        /* the game's own text that only this screen draws */
    const char *shown;    /* what the profile says */
} screen;

static const screen SCREENS[] = {
    {0x08B700ECu, "Home"},              /* "Menu", the label at the top of Home */
    {0x08B70C94u, "Choosing a song"},   /* "High Score", in the song list */
    {0x08B710DCu, "In Miku's Room"},    /* "Change Layout" */
    {0x08B70E94u, "In Miku's Room"},    /* "Room Style" */
    {0x08B56B24u, "Making a rhythm game"},  /* "Mode Select", in the editor */
    {0x08B571FCu, "Making a rhythm game"},  /* "Edit TOP" */
    {0x08B7065Cu, "In the options"},    /* "Change the game's BGM" */
    {0x08B70DC4u, "Changing modules"},  /* "Specific module" */
};
#define SCREEN_COUNT (sizeof SCREENS / sizeof SCREENS[0])

/* The title screen is all pictures, with no text of its own: the game starts on it, the demo it
 * plays goes back to it, and so does leaving Home after the message that asks to. */
#define ASK_RETURN_TO_TITLE 0x08B703E4u   /* "Return to the title." */
#define HOME_GONE_MS 1500
#define ASKED_LATELY_MS 10000
static ULONGLONG g_seen_at[SCREEN_COUNT];
static ULONGLONG g_title_asked_at;
static const char *g_screen = TITLE_SCREEN;
static int g_in_song;
static char g_song[FIELD], g_level[16];
static char g_last_placed[FIELD];
static char g_asked_song[FIELD], g_asked_level[16];

/* The save keeps a record for each song and level: from its scores object, 48 bytes on, 1680
 * bytes a song, 560 a level, and the high score 552 bytes into a record (see the song list's
 * panel, 08AC00B0, and the functions it calls). The panel says which song and level are chosen. */
#define RECORDS_START 48u
#define SONG_RECORDS 1680u
#define LEVEL_RECORD 560u
#define HIGH_SCORE_AT 552u
#define LEVELS 3
static uint32_t g_scores, g_chosen_scores;   /* the save's scores object, as the panel had it */
static int g_chosen_song = -1, g_song_index = -1;

static uint32_t high_score(uint32_t scores, int song, int level) {
    if (!scores || song < 0) return 0;
    return psp_read32(scores + RECORDS_START + (uint32_t)song * SONG_RECORDS + (uint32_t)level * LEVEL_RECORD + HIGH_SCORE_AT);
}

static void with_commas(char *out, size_t size, uint32_t n) {
    if (n >= 1000000) snprintf(out, size, "%u,%03u,%03u", n / 1000000, n / 1000 % 1000, n % 1000);
    else if (n >= 1000) snprintf(out, size, "%u,%03u", n / 1000, n % 1000);
    else snprintf(out, size, "%u", n);
}

static void show_now(void) {
    if (g_in_song) {
        char state[FIELD];
        snprintf(state, sizeof state, "%s", g_level);
        if (g_song_index >= 0) {
            static const char *const SHORT[LEVELS] = {"E", "N", "H"};
            char best[FIELD] = "";
            for (int level = 0; level < LEVELS; level++) {
                char n[24];
                with_commas(n, sizeof n, high_score(g_scores, g_song_index, level));
                snprintf(best + strlen(best), sizeof best - strlen(best), "%s%s %s", level ? " / " : "", SHORT[level], n);
            }
            snprintf(state, sizeof state, "%s%sHigh scores %s", g_level, g_level[0] ? " - " : "", best);
        }
        show(g_song[0] ? g_song : "Playing a song", state, g_session_start);
    } else {
        show(g_screen, "", g_session_start);
    }
}

void presence_text(uint32_t asked, const char *shown, int placed) {
    if (!g_started || !shown) return;
    const ULONGLONG now = GetTickCount64();
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (SCREENS[i].text == asked) g_seen_at[i] = now;
    }
    if (asked == ASK_RETURN_TO_TITLE) g_title_asked_at = now;
    /* The message that asks to play a song: its name, then its level, then the question. */
    if (placed) {
        if (!strcmp(shown, "EASY") || !strcmp(shown, "NORMAL") || !strcmp(shown, "HARD")) {
            snprintf(g_asked_song, FIELD, "%s", g_last_placed);
            snprintf(g_asked_level, sizeof g_asked_level, "%s", shown);
        }
        snprintf(g_last_placed, FIELD, "%s", shown);
    }
    if (g_in_song) return;
    /* The screen whose own text was drawn last, if it was drawn just now. With none, the last one
     * stays: Miku's Room with its menu put away, to look at the room or take a picture, draws no
     * text at all, and is still Miku's Room. */
    const char *best = g_screen;
    ULONGLONG best_at = 0;
    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        if (g_seen_at[i] && now - g_seen_at[i] < SCREEN_FRESH_MS && g_seen_at[i] > best_at) {
            best = SCREENS[i].shown;
            best_at = g_seen_at[i];
        }
    }
    if (best != g_screen) fprintf(stderr, "presence: screen %s\n", best);
    g_screen = best;
    show_now();
}

#define SONG_GONE_MS 1500
static volatile ULONGLONG g_song_heard;

static void song_started(void) {
    if (!g_started || g_in_song) return;
    g_in_song = 1;
    g_song_index = g_asked_song[0] ? g_chosen_song : -1;
    g_scores = g_chosen_scores;
    /* A song with no message asking for it first, as the one the title screen plays by itself. */
    snprintf(g_song, FIELD, "%s", g_asked_song[0] ? g_asked_song : "Watching a demo");
    snprintf(g_level, sizeof g_level, "%s", g_asked_song[0] ? g_asked_level : "");
    g_asked_song[0] = g_asked_level[0] = 0;
    fprintf(stderr, "presence: song started: %s %s\n", g_song, g_level);
    show_now();
}

static void song_stopped(void) {
    if (!g_started || !g_in_song) return;
    g_in_song = 0;
    /* The demo is played by the title screen, and goes back to it. */
    if (g_song_index < 0 && !g_level[0]) g_screen = TITLE_SCREEN;
    show_now();
}

/* Each time the game updates a song: whether one is on, paused or not. When that stops coming,
 * the song is over, whether it was played to its end, left, or the title screen's demo ended. */
void presence_song_frame(int on) {
    if (!g_started) return;
    if (on) {
        g_song_heard = GetTickCount64();
        song_started();
    } else if (g_in_song) {
        g_song_heard = 0;
        song_stopped();
    }
}

static void song_check(void) {
    const ULONGLONG now = GetTickCount64();
    if (g_in_song && now - g_song_heard > SONG_GONE_MS) song_stopped();
    /* Asked to go back to the title, and Home has gone since: the title screen. */
    if (!g_in_song && g_title_asked_at && now - g_title_asked_at < ASKED_LATELY_MS && now - g_seen_at[0] > HOME_GONE_MS) {
        g_title_asked_at = 0;
        g_screen = TITLE_SCREEN;
        show_now();
    }
}

/* The song list's panel, being drawn for the song and level chosen, with the save's scores object. */
void presence_chosen(uint32_t scores, int song, int level) {
    (void)level;
    g_chosen_scores = scores;
    g_chosen_song = song;
}

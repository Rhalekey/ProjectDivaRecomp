/* A tool for finding a number in the game's memory that only ever goes down, such as
 * the life gauge during a song in which nothing is being hit.
 *
 *   set DIVA_LIFE_SEARCH=1
 *
 * F9 starts looking and F10 stops, with a beep for each. What went down several times and
 * never up, without going down all the time as timers do, goes to the log. It runs
 * beside the game and looks at its memory four times a second, so the game does not
 * change. It costs nothing unless set.
 */

#include "lifesearch.h"

#include <psprecomp/mem.h>

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define FIRST 0x08800000u
#define SIZE  0x01800000u
#define WORDS (SIZE / 4)
#define MAX_REPORT 400
#define MIN_DOWNS 3

/* Each word is read as these, so that all counts go in one row per word. */
enum { VIEW_I32, VIEW_F32, VIEW_I16_LOW, VIEW_I16_HIGH, VIEW_B0, VIEW_B1, VIEW_B2, VIEW_B3, VIEW_COUNT };
static const char *const VIEW_NAMES[VIEW_COUNT] = {"i32", "f32", "i16", "i16+2", "u8", "u8+1", "u8+2", "u8+3"};

static uint8_t *g_downs, *g_ups;

static double value_of(uint32_t word, int view) {
    switch (view) {
    case VIEW_I32: return (int32_t)word;
    case VIEW_F32: {
        float f;
        memcpy(&f, &word, 4);
        return f == f ? f : 0.0;
    }
    case VIEW_I16_LOW: return (int16_t)word;
    case VIEW_I16_HIGH: return (int16_t)(word >> 16);
    default: return (word >> ((view - VIEW_B0) * 8)) & 0xFF;
    }
}

static void compare(const uint32_t *before, const uint32_t *now) {
    for (uint32_t i = 0; i < WORDS; i++) {
        if (before[i] == now[i]) continue;
        for (int view = 0; view < VIEW_COUNT; view++) {
            const double a = value_of(before[i], view), b = value_of(now[i], view);
            const size_t at = (size_t)i * VIEW_COUNT + view;
            if (b < a && g_downs[at] < 255) g_downs[at]++;
            else if (b > a && g_ups[at] < 255) g_ups[at]++;
        }
    }
}

static void report(const uint32_t *start, const uint32_t *now, int samples) {
    int shown = 0, more = 0;
    for (uint32_t i = 0; i < WORDS; i++) {
        for (int view = 0; view < VIEW_COUNT; view++) {
            const size_t at = (size_t)i * VIEW_COUNT + view;
            const int downs = g_downs[at];
            if (g_ups[at] || downs < MIN_DOWNS || downs * 2 > samples) continue;
            const double first = value_of(start[i], view), last = value_of(now[i], view);
            if (first <= 0 || first > 100000 || last < 0) continue;
            if (shown++ < MAX_REPORT) {
                fprintf(stderr, "lifesearch: 0x%08X %-5s %g -> %g, down %d times\n", FIRST + i * 4, VIEW_NAMES[view], first, last, downs);
            } else {
                more++;
            }
        }
    }
    fprintf(stderr, "lifesearch: %d samples, %d shown, %d more\n", samples, shown - more, more);
    fflush(stderr);
}

static int pressed(int key) {
    return (GetAsyncKeyState(key) & 1) != 0;
}

static unsigned __stdcall run(void *arg) {
    (void)arg;
    const size_t counts = (size_t)WORDS * VIEW_COUNT;
    uint32_t *start = malloc(SIZE), *before = malloc(SIZE), *now = malloc(SIZE);
    g_downs = malloc(counts);
    g_ups = malloc(counts);
    if (!start || !before || !now || !g_downs || !g_ups) {
        fprintf(stderr, "lifesearch: not enough memory\n");
        return 0;
    }
    const void *ram = psp_mem.ram + (FIRST - PSP_RAM_BASE);
    for (;;) {
        while (!pressed(VK_F9)) Sleep(50);
        Beep(880, 150);
        memcpy(start, ram, SIZE);
        memcpy(before, start, SIZE);
        memset(g_downs, 0, counts);
        memset(g_ups, 0, counts);
        fprintf(stderr, "lifesearch: started\n");
        int samples = 0;
        while (!pressed(VK_F10)) {
            Sleep(250);
            memcpy(now, ram, SIZE);
            compare(before, now);
            memcpy(before, now, SIZE);
            samples++;
        }
        Beep(440, 300);
        report(start, before, samples);
    }
}

void lifesearch_start(void) {
    const char *value = getenv("DIVA_LIFE_SEARCH");
    if (!value || value[0] != '1') return;
    _beginthreadex(NULL, 0, run, NULL, 0, NULL);
    fprintf(stderr, "lifesearch: F9 starts, F10 stops\n");
}

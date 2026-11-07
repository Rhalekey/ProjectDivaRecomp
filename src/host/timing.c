/* When a press counts, and how late the player's presses come.
 *
 * Each note keeps how many seconds are left until its moment, and the game compares that with a
 * table of limits when its button is pressed: 30 ms either way is a Cool, then 70, 100 and 130.
 * Sound and picture both reach the player some time after the game has made them, and a player
 * who follows them presses that much late. Moving all the limits together moves when a press is
 * on time, and leaves the notes and the song as they are.
 *
 * What is left on a note when it is pressed says how late that press was, so each song measures
 * it. The middle value is taken, as a stray press should not count for much.
 */

#include "timing.h"

#include "options.h"
#include "settings.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The function that sets the limits at the start of a song, and where it has just done so. */
#define SETUP 0x08857818u
#define SETUP_DONE 0x08857E70u
/* The function that plays the notes, and where the right button has just been pressed on one. */
#define NOTES 0x08858200u
#define NOTES_PRESS 0x08858314u
#define NOTES_CURRENT 52u
#define NOTE_SECONDS_LEFT 1132u

static const struct {
    uint32_t field;
    float seconds;
} LIMITS[] = {
    {13596, 0.03f}, {13600, -0.03f},
    {13604, 0.07f}, {13608, -0.07f},
    {13612, 0.10f}, {13616, -0.10f},
    {13620, 0.13f}, {13624, -0.13f},
};

#define PRESSES_KEPT 4096
#define PRESSES_NEEDED 20

static float g_presses[PRESSES_KEPT];
static int g_press_count, g_presses_logged;
static int g_have_measure, g_late_ms;

static void on_setup_done(void) {
    const uint32_t object = psp_cpu.r[PSP_REG_S0];
    const float shift = g_settings.extras ? (float)g_options.note_timing / 1000.0f : 0.0f;
    for (size_t i = 0; i < sizeof LIMITS / sizeof LIMITS[0]; i++) {
        const float value = LIMITS[i].seconds - shift;
        uint32_t bits;
        memcpy(&bits, &value, sizeof bits);
        psp_write32(object + LIMITS[i].field, bits);
    }
    g_press_count = g_presses_logged = 0;
}

static void on_press(void) {
    if (g_press_count == PRESSES_KEPT) return;
    const uint32_t note = psp_read32(psp_cpu.r[PSP_REG_S5] + NOTES_CURRENT);
    const uint32_t bits = psp_read32(note + NOTE_SECONDS_LEFT);
    float left;
    memcpy(&left, &bits, sizeof left);
    g_presses[g_press_count++] = -left * 1000.0f;
}

static int by_value(const void *a, const void *b) {
    const float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

/* The middle of `count` presses from `first` on, leaving those in their order. */
static float middle_of(int first, int count) {
    static float sorted[PRESSES_KEPT];
    memcpy(sorted, g_presses + first, (size_t)count * sizeof sorted[0]);
    qsort(sorted, (size_t)count, sizeof sorted[0], by_value);
    return sorted[count / 2];
}

void timing_song_stopped(void) {
    if (g_press_count < PRESSES_NEEDED || g_press_count == g_presses_logged) return;
    g_presses_logged = g_press_count;
    const int half = g_press_count / 2;
    const float all = middle_of(0, g_press_count);
    g_late_ms = (int)lroundf(all);
    g_have_measure = 1;
    /* The two halves tell a player's habit from a song and notes that drift apart. */
    fprintf(stderr, "timing: %d presses, %.1f ms late, first half %.1f, second half %.1f, set to %+d\n", g_press_count, (double)all,
            (double)middle_of(0, half), (double)middle_of(half, g_press_count - half), g_options.note_timing);
}

int timing_last_song(int *late_ms) {
    *late_ms = g_late_ms;
    return g_have_measure;
}

int timing_hook(uint32_t addr) {
    if (addr == SETUP || addr == NOTES) { /* only so that those below can be inside them */ }
    else if (addr == SETUP_DONE) on_setup_done();
    else if (addr == NOTES_PRESS) on_press();
    else return 0;
    return 1;
}

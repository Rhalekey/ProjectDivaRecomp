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
 *
 * The game sees the buttons once a frame, so a press counts as made when the frame read it, up to
 * 16 ms after it really was. The buttons are read about once a millisecond, so the press is known
 * to that, and the note is judged as it stood then: what was left on it then is what is compared
 * with the limits. The note itself is not touched.
 */

#include "timing.h"

#include "audio_out.h"
#include "options.h"
#include "settings.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
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
/* The register holding what is left on the note, compared with the limits next; the latest a press
 * still counts at all (13620), which the game has already checked against; and at most how long
 * before the frame read it a press is taken to have come, two frames. */
#define PRESS_LEFT_REG 2
#define LATEST_LIMIT 13620u
#define PRESS_AGE_MOST_US 34000u

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
static float g_queued_at[PRESSES_KEPT];   /* the sound waiting in the queue at each press, in ms */
static int g_press_count, g_presses_logged;
static int g_have_measure, g_late_ms;
static uint64_t g_age_sum_us;   /* how long before their frames the presses came, for the log */
static int g_age_count;

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
    g_age_sum_us = 0;
    g_age_count = 0;
}

static void on_press(void) {
    float left = psp_cpu.f[PRESS_LEFT_REG];
    if (g_settings.extras) {
        uint32_t age = psp_ctrl_press_age_us();
        if (age > PRESS_AGE_MOST_US) age = PRESS_AGE_MOST_US;
        const uint32_t latest_bits = psp_read32(psp_cpu.r[PSP_REG_S5] + LATEST_LIMIT);
        float latest;
        memcpy(&latest, &latest_bits, sizeof latest);
        g_age_sum_us += age;
        g_age_count++;
        left += (float)age / 1000000.0f;
        if (left > latest) left = latest;
        psp_cpu.f[PRESS_LEFT_REG] = left;
    }
    if (g_press_count < PRESSES_KEPT) {
        g_queued_at[g_press_count] = (float)audio_out_queued_now();
        g_presses[g_press_count++] = -left * 1000.0f;
    }
}

static int by_value(const void *a, const void *b) {
    const float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

/* The middle of `count` values from `first` on, leaving those in their order. */
static float middle_in(const float *values, int first, int count) {
    static float sorted[PRESSES_KEPT];
    memcpy(sorted, values + first, (size_t)count * sizeof sorted[0]);
    qsort(sorted, (size_t)count, sizeof sorted[0], by_value);
    return sorted[count / 2];
}

static float middle_of(int first, int count) { return middle_in(g_presses, first, count); }

void timing_song_stopped(void) {
    if (g_press_count < PRESSES_NEEDED || g_press_count == g_presses_logged) return;
    g_presses_logged = g_press_count;
    const int half = g_press_count / 2;
    const float all = middle_of(0, g_press_count);
    g_late_ms = (int)lroundf(all);
    g_have_measure = 1;
    /* The two halves tell a player's habit from a song and notes that drift apart. */
    fprintf(stderr, "timing: %d presses, %.1f ms late, first half %.1f, second half %.1f, set to %+d, pressed %.1f ms before their frames\n",
            g_press_count, (double)all, (double)middle_of(0, half), (double)middle_of(half, g_press_count - half), g_options.note_timing,
            g_age_count ? (double)g_age_sum_us / g_age_count / 1000.0 : 0.0);
    /* Sound that waits longer reaches the player later, and the presses that follow it with it. */
    fprintf(stderr, "timing: sound queued %.1f ms in the first half, %.1f in the second\n", (double)middle_in(g_queued_at, 0, half),
            (double)middle_in(g_queued_at, half, g_press_count - half));
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

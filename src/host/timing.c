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
 *
 * Each song also learns its own correction: some charts sit a little off the music, and a player
 * who is on time in one is late or early in another. After a song with enough presses, how late
 * they were against the setting moves that song's correction towards it, all the way the first
 * time and by a third after, so one bad try does not spoil it. It is added to the setting as the
 * song starts. They are kept in SONGTIMING.INI beside this port's options. The measure shown in
 * Options leaves the song's correction out, so that it still says how the setting stands.
 */

#include "timing.h"

#include "audio_out.h"
#include "language.h"
#include "options.h"
#include "settings.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <windows.h>

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

/* ---- each song's own correction ---------------------------------------------------------------- */
#define SONGS 64
#define SONG_MOST_MS 80          /* a song's correction goes no further than this either way */
#define SETUP_STILL_MS 15000u    /* a song that starts this soon after the limits were set is theirs */
static float g_song_ms[SONGS];
static int g_song_runs[SONGS], g_songs_read;
static int g_song = -1;          /* the song playing, -1 if not known */
static int g_applied_ms;         /* the song's correction as the limits were set */
static int g_learned_from;       /* the first press not yet learned from */
static uint32_t g_object;        /* the notes object the limits were set in, and when */
static ULONGLONG g_setup_ms;

static void song_timing_path(char *out, size_t cap, int make) {
    const char *data = language_data_dir();
    static const char *const PARTS[] = {"\\ms", "\\ms\\PSP", "\\ms\\PSP\\SAVEDATA", "\\ms\\PSP\\SAVEDATA\\ULJM05472RECOMP"};
    for (int i = 0; make && i < 4; i++) {
        snprintf(out, cap, "%s%s", data, PARTS[i]);
        _mkdir(out);
    }
    snprintf(out, cap, "%s%s\\SONGTIMING.INI", data, PARTS[3]);
}

static void read_song_timing(void) {
    g_songs_read = 1;
    char path[1200], line[96];
    song_timing_path(path, sizeof path, 0);
    FILE *f = fopen(path, "r");
    if (!f) return;
    int song, runs;
    float ms;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "song %d = %f %d", &song, &ms, &runs) == 3 && song >= 0 && song < SONGS && ms >= -SONG_MOST_MS && ms <= SONG_MOST_MS) {
            g_song_ms[song] = ms;
            g_song_runs[song] = runs > 0 ? runs : 1;
        }
    }
    fclose(f);
}

static void write_song_timing(void) {
    char path[1200];
    song_timing_path(path, sizeof path, 1);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "; Each song's own correction to Note timing, in ms, and how many plays it is from. Learned as you play.\n");
    for (int i = 0; i < SONGS; i++)
        if (g_song_runs[i]) fprintf(f, "song %d = %.1f %d\n", i, (double)g_song_ms[i], g_song_runs[i]);
    fclose(f);
}

static int song_correction(void) {
    if (!g_settings.extras || g_song < 0 || g_song >= SONGS) return 0;
    if (!g_songs_read) read_song_timing();
    return (int)lroundf(g_song_ms[g_song]);
}

static void write_limits(uint32_t object) {
    g_applied_ms = song_correction();
    const float shift = g_settings.extras ? (float)(g_options.note_timing + g_applied_ms) / 1000.0f : 0.0f;
    for (size_t i = 0; i < sizeof LIMITS / sizeof LIMITS[0]; i++) {
        const float value = LIMITS[i].seconds - shift;
        uint32_t bits;
        memcpy(&bits, &value, sizeof bits);
        psp_write32(object + LIMITS[i].field, bits);
    }
}

void timing_song_started(int song) {
    g_song = song >= 0 && song < SONGS ? song : -1;
    /* The limits may have been set just before the song was known: set again, with its correction. */
    if (g_object && GetTickCount64() - g_setup_ms < SETUP_STILL_MS) {
        write_limits(g_object);
        if (g_applied_ms) fprintf(stderr, "timing: song %d has its own correction, %+d ms\n", g_song, g_applied_ms);
    }
}

static void on_setup_done(void) {
    g_object = psp_cpu.r[PSP_REG_S0];
    g_setup_ms = GetTickCount64();
    write_limits(g_object);
    if (g_applied_ms) fprintf(stderr, "timing: song %d has its own correction, %+d ms\n", g_song, g_applied_ms);
    g_learned_from = 0;
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
    /* When the first notes were pressed, on the clock the renderer's DIVA_LOG_DRAWS uses, to tell
     * the pictures of a press's effect from the rest. */
    if (g_press_count < 3) fprintf(stderr, "timing: press %d at %.2f s\n", g_press_count + 1, (double)GetTickCount64() / 1000.0);
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

/* The presses since the last time move the song's correction towards how late they were against
 * the setting. */
static void learn_song(void) {
    const int n = g_press_count - g_learned_from;
    if (!g_settings.extras || g_song < 0 || g_song >= SONGS || n < PRESSES_NEEDED) return;
    if (!g_songs_read) read_song_timing();
    const float late = middle_of(g_learned_from, n), aim = late - (float)g_options.note_timing;
    const float before = g_song_ms[g_song], weight = g_song_runs[g_song] ? 1.0f / 3.0f : 1.0f;
    float now = before + (aim - before) * weight;
    if (now > SONG_MOST_MS) now = SONG_MOST_MS;
    if (now < -SONG_MOST_MS) now = -SONG_MOST_MS;
    g_song_ms[g_song] = now;
    g_song_runs[g_song]++;
    g_learned_from = g_press_count;
    write_song_timing();
    fprintf(stderr, "timing: song %d's own correction %+.1f -> %+.1f ms (presses %.1f ms late against the setting)\n", g_song, (double)before,
            (double)now, (double)aim);
}

void timing_song_stopped(void) {
    if (g_press_count < PRESSES_NEEDED || g_press_count == g_presses_logged) return;
    g_presses_logged = g_press_count;
    learn_song();
    const int half = g_press_count / 2;
    const float all = middle_of(0, g_press_count);
    /* What Options shows leaves the song's own correction out. */
    g_late_ms = (int)lroundf(all) - g_applied_ms;
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

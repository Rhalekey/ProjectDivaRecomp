/* Practice: a song that cannot be failed and scores nothing. See practice.h. */
#include "practice.h"

#include "language.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>
#include <psprecomp/render.h>

#include <stdio.h>

/* Each note's points go through 0886AF54 (song, points, kept up, ...), which adds them and the
 * combo's bonus to the song's score. Both ways through it then reach 0886B08C, with the song in s3. */
#define SCORE_ADDED 0x0886B08Cu
#define SONG_SCORE (0x10000u - 17428u)

static int g_on;

static const char *const SHOWN_ON[UI_COUNT] = {
    [UI_ENGLISH] = "Practice on: you cannot fail, and notes score nothing",
    [UI_JAPANESE] = "\xE7\xB7\xB4\xE7\xBF\x92\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89\xEF\xBC\x9A\xE3\x82\xAA\xE3\x83\xB3\xE3\x80\x80\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\xAA\xE3\x83\xBC\xE3\x83\x90\xE3\x83\xBC\xE3\x81\xAA\xE3\x81\x97\xE3\x80\x81\xE3\x82\xB9\xE3\x82\xB3\xE3\x82\xA2\xE3\x81\xAF\xE8\xA8\x98\xE9\x8C\xB2\xE3\x81\x95\xE3\x82\x8C\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93",
    [UI_GERMAN] = "Übung an: kein Scheitern, Noten geben keine Punkte",
};
static const char *const SHOWN_OFF[UI_COUNT] = {
    [UI_ENGLISH] = "Practice off",
    [UI_JAPANESE] = "\xE7\xB7\xB4\xE7\xBF\x92\xE3\x83\xA2\xE3\x83\xBC\xE3\x83\x89\xEF\xBC\x9A\xE3\x82\xAA\xE3\x83\x95",
    [UI_GERMAN] = "Übung aus",
};

void practice_toggle(void) {
    g_on = !g_on;
    psp_render_gl_show_message((g_on ? SHOWN_ON : SHOWN_OFF)[language_ui()]);
    fprintf(stderr, "practice: %s\n", g_on ? "on" : "off");
}

int practice_on(void) { return g_on; }

int practice_hook(uint32_t addr) {
    if (addr == SCORE_ADDED) {
        if (g_on) psp_write32(psp_cpu.r[PSP_REG_S3] + SONG_SCORE, 0);
        return 1;
    }
    return 0;
}

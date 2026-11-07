/* The language the game is in: its text, its pictures, and the project's own rows. All of it
 * can change while the game runs, because what the game loaded stays Japanese and is only
 * swapped on its way to the screen. */

#include "language.h"

#include "extras.h"
#include "options.h"
#include "settings.h"
#include "translation.h"

#include <psprecomp/render.h>

#include <stdio.h>
#include <string.h>

static char g_data_dir[1100];

void language_start(const char *data_dir) {
    snprintf(g_data_dir, sizeof g_data_dir, "%s", data_dir);
}

int language_ui(void) {
    if (!strcmp(g_options.language, "ja")) return UI_JAPANESE;
    if (!strcmp(g_options.language, "de")) return UI_GERMAN;
    return UI_ENGLISH;
}

static int g_vectors_off;

/* Those of the language being played. Japanese has no words for them yet. */
static void pick_vectors(void) {
    char pictures[1200];
    if (g_options.confirm_cross == 1) {
        snprintf(pictures, sizeof pictures, "%s\\..\\textures_hd\\vector\\%s_x|%s\\..\\textures_hd\\vector\\%s", g_data_dir,
                 g_options.language, g_data_dir, g_options.language);
    } else {
        snprintf(pictures, sizeof pictures, "%s\\..\\textures_hd\\vector\\%s", g_data_dir, g_options.language);
    }
    psp_render_gl_set_vector_dir(g_vectors_off || !strcmp(g_options.language, "ja") ? NULL : pictures);
}

void language_confirm_changed(void) { pick_vectors(); }

void language_show_vectors(int on) {
    g_vectors_off = !on;
    pick_vectors();
}

void language_apply(const char *code) {
    /* The code may be in the place it is copied to. */
    char chosen[sizeof g_options.language];
    snprintf(chosen, sizeof chosen, "%s", code);
    snprintf(g_options.language, sizeof g_options.language, "%s", chosen);

    const int japanese = !strcmp(chosen, "ja");
    translation_set_language(chosen);
    char pictures[1200];
    snprintf(pictures, sizeof pictures, "%s\\..\\lang\\%s", g_data_dir, chosen);
    psp_render_gl_set_language_dir(japanese ? NULL : pictures);
    pick_vectors();
    if (g_settings.extras) extras_language_changed();
}

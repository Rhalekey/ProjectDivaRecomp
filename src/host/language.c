/* The language the game is in: its text, its pictures, and the project's own rows. All of it
 * can change while the game runs, because what the game loaded stays Japanese and is only
 * swapped on its way to the screen. */

#include "language.h"

#include "extras.h"
#include "note_skins.h"
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

const char *language_data_dir(void) { return g_data_dir; }

int language_ui(void) {
    if (!strcmp(g_options.language, "ja")) return UI_JAPANESE;
    if (!strcmp(g_options.language, "de")) return UI_GERMAN;
    return UI_ENGLISH;
}

static int g_vectors_off;

/* Those of the language being played. Japanese has no words for them yet. */
static void pick_vectors(void) {
    char pictures[1200];
    /* The folder beside the language's own with what differs: _x for Cross confirming, and in the
     * styles with letters _x or _n, for Xbox or Nintendo, with a or b for the letter that confirms. */
    const int letter = options_letter_for(g_options.confirm_cross == 1);
    char with_letter[8];
    snprintf(with_letter, sizeof with_letter, "_%c%c", g_options.button_style == OPTIONS_STYLE_NINTENDO ? 'n' : 'x', letter == 'A' ? 'a' : 'b');
    const char *differs = letter ? with_letter : g_options.confirm_cross == 1 ? "_x" : NULL;
    if (differs) {
        snprintf(pictures, sizeof pictures, "%s\\..\\textures_hd\\vector\\%s%s|%s\\..\\textures_hd\\vector\\%s", g_data_dir,
                 g_options.language, differs, g_data_dir, g_options.language);
    } else {
        snprintf(pictures, sizeof pictures, "%s\\..\\textures_hd\\vector\\%s", g_data_dir, g_options.language);
    }
    psp_render_gl_set_vector_dir(g_vectors_off || !strcmp(g_options.language, "ja") ? NULL : pictures);
}

void language_confirm_changed(void) { pick_vectors(); }

void language_notes_changed(void) {
    static const char *const FOLDER[OPTIONS_NOTES_COUNT] = {NULL, "xbox", "nintendo", "keyboard"};
    if (g_options.note_style >= OPTIONS_NOTES_COUNT) {
        /* A note skin from the mods folder: if it has gone, the game's own notes. */
        const char *skin = note_skins_folder(g_options.note_style - OPTIONS_NOTES_COUNT);
        if (!skin) g_options.note_style = OPTIONS_NOTES_PLAYSTATION;
        psp_render_gl_set_note_dir(skin);
        return;
    }
    const char *folder = g_options.note_style > 0 ? FOLDER[g_options.note_style] : NULL;
    char notes[1200];
    if (folder) snprintf(notes, sizeof notes, "%s\\..\\textures_hd\\notes\\%s", g_data_dir, folder);
    psp_render_gl_set_note_dir(folder ? notes : NULL);
}

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

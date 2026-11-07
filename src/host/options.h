#ifndef DIVA_OPTIONS_H
#define DIVA_OPTIONS_H

#include <psprecomp/window.h>

/* What the player chose in the game, kept with the saves. */
typedef struct {
    char language[8];    /* "ja", "en" or "de" */
    char song_names[12]; /* "japanese" or "romaji" */
    char lyrics[12];     /* "japanese" or "romaji" */
    int ask_language;    /* over the edition's language_prompt; -1 leaves it to the edition */
    int show_fps;
    int confirm_cross;   /* 1 if Cross confirms in menus, 0 if Circle, -1 if not asked yet */
    int keys[PSP_BIND_COUNT]; /* each PSP button's key; -1 leaves it as it comes */
    int pads[PSP_BIND_COUNT]; /* and controller buttons */
    int display_mode;    /* a PSP_WINDOW_ mode */
    int render_scale;    /* times the PSP's resolution; 0 leaves it to the edition */
    int smoothing;       /* of the picture as it fits the window; -1 leaves it to the edition */
    int wide_view;       /* 0 bars, 1 crops the 3D scene, 2 shows more of it */
    int keys2[PSP_BIND_COUNT]; /* each button's second key, 0 for none; -1 leaves it as it comes */
} diva_options;

extern diva_options g_options;

/* Returns nonzero if a language had been chosen before. */
int options_load(const char *data_dir);
void options_save(const char *data_dir);

#endif

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
    int background_dim;  /* how dark the background is in a song, from 0, for not at all, to 100 */
    int note_timing;     /* in ms, how much later than the note's moment a press counts as on time */
    int blur;            /* blurred copies of pictures past their sides in a wider window */
    int button_style;    /* the signs shown for Cross and Circle: an OPTIONS_STYLE_ */
    int note_style;      /* how the notes and targets look: an OPTIONS_NOTES_, or past them a note skin */
    char note_skin[48];  /* that note skin's name, kept by name as the skins found can change */
    char module_skin[48]; /* the module skin worn over its base, by name; empty for none */
    int restart_key;     /* the key that plays a song again from its start, 0 for none */
    int restart_pad;     /* the controller buttons that do it, held together; 0 for L and R */
    int singer_module[2]; /* the module Kagamine Rin, then Len, sing their own songs in */
    int dpad_notes;      /* what hits notes in songs besides the buttons: OPTIONS_HIT_ bits */
    int free_singers;    /* Rin's and Len's songs sung in the module worn, not forced to them */
    int keep_vocal;      /* a missed note leaves the singing on, where the game silences it */
} diva_options;

/* The note timing can be set this far either way. Past 130 before, a note could never be late. */
#define OPTIONS_TIMING_LEAST (-100)
#define OPTIONS_TIMING_MOST 300

/* PlayStation shows Circle and Cross; Xbox shows Cross, the lower button, as A and Circle as B;
 * Nintendo the other way round, its A being on the right. */
enum { OPTIONS_STYLE_PLAYSTATION, OPTIONS_STYLE_XBOX, OPTIONS_STYLE_NINTENDO, OPTIONS_STYLE_COUNT };
/* The notes: the game's own shapes, letters in Xbox's or Nintendo's colours, or arrow keys. */
/* Each way of these is the button on that side in songs: none, the d-pad, the stick, or both. */
enum { OPTIONS_HIT_OFF = 0, OPTIONS_HIT_DPAD = 1, OPTIONS_HIT_STICK = 2, OPTIONS_HIT_BOTH = 3 };
enum { OPTIONS_NOTES_PLAYSTATION, OPTIONS_NOTES_XBOX, OPTIONS_NOTES_NINTENDO, OPTIONS_NOTES_KEYBOARD, OPTIONS_NOTES_COUNT };

/* The face buttons, by where they are: Cross below, Circle right, Square left, Triangle above. */
enum { OPTIONS_FACE_CROSS, OPTIONS_FACE_CIRCLE, OPTIONS_FACE_SQUARE, OPTIONS_FACE_TRIANGLE };

/* The letter a face button's sign is in this style, 'A', 'B', 'X' or 'Y', or 0 for its own sign. */
int options_face_letter(int face);
/* The same for Cross, or with 0 for Circle. */
int options_letter_for(int cross);

extern diva_options g_options;

/* Returns nonzero if a language had been chosen before. */
int options_load(const char *data_dir);
void options_save(const char *data_dir);

#endif

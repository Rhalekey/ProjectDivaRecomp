#ifndef DIVA_SETTINGS_H
#define DIVA_SETTINGS_H

#include <stdint.h>

typedef struct {
    char renderer[16];             /* "gl" or "software" */
    int render_scale;              /* times the PSP's resolution */
    int present_linear;            /* smooth the picture when fitting the window */
    uint32_t interrupt_offset_us;  /* how long after a refresh the vblank interrupt runs */
    int save_messages;             /* say on screen what the save screens would have */
    int mods;
    int translations;              /* languages other than Japanese can be played */
    int language_prompt;           /* ask on every start; off, ask only if nothing is saved */
    int hd_textures;               /* use the fonts and packs prepared in textures_hd */
    int extras;                    /* the project's own rows in the game's Options window */
} diva_settings;

extern diva_settings g_settings;

/* Read the edition's settings and the player's own. See settings.c for the order. */
void settings_load(const char *data_dir);

/* Test switches from dev.ini beside the player's data. See settings.c. Goes before anything else. */
void settings_load_dev(const char *data_dir);

#endif

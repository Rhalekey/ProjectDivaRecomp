#ifndef DIVA_EXTRAS_H
#define DIVA_EXTRAS_H

#include <stdint.h>

/* The functions of the game's Options window that Tools\build.ps1 hooks. */
#define EXTRAS_WINDOW_UPDATE 0x08AB5908u
#define EXTRAS_WINDOW_DRAW 0x08AB5A4Cu
#define EXTRAS_WINDOW_ENTER 0x08AB5448u
#define EXTRAS_WINDOW_TITLE 0x08AB5528u
#define EXTRAS_WINDOW_VOLUME 0x08AB59A0u
#define EXTRAS_MENU_ROW_READ 0x08AB4384u
#define EXTRAS_WINDOW_ROWS 0x08AB5EF0u
#define EXTRAS_WINDOW_ROWS_MARKED 0x08AB5F38u
/* And those of the main menu. */
#define EXTRAS_MENU_ENTER 0x08AB41C4u
#define EXTRAS_MENU_DRAW 0x08AB4C58u
#define EXTRAS_MENU_CONFIRM 0x08AB4D18u

/* Call after the game is loaded and options_load, before the game starts. */
void extras_start(const char *data_dir);

/* g_options.language has changed. */
void extras_language_changed(void);

/* The next note style, the game's own then the note skins, put to use. */
void extras_next_note_style(void);
/* One step along the note styles: 1 forward, -1 back. */
void extras_step_note_style(int step);
/* A note skin's name as the menu shows it, while one is chosen. */
const char *extras_note_style_name(void);

/* Generated code has reached a hooked function. Returns nonzero if it was one of these. */
int extras_hook(uint32_t addr);

#endif

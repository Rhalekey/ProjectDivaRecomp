#ifndef DIVA_LANGUAGE_SCREEN_H
#define DIVA_LANGUAGE_SCREEN_H

/* Ask which language to play in. Returns a code of lang\languages.txt; `current` starts selected.
 * `disc` is the folder holding PSP_GAME, where the game's menu art is. */
const char *language_screen_run(const char *current, const char *disc);

#endif

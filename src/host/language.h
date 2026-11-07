#ifndef DIVA_LANGUAGE_H
#define DIVA_LANGUAGE_H

/* The languages the project's own words are written in. */
enum { UI_ENGLISH, UI_JAPANESE, UI_GERMAN, UI_COUNT };

/* The one being played, which is English for any it has no words in. */
int language_ui(void);

void language_start(const char *data_dir);

/* Play in this language from now on: "ja", "en" or "de". It does not save the choice. */
void language_apply(const char *code);

/* Whether the menus' pieces are drawn from vectors or are the game's own. */
void language_show_vectors(int on);

/* The player has changed which button confirms, which the pictures show. */
void language_confirm_changed(void);

#endif

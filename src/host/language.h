#ifndef DIVA_LANGUAGE_H
#define DIVA_LANGUAGE_H

void language_start(const char *data_dir);
/* The folder the saves and the made pictures are found from, as given to language_start. */
const char *language_data_dir(void);

/* Play in this language from now on, by its code in lang\languages.txt. It does not save the choice. */
void language_apply(const char *code);

/* Whether the menus' pieces are drawn from vectors or are the game's own. */
void language_show_vectors(int on);

/* The player has changed which button confirms, which the pictures show. */
void language_confirm_changed(void);

/* The player has chosen how the notes look: the renderer takes their pictures from that folder. */
void language_notes_changed(void);

#endif

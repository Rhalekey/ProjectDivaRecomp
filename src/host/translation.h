#ifndef DIVA_TRANSLATION_H
#define DIVA_TRANSLATION_H

#include <stdint.h>

/* Load the table of each language in <dir>. None is shown until asked for. */
void translation_start(const char *dir, const char *data_dir);

/* Show the interface in this language. One with no table, as Japanese, leaves it as it is. */
void translation_set_language(const char *code);

/* Load the song names and lyrics in romaji from <dir>, which are shown only once asked for. */
void translation_romaji_start(const char *dir);
int translation_has_song_romaji(void);
/* What is shown in place of a text of the game's in the language being played, or NULL if it is
 * shown as it is. */
const char *translation_shown(const char *original);
/* A song name in romaji, whether or not it is being shown, or NULL if it has none. */
const char *translation_song_romaji(const char *original);
int translation_has_lyric_romaji(void);
void translation_set_romaji(int song_names, int lyrics);

/* Generated code has reached one of the functions that handle text. */
void translation_hook(uint32_t addr);

#endif

#ifndef DIVA_UI_TEXT_H
#define DIVA_UI_TEXT_H

#include <stddef.h>
#include <wchar.h>

/* The project's own words, in every language it is played in, from lang\:
 *
 *   languages.txt   the languages, in the order the language screen lists them: code | name
 *   <code>_ui.txt   key = text, UTF-8, for each
 *   letters.txt     the letters the game's font lacks, and the bytes of its they are drawn over
 *
 * A word missing from a language is taken from English, and failing that is its key. What comes
 * back is kept until the game closes. */

#define UI_TEXT_MOST_LANGUAGES 12

void ui_text_start(const char *lang_dir);

int ui_text_languages(void);
const char *ui_text_code(int language);
/* The language's name in itself, as wide text for the language screen. */
const wchar_t *ui_text_native_name(int language);
/* Which of them a code is, or -1. */
int ui_text_find(const char *code);

/* The language words are given in from now on; one with no words of its own gives English's. */
void ui_text_set_language(const char *code);
/* The language words are given in, by number. */
int ui_text_language(void);

/* A word: UTF-8, as wide text, or as the game's font reads it (Shift-JIS, with the letters it lacks
 * in the places of letters.txt). The _in forms take the language by number. */
const char *ui_text(const char *key);
const wchar_t *ui_wtext(const char *key);
const char *ui_game_text(const char *key);
const char *ui_text_in(int language, const char *key);
const wchar_t *ui_wtext_in(int language, const char *key);
const char *ui_game_text_in(int language, const char *key);

/* UTF-8 text as the game's font reads it, as above. */
void ui_text_to_game(const char *utf8, char *out, size_t size);
/* The letter a byte of the game's font stands for in letters.txt, or 0 if it is not one of them. */
wchar_t ui_text_letter_of(unsigned char byte);

#endif

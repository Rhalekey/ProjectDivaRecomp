#ifndef DIVA_LETTERS_H
#define DIVA_LETTERS_H

#include <stdint.h>

/* Reads what Tools\hd_font.py measured, from the folder it wrote to. With nothing to read,
 * text stays where the game puts it. */
void letters_load(const char *dir);

/* How wide this text is by its letters, in the game's pixels, in font size `size`. */
float letters_width(const char *text, uint32_t size, int32_t spacing);

/* The same in the small font, which lists use, with no spacing. */
float letters_width_small(const char *text);
#define LETTERS_SMALL_SPACE 6.0f   /* and how wide a space between words of a list row is */

/* How wide the game takes it to be, and places it by. */
float letters_room(const char *text, uint32_t size, int32_t spacing);

/* The text about to be drawn. In a grid, each letter stays in a cell of the game's. `placed`
 * says its position has been worked out from letters_width, so it has no need to fit its room. */
void letters_begin(uint32_t text, uint32_t size, int32_t spacing, int grid, int placed);

/* Nonzero if the hook was this file's. */
int letters_hook(uint32_t addr);

#endif

#ifndef DIVA_PICTURES_H
#define DIVA_PICTURES_H

#include <stdint.h>

/* A picture file's pixels as R, G, B, A, top row first, or NULL; free() them. PNG, JPEG, BMP, GIF, TGA. */
uint8_t *picture_read(const char *path, unsigned *w, unsigned *h);

#endif

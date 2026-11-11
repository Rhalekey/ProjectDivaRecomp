#ifndef DIVA_MODEL_CONVERT_H
#define DIVA_MODEL_CONVERT_H

#include "model.h"

/* The game's own files a conversion starts from: module 0's body and head, and a song's motions,
 * which hold Miku's skeleton. */
typedef struct {
    const uint8_t *body, *head, *anim;
    size_t body_size, head_size, anim_size;
} model_templates;

/* A material's picture: R, G, B, A rows, top first; NULL pixels to keep module 0's. */
typedef struct {
    const uint8_t *pixels;
    unsigned w, h;
} model_picture;

/* The model, with a picture for each of its materials, made into the game's body and head files;
 * free() them. Returns 0 and says why in err if it cannot. */
int model_convert(const model *m, const model_picture *pictures, const model_templates *t, uint8_t **body, size_t *body_size,
                  uint8_t **head, size_t *head_size, char *err, size_t errlen);

#endif

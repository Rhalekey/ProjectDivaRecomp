#ifndef DIVA_MODEL_H
#define DIVA_MODEL_H

/* A character model as read from a common format, before it is turned into the game's: triangles
 * with their corners whole, the bones that move them, and each material's picture. */

#include <stddef.h>
#include <stdint.h>

#define MODEL_WEIGHTS 4

typedef struct {
    float pos[3], normal[3], uv[2];
    int bone[MODEL_WEIGHTS];        /* -1 for none */
    float weight[MODEL_WEIGHTS];
} model_corner;

typedef struct {
    char name[64];
    int parent;                     /* -1 for none */
    float head[3];                  /* where it is in the bind pose, in the model's space */
} model_bone;

typedef struct {
    int ncorners;                   /* three to a triangle */
    model_corner *corners;
    int *material;                  /* per triangle */
    int nbones;
    model_bone *bones;
    int nmaterials;
    char (*picture)[520];           /* the picture each material names, as written in the file */
} model;

/* Fills m from a binary FBX file; returns 0 and says why in err if it cannot. Y is up. */
int model_load_fbx(const char *path, model *m, char *err, size_t errlen);
/* The same from a PMX file (MikuMikuDance, 2.0 or 2.1). */
int model_load_pmx(const char *path, model *m, char *err, size_t errlen);
/* Collapses edges until at most `target` triangles are left (or no more can go without tearing or
 * turning a triangle over). Returns how many are left. */
int model_simplify(model *m, int target);
void model_free(model *m);

/* Decompresses zlib data (as FBX arrays are) into out, of the size expected; nonzero if done. */
int model_inflate(const uint8_t *in, size_t n, uint8_t *out, size_t want);

#endif

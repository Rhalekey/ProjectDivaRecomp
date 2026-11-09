#ifndef DIVA_SONG_SORT_H
#define DIVA_SONG_SORT_H

#include <stdint.h>

/* The footer's extra hints, as Tools\ui_vector.py numbers them (EXTRA_HINTS). The song list's come
 * with its PV hint by themselves (EXTRA_WITH); other screens ask for theirs. */
#define FOOTER_SORT   (1u << 0)   /* Select: Sort */
#define FOOTER_MODULE (1u << 1)   /* L R: Module */

/* Generated code has reached a hooked function. Returns nonzero if it was this file's. */
int song_sort_hook(uint32_t addr);

/* When a song was last played in a module (seconds since 1970), 0 if never: for module_sort.c. */
long long song_sort_module_played(int module);

#endif

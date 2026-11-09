#ifndef DIVA_SONG_SORT_H
#define DIVA_SONG_SORT_H

#include <stdint.h>

/* Generated code has reached a hooked function. Returns nonzero if it was this file's. */
int song_sort_hook(uint32_t addr);

/* When a song was last played in a module (seconds since 1970), 0 if never: for module_sort.c. */
long long song_sort_module_played(int module);

#endif

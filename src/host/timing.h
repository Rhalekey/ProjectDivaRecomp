#ifndef DIVA_TIMING_H
#define DIVA_TIMING_H

#include <stdint.h>

/* Call from the hook for the functions named in timing.c. Returns nonzero if it was one of them. */
int timing_hook(uint32_t addr);

/* A song is starting, by its number: its own correction is added to the limits. */
void timing_song_started(int song);

/* A song has stopped or paused: works out what it measured, and logs it. */
void timing_song_stopped(void);

/* Nonzero if a song has given a measure: how many ms late its presses were, before any setting. */
int timing_last_song(int *late_ms);

#endif

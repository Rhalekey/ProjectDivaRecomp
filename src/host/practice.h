/* Practice: Start in the Free Play song list switches it on or off. In practice a song cannot be
 * failed and no note scores: the score stays at 0, so there is nothing to keep or to unlock. */
#ifndef DIVA_PRACTICE_H
#define DIVA_PRACTICE_H

#include <stdint.h>

/* Start was just pressed in the song list. */
void practice_toggle(void);

/* Whether practice is on. */
int practice_on(void);

/* Generated code has reached a hooked function. Returns nonzero if it was this file's. */
int practice_hook(uint32_t addr);

#endif

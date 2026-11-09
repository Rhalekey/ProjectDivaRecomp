/* What several files need of the game while it runs: calling its functions, telling its memory
 * from anything else, and where it keeps its windows. */
#ifndef DIVA_GUEST_H
#define DIVA_GUEST_H

#include <stdint.h>

/* The game's memory, where any of its objects can be. */
#define GUEST_RAM_START 0x08800000u
#define GUEST_RAM_END   0x0A000000u

static inline int guest_in_ram(uint32_t at) { return at >= GUEST_RAM_START && at < GUEST_RAM_END; }

/* The game's window manager: its windows, each with its lists. */
#define GAME_WINDOW_MANAGER 0x08C43278u

/* A function of the game's, called from a hook as at that function's entry: its registers are put
 * back after, so the hooked function finds them as they were. Gives back what it returned. */
uint32_t guest_call(uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3);

/* The same, with a float in f12 too, put back after as well. */
uint32_t guest_call_f(uint32_t function, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, float f12);

#endif

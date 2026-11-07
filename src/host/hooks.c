/* Where generated code calls the host. Tools\build.ps1 names the functions that do. */

#include "hooks.h"

#include "controls.h"
#include "extras.h"
#include "timing.h"
#include "translation.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <stdlib.h>

#define MOVIE_START 0x08AD49B4u
/* What changes life is told to start from the top each time, and no change empties it. Life is a
 * field of the song's object, past the first 64 KB. */
#define LIFE_CHANGE 0x0886B304u
#define LIFE_FIELD  (0x10000u - 17432u)
#define LIFE_FULL   255u

enum { REG_A0 = 4 };

static uint32_t g_empty_player;

void hooks_start(void) {
    /* A movie player that has nothing loaded: its first word says so. */
    g_empty_player = psp_sysmem_alloc(16, 1);
    if (g_empty_player) psp_write32(g_empty_player, 0);
}

/* There is no video decoder yet. Started with a player that has nothing loaded, the
 * game's own check sends it straight back, as if there were no movie to show. */
static void on_movie_start(void) {
    if (g_empty_player) psp_cpu.r[REG_A0] = g_empty_player;
}

/* With DIVA_NO_DEATH set a song cannot be lost, to look at the picture and not play. */
static void on_life_change(void) {
    static int no_death = -1;
    if (no_death < 0) no_death = getenv("DIVA_NO_DEATH") != NULL;
    if (no_death) psp_write32(psp_cpu.r[REG_A0] + LIFE_FIELD, LIFE_FULL);
}

void psp_game_hook(uint32_t addr) {
    if (addr == LIFE_CHANGE) on_life_change();
    else if (addr == MOVIE_START) on_movie_start();
    else if (!controls_hook(addr) && !extras_hook(addr) && !timing_hook(addr)) translation_hook(addr);
}

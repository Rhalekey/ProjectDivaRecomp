/* Where generated code calls the host. Tools\build.ps1 names the functions that do. */

#include "hooks.h"
#include "module_skins.h"

#include "controls.h"
#include "extras.h"
#include "practice.h"
#include "presence.h"
#include "song_sort.h"
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

/* The song list's panel (this, song, level): its object leads to the save's scores object. */
#define SONG_PANEL 0x08AC00B0u
#define PANEL_GAME 4u
#define GAME_SCORES 29328u

enum { REG_A0 = 4, REG_A1 = 5, REG_A2 = 6 };

static uint32_t g_empty_player;

void hooks_start(void) {
    /* A movie player that has nothing loaded: its first word says so. */
    g_empty_player = psp_sysmem_alloc(16, 1);
    if (g_empty_player) psp_write32(g_empty_player, 0);
}

/* Movies play when FFmpeg is beside the game (see psprecomp's mpeg.c). Without it, the player is
 * started with nothing loaded, and the game's own check sends it straight back, as if there were
 * no movie to show. */
static void on_movie_start(void) {
    if (!psp_mpeg_available() && g_empty_player) psp_cpu.r[REG_A0] = g_empty_player;
}

/* In practice a song cannot be lost; nor with DIVA_NO_DEATH set, to look at the picture and not play. */
static void on_life_change(void) {
    static int no_death = -1;
    if (no_death < 0) no_death = getenv("DIVA_NO_DEATH") != NULL;
    if (no_death || practice_on()) psp_write32(psp_cpu.r[REG_A0] + LIFE_FIELD, LIFE_FULL);
}

void psp_game_hook(uint32_t addr) {
    if (addr == SONG_PANEL) {
        const uint32_t game = psp_read32(psp_cpu.r[REG_A0] + PANEL_GAME);
        presence_chosen(game ? psp_read32(game + GAME_SCORES) : 0, (int)psp_cpu.r[REG_A1], (int)psp_cpu.r[REG_A2]);
    } else if (addr == LIFE_CHANGE) on_life_change();
    else if (addr == MOVIE_START) on_movie_start();
    else if (!controls_hook(addr) && !extras_hook(addr) && !timing_hook(addr) && !song_sort_hook(addr) && !practice_hook(addr) && !module_skins_hook(addr))
        translation_hook(addr);
}

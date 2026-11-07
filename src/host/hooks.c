/* Where generated code calls the host. Tools\build.ps1 names the functions that do. */

#include "hooks.h"

#include "bot.h"
#include "controls.h"
#include "extras.h"
#include "translation.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#define MOVIE_START 0x08AD49B4u

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

void psp_game_hook(uint32_t addr) {
    bot_hook(addr);
    if (addr == MOVIE_START) on_movie_start();
    else if (!controls_hook(addr) && !extras_hook(addr)) translation_hook(addr);
}

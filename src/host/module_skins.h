#ifndef DIVA_MODULE_SKINS_H
#define DIVA_MODULE_SKINS_H

#include <stdint.h>

/* Looks from mods\modules, added to the module screen's list, models converted from the game's own
 * files on the disc into a cache beside the data. See module_skins.c. Call before the game starts,
 * after options_load. Returns how many. */
int module_skins_scan(const char *mods_dir, const char *disc_dir, const char *data_dir);

/* Called as the game reads the buttons, with the module screen's list, or 0 if it is not up. */
void module_skins_screen(uint32_t list);

/* The skin whose row has these words, or -1: a row of the module list that is no module of the game. */
int module_skins_row(uint32_t words);

/* Nonzero if addr was one of its hooks. */
int module_skins_hook(uint32_t addr);

#endif

#ifndef DIVA_MODS_H
#define DIVA_MODS_H

/* Load the mods in this folder, if any. Returns how many files they replace. */
int mods_init(const char *dir);

/* Call this function, once, when the game first reads an entry of an archive. The game waits
 * for it to return. Call before the game starts. */
void mods_on_first_read(const char *entry, void (*function)(void));

#endif

#ifndef DIVA_MODS_H
#define DIVA_MODS_H

#include <stdint.h>

/* Load the mods in this folder, if any. Returns how many files they replace. */
int mods_init(const char *dir);

/* Call this function, once, when the game first reads an entry of an archive. The game waits
 * for it to return. Call before the game starts. */
void mods_on_first_read(const char *entry, void (*function)(void));

/* Adds an entry past the end of DivaDataList.afs, under this name, of this size: the game keeps
 * every entry's size from the start. Call before the game starts. Returns its place among those
 * added, or -1. It reads as noughts until it is given a file. */
int mods_add_entry(const char *name, uint32_t room);
/* The file an added entry is read from from now on (past its end, noughts). */
void mods_set_entry_file(int k, const char *path);
/* The archive entry number the game knows the k-th added file by, once the archive is open; or -1. */
int mods_added_entry(int k);

#endif

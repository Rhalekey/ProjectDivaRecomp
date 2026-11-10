/* Rhythm Game Edit's data shared by song: a zip with the song's MP3, if it is the player's own,
 * and every Edit made with it. Exported to Documents\Project DIVA Recomp\Export; a zip put in
 * Documents\Project DIVA Recomp\Import is brought in when the list to load from opens. */
#ifndef DIVA_EDIT_SHARE_H
#define DIVA_EDIT_SHARE_H

#include <psprecomp/hle.h>
#include <wchar.h>

/* Exports the song of slots[chosen] with every Edit of it. Says what came of it in `message`.
 * Gives back 1 if a file was written. */
int edit_share_export(const psp_save_slot *slots, int count, int chosen, wchar_t *message, int cap);

/* Brings in every zip waiting in the Import folder, its Edits into the free slots of `slots` and
 * its MP3 into the Memory Stick's MUSIC. Gives back how many Edits came in. */
int edit_share_import(const psp_save_slot *slots, int count, wchar_t *message, int cap);

#endif

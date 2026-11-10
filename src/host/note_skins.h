#ifndef DIVA_NOTE_SKINS_H
#define DIVA_NOTE_SKINS_H

/* Note skins from the mods folder, offered in the Options' Notes row after the game's own styles.
 * Each is a folder of pictures: mods\notes\<name>\circle.png, cross.png, square.png, triangle.png,
 * and targets.png if it brings targets too. Folders starting with '_' are left out. */

/* Reads them into the renderer's format under textures_hd\notes\mods. Returns how many there are. */
int note_skins_scan(const char *mods_dir, const char *data_dir);

int note_skins_count(void);
/* Its folder's name, as shown in the menu and kept in OPTIONS.INI. */
const char *note_skins_name(int skin);
/* Where its pictures are, for psp_render_gl_set_note_dir. */
const char *note_skins_folder(int skin);
/* -1 if there is none of that name. */
int note_skins_find(const char *name);

#endif

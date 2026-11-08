/* Screenshots the game takes with the system's screenshot dialog, kept on the player's computer. */
#ifndef DIVA_SCREENSHOT_H
#define DIVA_SCREENSHOT_H

/* Saves the next frame shown, at the size it is drawn, as a PNG in the player's Pictures folder,
 * under "Project DIVA Recomp". Any thread; the file is written on a thread of its own. */
void screenshot_take(void);

#endif

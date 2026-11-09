#ifndef DIVA_CONTROLS_H
#define DIVA_CONTROLS_H

#include <stdint.h>

/* Puts the player's buttons in place. `disc` is the folder holding PSP_GAME, for the screens. */
void controls_start(const char *disc);

/* The screens. The game is held while they show. */
void controls_ask_confirm(void);
void controls_change_buttons(void);

/* After the player has chosen which button confirms. */
void controls_apply(void);
/* After the player has chosen the style of the button signs: the pictures and the rows follow. */
void controls_style_changed(void);

/* Nonzero if the hook was this file's. */
int controls_hook(uint32_t addr);

#endif

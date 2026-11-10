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

/* What the screens over the game read, as they read it: the confirm button as the player chose it.
 * Whether the last thing pressed was on a controller, and the disc's folder, for kit_open. */
typedef struct {
    int up, down, left, right, ok, back;
} controls_input;
controls_input controls_screen_input(void);
int controls_screen_from_pad(void);
const char *controls_disc(void);

/* Nonzero if the hook was this file's. */
int controls_hook(uint32_t addr);

#endif

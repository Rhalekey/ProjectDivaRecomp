#ifndef DIVA_SCREEN_KIT_H
#define DIVA_SCREEN_KIT_H

/* What the screens shown over the game have in common, from language_screen.c: a canvas of
 * this size with the game's logo and background, in a window over the game's, which is held
 * for as long as one shows. */
#include <stdint.h>
#include <wchar.h>

#define KIT_CANVAS_W 960
#define KIT_CANVAS_H 544
/* Below the band with the logo, and above the band at the bottom. */
#define KIT_TOP 112
#define KIT_BOTTOM 496

/* Zero if there is no room or no window for it. Only one can be open. */
int kit_open(const char *disc);
void kit_close(void);

/* Back to the background alone, ready to draw on. */
void kit_clear(void);
void kit_row(int x, int y, int w, int h, int radius, int on);
void kit_text(int x, int y, int height, int bold, uint32_t rgb, const wchar_t *s, int centred);

/* Once a frame: keeps the window alive and puts the canvas on it. */
void kit_frame(void);

/* Waits for every button and key to be let go, so that a press that ended a screen is not
 * taken by what comes next. */
void kit_wait_for_release(void);

#endif

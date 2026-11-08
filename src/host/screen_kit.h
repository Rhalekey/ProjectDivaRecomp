#ifndef DIVA_SCREEN_KIT_H
#define DIVA_SCREEN_KIT_H

/* What the screens shown over the game have in common: a dark canvas in the game's teal, with
 * a title bar at the top, the game's logo in it, and a strip for the buttons at the bottom, in a
 * window over the game's, which is held for as long as one shows.
 *
 * Everything is placed in units of a 960 x 544 canvas, and drawn at twice that, so that it stays
 * sharp in a large window. Text is placed by the middle of its line. */
#include <stdint.h>
#include <wchar.h>

#define KIT_CANVAS_W 960
#define KIT_CANVAS_H 544
/* Below the title bar, and above the strip at the bottom. */
#define KIT_TOP 64
#define KIT_BOTTOM 492

/* The colours, as 0xRRGGBB. */
#define KIT_ACCENT 0x39C5BB   /* the game's teal */
#define KIT_TEXT 0xECF5F7
#define KIT_SOFT 0x96AAB2
#define KIT_DIM 0x566870
#define KIT_INK 0x0A141A      /* text on the accent */

enum { KIT_LEFT, KIT_CENTRE, KIT_RIGHT };
enum { KIT_GLYPH_CIRCLE, KIT_GLYPH_CROSS, KIT_GLYPH_DPAD, KIT_GLYPH_SQUARE, KIT_GLYPH_TRIANGLE };

/* Zero if there is no room or no window for it. Only one can be open. */
int kit_open(const char *disc);
void kit_close(void);

/* Back to the background alone, with `title` in the title bar, ready to draw on. */
void kit_clear(const wchar_t *title);

/* A dark, see-through panel that holds a list or a card. */
void kit_panel(int x, int y, int w, int h);
/* The row in use: a solid bar of the accent, with a soft light around it. */
void kit_selected(int x, int y, int w, int h);
/* A faint bar of the accent, for the row in use when only a part of it is selected. */
void kit_faint(int x, int y, int w, int h);
/* A rounded outline of the accent, for something waiting on the player. */
void kit_waiting(int x, int y, int w, int h);
/* A thin line between rows. */
void kit_rule(int x, int y, int w);
/* The short accent mark that starts a card or a heading. */
void kit_mark(int x, int y);

/* `height` is the size of the letters. Spaced letters for headings in capitals. */
void kit_text(int x, int y, int height, int bold, uint32_t rgb, const wchar_t *s, int align);
void kit_spaced(int x, int y, int height, uint32_t rgb, const wchar_t *s);
int kit_text_width(int height, int bold, const wchar_t *s);

/* A round flag of `radius`; `fn` gives the colour at x and y from -1 to 1 across it. */
typedef void (*kit_flag_fn)(double x, double y, double *r, double *g, double *b);
void kit_flag(int cx, int cy, int radius, kit_flag_fn fn);
/* A button's sign in a ring, `radius` across. */
void kit_glyph(int cx, int cy, int radius, int glyph, uint32_t ring, uint32_t sign);
/* Light from the accent fading out from a point, behind something large. */
void kit_halo(int cx, int cy, int radius);

/* The buttons that can be pressed, placed from the right edge of the bottom strip leftwards: each
 * call returns where the next one ends. A key is named in a key cap. */
int kit_hint(int right, int glyph, const wchar_t *word);
int kit_hint_key(int right, const wchar_t *key, const wchar_t *word);
/* A line in the bottom strip, from the right edge, in the accent. */
void kit_hint_note(const wchar_t *s);

/* Once a frame: keeps the window alive and puts the canvas on it. */
void kit_frame(void);

/* Waits for every button and key to be let go, so that a press that ended a screen is not
 * taken by what comes next. */
void kit_wait_for_release(void);

#endif

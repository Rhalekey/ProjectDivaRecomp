/* Each letter as wide as it is.
 *
 * The game gives every half-width character the same room, which suits Japanese and leaves
 * Latin letters too far apart or too tight. It writes six vertices for each character, and
 * once those are written their positions are changed here, so nothing else about the drawing
 * differs: no more draws, and the same text state.
 *
 * Tools\hd_font.py stretches each half-width letter to fill its cell, and measures it. The
 * quad is then made as wide as the letter really is, which brings it back to its own shape.
 *
 * Digits keep the width of a cell, so that columns of numbers still line up, and so do spaces
 * next to a digit or another space, because the game leaves gaps of spaces for numbers it
 * draws over them. Text with no half-width character at all is left as the game placed it.
 *
 * The game places text by the room it thinks it takes, so text may not take more unless its
 * position has been worked out again: capitals, which are wider than a cell, are narrowed
 * together to fit. Text the game presses together with a negative spacing is on a button or the
 * like, and goes in the middle of its room.
 */

#include "letters.h"

#include <psprecomp/cpu.h>
#include <psprecomp/mem.h>

#include <stdio.h>
#include <string.h>

#define RUN_START 0x0883EC34u
#define GLYPH_DONE 0x0883F104u

/* What each font size's characters measure: two 32-bit values per size. */
#define FONT_SIZES 0x08B92314u

enum { REG_A2 = 6, REG_S4 = 20, REG_SP = 29, REG_FP = 30 };

/* In the frame of the function at RUN_START. */
#define FRAME_RUN 64u
#define FRAME_STRIDE 216u

/* hd_font.py's numbers: its files are four times the game's pixels, and the first page of a
 * font, which has the half-width letters, twice that again across. */
#define FILE_SCALE 4.0f
#define CELL_ACROSS 8
#define CELL_MARGIN 2
#define VERTICES 6
/* Between a digit and the next, in the game's pixels. */
#define FIXED_GAP 0.5f

enum { FONT_LARGE, FONT_SMALL, FONT_COUNT };
static const int HALF_WIDTH[FONT_COUNT] = {8, 6};

typedef struct {
    float advance, bearing, ink;
} measure;

static measure g_measure[FONT_COUNT][256];
static int g_loaded;

static int g_on;
static uint32_t g_text;
static int g_font;
static float g_half, g_spacing;
static int g_grid;
static float g_fit, g_start;
static float g_pen;
static int g_run_at;

static int font_of(uint32_t size) {
    const uint32_t half = psp_read32(FONT_SIZES + size * 8) / 2;
    for (int i = 0; i < FONT_COUNT; i++) {
        if ((uint32_t)HALF_WIDTH[i] == half) return i;
    }
    return -1;
}

void letters_load(const char *dir) {
    char path[1200];
    snprintf(path, sizeof path, "%s\\letter_widths.txt", dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    int half, byte;
    float advance, bearing, ink;
    while (fscanf(f, "%d %d %f %f %f", &half, &byte, &advance, &bearing, &ink) == 5) {
        for (int i = 0; i < FONT_COUNT; i++) {
            if (HALF_WIDTH[i] != half || byte < 0 || byte > 255) continue;
            g_measure[i][byte] = (measure){advance / FILE_SCALE, bearing / FILE_SCALE, ink / FILE_SCALE};
            g_loaded = 1;
        }
    }
    fclose(f);
    fprintf(stderr, "letters: %s\n", g_loaded ? "measured" : "nothing in the file");
}

static int two_bytes(uint8_t c) {
    return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static float advance_of(const char *text, size_t at, int font, float half) {
    const measure *m = &g_measure[font][(uint8_t)text[at]];
    if (text[at] == ' ') {
        const int alone = at > 0 && text[at + 1] && text[at - 1] != ' ' && text[at + 1] != ' ' && !is_digit(text[at - 1]) && !is_digit(text[at + 1]);
        return alone && m->advance > 0 ? m->advance : half;
    }
    return m->advance > 0 ? m->advance : half;
}

/* Where the pen is after `count` bytes of the text. */
static float pen_after(const char *text, size_t count, int font, float half, float spacing) {
    float pen = 0;
    for (size_t i = 0; i < count && text[i]; i++) {
        if (two_bytes((uint8_t)text[i]) && text[i + 1]) {
            pen += half * 2 + spacing;
            i++;
        } else {
            pen += advance_of(text, i, font, half) + spacing;
        }
    }
    return pen;
}

/* Words were pressed together to fit Japanese, which these no longer need. */
static float wider_only(int32_t spacing) {
    return spacing > 0 ? (float)spacing : 0.0f;
}

static int has_half_width(const char *text) {
    for (const char *p = text; *p; p++) {
        if (two_bytes((uint8_t)*p) && p[1]) p++;
        else if (*p != ' ') return 1;
    }
    return 0;
}

/* A two-byte character takes two of the game's cells, and any other one. */
static float cells_of(const char *text, float half, float spacing) {
    float room = 0;
    for (const char *p = text; *p; p++) {
        if (two_bytes((uint8_t)*p) && p[1]) {
            room += half;
            p++;
        }
        room += half + spacing;
    }
    return room;
}

float letters_room(const char *text, uint32_t size, int32_t spacing) {
    return cells_of(text, (float)psp_read32(FONT_SIZES + size * 8) / 2.0f, (float)spacing);
}

float letters_width(const char *text, uint32_t size, int32_t spacing) {
    const float half = (float)psp_read32(FONT_SIZES + size * 8) / 2.0f;
    const int font = g_loaded ? font_of(size) : -1;
    const float room = letters_room(text, size, spacing);
    if (font < 0 || !has_half_width(text)) return room;
    return pen_after(text, strlen(text), font, half, wider_only(spacing));
}

float letters_width_small(const char *text) {
    const float half = (float)HALF_WIDTH[FONT_SMALL];
    if (g_loaded && has_half_width(text)) return pen_after(text, strlen(text), FONT_SMALL, half, 0.0f);
    return cells_of(text, half, 0.0f);
}

void letters_begin(uint32_t text, uint32_t size, int32_t spacing, int grid, int placed) {
    g_on = 0;
    if (!g_loaded) return;
    const char *s = (const char *)psp_mem_ptr(text, 1);
    g_font = font_of(size);
    if (!s || g_font < 0 || !has_half_width(s)) return;
    g_on = 1;
    g_text = text;
    g_half = (float)HALF_WIDTH[g_font];
    g_spacing = wider_only(spacing);
    /* A letter on its own is being placed by whoever asks, one at a time. */
    g_grid = grid || strlen(s) == 1;
    g_fit = 1.0f;
    g_start = 0.0f;
    if (!g_grid) {
        const float room = letters_room(s, size, spacing);
        const float natural = pen_after(s, strlen(s), g_font, g_half, g_spacing);
        if (!placed) {
            if (natural > room && natural > 0) g_fit = room / natural;
            else if (spacing < 0) g_start = (room - natural) / 2.0f;
        }
    }
}

static void write_float(uint32_t addr, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    psp_write32(addr, bits);
}

/* Over the six vertices just written: left left right right left right. */
static void place_quad(float left, float right) {
    static const int IS_RIGHT[VERTICES] = {0, 0, 1, 1, 0, 1};
    const uint32_t end = psp_cpu.r[REG_FP], stride = psp_read32(psp_cpu.r[REG_SP] + FRAME_STRIDE);
    for (uint32_t i = 0; i < VERTICES; i++) {
        write_float(end - (VERTICES - i) * stride, IS_RIGHT[i] ? right : left);
    }
}

static void on_run_start(void) {
    const char *s = (const char *)psp_mem_ptr(g_text, 1);
    const size_t bytes = psp_cpu.r[REG_A2];
    g_pen = g_grid ? (float)bytes * g_half : g_start + pen_after(s, bytes, g_font, g_half, g_spacing) * g_fit;
    g_run_at = 0;
}

static void on_glyph_done(void) {
    const int consumed = (int)psp_cpu.r[REG_S4];
    const int bytes = consumed - g_run_at;
    const uint32_t run = psp_read32(psp_cpu.r[REG_SP] + FRAME_RUN);
    const uint8_t first = psp_read8(run + (uint32_t)g_run_at);
    g_run_at = consumed;
    if (bytes != 1) {
        place_quad(g_pen, g_pen + g_half * 2.0f * g_fit);
        g_pen += (g_half * 2.0f + g_spacing) * g_fit;
        return;
    }
    const measure *m = &g_measure[g_font][first];
    const int in_cell = g_grid || is_digit((char)first);
    const float room = (in_cell || m->advance <= 0 ? g_half : m->advance) * g_fit;
    if (m->ink <= 0) {
        /* The game's own letter, which is the size of the cell. */
        place_quad(g_pen, g_pen + g_half * g_fit);
    } else {
        float ink = m->ink * g_fit;
        if (in_cell && ink > room - FIXED_GAP) ink = room - FIXED_GAP;
        const float ink_left = g_pen + (in_cell ? (room - ink) / 2.0f : m->bearing * g_fit);
        const float atlas_w = g_half * CELL_ACROSS;
        const float per_pixel = ink / (atlas_w - 2 * CELL_MARGIN);
        const float left = ink_left - per_pixel * CELL_MARGIN;
        place_quad(left, left + per_pixel * atlas_w);
    }
    g_pen += room + g_spacing * g_fit;
}

int letters_hook(uint32_t addr) {
    if (addr == RUN_START) {
        if (g_on) on_run_start();
        return 1;
    }
    if (addr == GLYPH_DONE) {
        if (g_on) on_glyph_done();
        return 1;
    }
    return 0;
}

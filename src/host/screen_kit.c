/* The screens shown over the game: see screen_kit.h.
 *
 * The look is a plain modern one in the game's colours: a dark teal ground, one solid bar for
 * the row in use, cards for what a row means, and the buttons in a strip at the bottom. The logo
 * is the game's, read from its archive; with no logo the game's name is written instead. The
 * libraries for it are looked up when needed, so a system without one still gets the screen.
 */

#include "screen_kit.h"

#include <psprecomp/window.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define COBJMACROS
#include <windows.h>

/* Canvas pixels for each unit of the layout. */
#define S 2
#define CW (KIT_CANVAS_W * S)
#define CH (KIT_CANVAS_H * S)

#define LOGO_PICTURE "title.png"
#define LOGO_X 0
#define LOGO_Y 64
#define LOGO_W 262
#define LOGO_H 104
#define LOGO_SHOWN_H 46

#define BAR_H 56
#define STRIP_TOP 496
#define TITLE_X 40
#define SUBSAMPLES 3
#define FONT L"Yu Gothic UI"

typedef struct {
    double r, g, b;
} colour;

static colour rgb(uint32_t c) {
    return (colour){(double)(c >> 16 & 255), (double)(c >> 8 & 255), (double)(c & 255)};
}

static const colour INK_GROUND_TOP = {13, 34, 42}, INK_GROUND_BOTTOM = {6, 14, 19}, PANEL = {12, 24, 31};
static const colour ACCENT_DEEP = {34, 150, 150};

/* ---- the game's logo ----------------------------------------------------- */

typedef struct {
    UINT32 version;
    void *debug;
    BOOL no_thread;
    BOOL no_codecs;
} gdiplus_start;

typedef struct {
    INT x, y, w, h;
} gdiplus_rect;

typedef struct {
    UINT w, h;
    INT stride;
    INT format;
    void *pixels;
    UINT_PTR reserved;
} gdiplus_data;

#define PREMULTIPLIED_BGRA 0x000E200B
#define LOCK_READ 1

typedef BOOL(WINAPI *alpha_blend_fn)(HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION);

static alpha_blend_fn g_alpha_blend;

/* A named file from the game's main archive, in memory. */
static uint8_t *read_from_archive(const char *disc, const char *name, uint32_t *size) {
    char path[1200];
    snprintf(path, sizeof path, "%s\\PSP_GAME\\USRDIR\\media\\afs\\DivaDataList.afs", disc);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t *found = NULL;
    uint32_t head[2];
    if (fread(head, 4, 2, f) == 2 && !memcmp(head, "AFS", 4)) {
        const uint32_t count = head[1];
        uint32_t *table = (uint32_t *)malloc((size_t)count * 8 + 8);
        char *names = (char *)malloc((size_t)count * 48);
        if (table && names && fread(table, 8, count + 1, f) == count + 1) {
            fseek(f, (long)table[count * 2], SEEK_SET);
            if (fread(names, 48, count, f) == count) {
                for (uint32_t i = 0; i < count && !found; i++) {
                    if (strncmp(names + i * 48, name, 32) != 0) continue;
                    *size = table[i * 2 + 1];
                    found = (uint8_t *)malloc(*size);
                    fseek(f, (long)table[i * 2], SEEK_SET);
                    if (found && fread(found, 1, *size, f) != *size) {
                        free(found);
                        found = NULL;
                    }
                }
            }
        }
        free(table);
        free(names);
    }
    fclose(f);
    return found;
}

static HDC load_picture(const char *disc, const char *name) {
    HMODULE gdiplus = LoadLibraryA("gdiplus.dll");
    HMODULE shlwapi = LoadLibraryA("shlwapi.dll");
    HMODULE msimg = LoadLibraryA("msimg32.dll");
    if (!gdiplus || !shlwapi || !msimg) return NULL;

    int(WINAPI *startup)(ULONG_PTR *, const gdiplus_start *, void *) = (void *)GetProcAddress(gdiplus, "GdiplusStartup");
    int(WINAPI *from_stream)(IStream *, void **) = (void *)GetProcAddress(gdiplus, "GdipCreateBitmapFromStream");
    int(WINAPI *get_w)(void *, UINT *) = (void *)GetProcAddress(gdiplus, "GdipGetImageWidth");
    int(WINAPI *get_h)(void *, UINT *) = (void *)GetProcAddress(gdiplus, "GdipGetImageHeight");
    int(WINAPI *lock)(void *, const gdiplus_rect *, UINT, INT, gdiplus_data *) =
        (void *)GetProcAddress(gdiplus, "GdipBitmapLockBits");
    int(WINAPI *unlock)(void *, gdiplus_data *) = (void *)GetProcAddress(gdiplus, "GdipBitmapUnlockBits");
    int(WINAPI *dispose)(void *) = (void *)GetProcAddress(gdiplus, "GdipDisposeImage");
    IStream *(WINAPI *mem_stream)(const BYTE *, UINT) = (void *)GetProcAddress(shlwapi, "SHCreateMemStream");
    g_alpha_blend = (alpha_blend_fn)GetProcAddress(msimg, "AlphaBlend");
    if (!startup || !from_stream || !get_w || !get_h || !lock || !unlock || !dispose || !mem_stream || !g_alpha_blend) {
        return NULL;
    }

    uint32_t size = 0;
    uint8_t *png = read_from_archive(disc, name, &size);
    if (!png) return NULL;

    HDC picture = NULL;
    ULONG_PTR token;
    const gdiplus_start start = {1, NULL, FALSE, FALSE};
    IStream *stream = mem_stream(png, size);
    void *bitmap = NULL;
    if (stream && startup(&token, &start, NULL) == 0 && from_stream(stream, &bitmap) == 0) {
        UINT w = 0, h = 0;
        get_w(bitmap, &w);
        get_h(bitmap, &h);
        const gdiplus_rect all = {0, 0, (INT)w, (INT)h};
        gdiplus_data data;
        if (lock(bitmap, &all, LOCK_READ, PREMULTIPLIED_BGRA, &data) == 0) {
            BITMAPINFO bi;
            ZeroMemory(&bi, sizeof bi);
            bi.bmiHeader.biSize = sizeof bi.bmiHeader;
            bi.bmiHeader.biWidth = (LONG)w;
            bi.bmiHeader.biHeight = -(LONG)h;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            void *pixels = NULL;
            HBITMAP dib = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pixels, NULL, 0);
            if (dib) {
                for (UINT y = 0; y < h; y++) {
                    memcpy((uint8_t *)pixels + y * w * 4, (const uint8_t *)data.pixels + (INT)y * data.stride, w * 4);
                }
                picture = CreateCompatibleDC(NULL);
                SelectObject(picture, dib);
            }
            unlock(bitmap, &data);
        }
        dispose(bitmap);
    }
    if (stream) IStream_Release(stream);
    free(png);
    return picture;
}

/* ---- drawing, in canvas pixels ------------------------------------------- */

static uint32_t *g_px;     /* the canvas */
static uint32_t *g_base;   /* the same with what never changes, and nothing else */
static HDC g_canvas;
static HDC g_logo;

static void blend(int x, int y, colour c, double a) {
    if (x < 0 || y < 0 || x >= CW || y >= CH || a <= 0) return;
    if (a > 1) a = 1;
    const uint32_t p = g_px[y * CW + x];
    const double pr = (p >> 16) & 255, pg = (p >> 8) & 255, pb = p & 255;
    const int nr = (int)(pr + (c.r - pr) * a + 0.5), ng = (int)(pg + (c.g - pg) * a + 0.5), nb = (int)(pb + (c.b - pb) * a + 0.5);
    g_px[y * CW + x] = (uint32_t)nr << 16 | (uint32_t)ng << 8 | (uint32_t)nb;
}

static double coverage(double distance) {
    const double c = 0.5 - distance;
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

/* How far a point is outside a rounded rectangle; less than zero is inside. */
static double rounded_distance(double px, double py, double x, double y, double w, double h, double radius) {
    const double qx = fabs(px - (x + w / 2)) - (w / 2 - radius), qy = fabs(py - (y + h / 2)) - (h / 2 - radius);
    const double ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    const double inside = qx > qy ? qx : qy;
    return sqrt(ox * ox + oy * oy) + (inside < 0 ? inside : 0) - radius;
}

/* From one colour at the left to another at the right. */
static void fill_rounded(double x, double y, double w, double h, double radius, colour left, colour right, double alpha) {
    for (int py = (int)y - 1; py <= (int)(y + h) + 1; py++) {
        for (int px = (int)x - 1; px <= (int)(x + w) + 1; px++) {
            const double a = coverage(rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius)) * alpha;
            if (a <= 0) continue;
            double t = (px + 0.5 - x) / w;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const colour c = {left.r + (right.r - left.r) * t, left.g + (right.g - left.g) * t, left.b + (right.b - left.b) * t};
            blend(px, py, c, a);
        }
    }
}

static void outline_rounded(double x, double y, double w, double h, double radius, double width, colour c, double alpha) {
    const int reach = (int)width + 2;
    for (int py = (int)y - reach; py <= (int)(y + h) + reach; py++) {
        for (int px = (int)x - reach; px <= (int)(x + w) + reach; px++) {
            const double d = fabs(rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius)) - width / 2;
            blend(px, py, c, coverage(d) * alpha);
        }
    }
}

/* Light around a rounded rectangle, fading out over `reach`. */
static void glow_rounded(double x, double y, double w, double h, double radius, double reach, colour c, double alpha) {
    for (int py = (int)(y - reach); py <= (int)(y + h + reach); py++) {
        for (int px = (int)(x - reach); px <= (int)(x + w + reach); px++) {
            const double d = rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius);
            if (d <= 0 || d >= reach) continue;
            const double t = 1 - d / reach;
            blend(px, py, c, t * t * alpha);
        }
    }
}

static void ring(double cx, double cy, double radius, double width, colour c, double alpha) {
    const int reach = (int)(radius + width + 2);
    for (int py = (int)cy - reach; py <= (int)cy + reach; py++) {
        for (int px = (int)cx - reach; px <= (int)cx + reach; px++) {
            const double qx = px + 0.5 - cx, qy = py + 0.5 - cy;
            blend(px, py, c, coverage(fabs(sqrt(qx * qx + qy * qy) - radius) - width / 2) * alpha);
        }
    }
}

static void disc(double cx, double cy, double radius, colour c, double alpha) {
    const int reach = (int)radius + 2;
    for (int py = (int)cy - reach; py <= (int)cy + reach; py++) {
        for (int px = (int)cx - reach; px <= (int)cx + reach; px++) {
            const double qx = px + 0.5 - cx, qy = py + 0.5 - cy;
            blend(px, py, c, coverage(sqrt(qx * qx + qy * qy) - radius) * alpha);
        }
    }
}

static void segment(double ax, double ay, double bx, double by, double width, colour c, double alpha) {
    const int x0 = (int)(min(ax, bx) - width - 1), x1 = (int)(max(ax, bx) + width + 1);
    const int y0 = (int)(min(ay, by) - width - 1), y1 = (int)(max(ay, by) + width + 1);
    const double dx = bx - ax, dy = by - ay, len = dx * dx + dy * dy;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double t = len > 0 ? ((px + 0.5 - ax) * dx + (py + 0.5 - ay) * dy) / len : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const double qx = ax + t * dx - (px + 0.5), qy = ay + t * dy - (py + 0.5);
            blend(px, py, c, coverage(sqrt(qx * qx + qy * qy) - width / 2) * alpha);
        }
    }
}

static HFONT make_font(int height, int bold) {
    return CreateFontW(-height * S, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, FONT);
}

/* In canvas pixels: `y` is the middle of the line's capitals. */
static void write(int x, int y, int height, int bold, int spacing, uint32_t c, const wchar_t *s, int align) {
    HFONT font = make_font(height, bold);
    HGDIOBJ old = SelectObject(g_canvas, font);
    SetTextColor(g_canvas, RGB(c >> 16 & 255, c >> 8 & 255, c & 255));
    SetTextCharacterExtra(g_canvas, spacing);
    const int length = (int)wcslen(s);
    SIZE size;
    GetTextExtentPoint32W(g_canvas, s, length, &size);
    if (align == KIT_CENTRE) x -= size.cx / 2;
    else if (align == KIT_RIGHT) x -= size.cx;
    TEXTMETRICW tm;
    GetTextMetricsW(g_canvas, &tm);
    const int capitals = tm.tmAscent - tm.tmInternalLeading;
    TextOutW(g_canvas, x, y - tm.tmInternalLeading - capitals * 11 / 20, s, length);
    SetTextCharacterExtra(g_canvas, 0);
    SelectObject(g_canvas, old);
    DeleteObject(font);
}

static int measure(int height, int bold, const wchar_t *s) {
    HFONT font = make_font(height, bold);
    HGDIOBJ old = SelectObject(g_canvas, font);
    SIZE size;
    GetTextExtentPoint32W(g_canvas, s, (int)wcslen(s), &size);
    SelectObject(g_canvas, old);
    DeleteObject(font);
    return (size.cx + S - 1) / S;
}

/* What stays the same: the ground, the title bar with the logo, and the strip at the bottom. */
static void draw_base(void) {
    for (int y = 0; y < CH; y++) {
        const double t = (double)y / CH;
        const uint32_t r = (uint32_t)(INK_GROUND_TOP.r + (INK_GROUND_BOTTOM.r - INK_GROUND_TOP.r) * t);
        const uint32_t g = (uint32_t)(INK_GROUND_TOP.g + (INK_GROUND_BOTTOM.g - INK_GROUND_TOP.g) * t);
        const uint32_t b = (uint32_t)(INK_GROUND_TOP.b + (INK_GROUND_BOTTOM.b - INK_GROUND_TOP.b) * t);
        for (int x = 0; x < CW; x++) g_px[y * CW + x] = r << 16 | g << 8 | b;
    }
    /* A wide soft light of the accent, off to the right, where the eye goes first. */
    const colour accent = rgb(KIT_ACCENT);
    for (int y = 0; y < CH; y++) {
        for (int x = 0; x < CW; x++) {
            const double dx = (x - 660.0 * S) / (330.0 * S), dy = (y - 250.0 * S) / (250.0 * S);
            const double d = 1 - sqrt(dx * dx + dy * dy);
            if (d > 0) blend(x, y, accent, d * d * 0.20);
        }
    }

    /* The title bar, a little lighter at its top, and a line of the accent fading to the right. */
    for (int y = 0; y < BAR_H * S; y++) {
        const double a = 0.80 * (1 - (double)y / (BAR_H * S) * 0.25);
        for (int x = 0; x < CW; x++) blend(x, y, PANEL, a);
    }
    for (int x = 0; x < CW; x++) {
        const double fade = 1 - (double)x / (CW * 0.88);
        if (fade > 0) {
            for (int y = BAR_H * S; y < BAR_H * S + S; y++) blend(x, y, accent, fade * 0.75);
        }
    }

    /* The strip for the buttons, with a faint line over it. */
    const colour white = rgb(KIT_TEXT);
    for (int y = STRIP_TOP * S; y < CH; y++) {
        for (int x = 0; x < CW; x++) blend(x, y, PANEL, 0.88);
    }
    for (int x = 0; x < CW; x++) blend(x, STRIP_TOP * S, white, 0.16);

    /* The game's logo at the right of the title bar. */
    const int shown_h = LOGO_SHOWN_H * S, shown_w = LOGO_W * shown_h / LOGO_H;
    if (g_logo && g_alpha_blend) {
        const BLENDFUNCTION blending = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        SetStretchBltMode(g_canvas, HALFTONE);
        SetBrushOrgEx(g_canvas, 0, 0, NULL);
        g_alpha_blend(g_canvas, CW - 24 * S - shown_w, (BAR_H * S - shown_h) / 2, shown_w, shown_h, g_logo, LOGO_X, LOGO_Y, LOGO_W,
                      LOGO_H, blending);
    } else {
        write(CW - 24 * S, BAR_H * S / 2, 18, 1, 0, KIT_SOFT, L"Project DIVA", KIT_RIGHT);
    }
    /* The pill that starts the title. */
    fill_rounded(24 * S, 18 * S, 3 * S, 20 * S, 1.5 * S, accent, accent, 1.0);
    memcpy(g_base, g_px, (size_t)CW * CH * 4);
}

/* ---- what the screens draw with ----------------------------------------- */

void kit_clear(const wchar_t *title) {
    memcpy(g_px, g_base, (size_t)CW * CH * 4);
    if (title) write(TITLE_X * S, BAR_H * S / 2, 24, 1, 0, KIT_TEXT, title, KIT_LEFT);
}

void kit_panel(int x, int y, int w, int h) {
    fill_rounded(x * S, y * S, w * S, h * S, 8 * S, PANEL, PANEL, 0.62);
}

void kit_selected(int x, int y, int w, int h) {
    const colour accent = rgb(KIT_ACCENT);
    glow_rounded(x * S, y * S, w * S, h * S, 6 * S, 9 * S, accent, 0.22);
    fill_rounded(x * S, y * S, w * S, h * S, 6 * S, accent, ACCENT_DEEP, 1.0);
}

void kit_faint(int x, int y, int w, int h) {
    fill_rounded(x * S, y * S, w * S, h * S, 6 * S, rgb(KIT_ACCENT), rgb(KIT_ACCENT), 0.12);
}

void kit_waiting(int x, int y, int w, int h) {
    const colour accent = rgb(KIT_ACCENT);
    glow_rounded(x * S, y * S, w * S, h * S, 6 * S, 9 * S, accent, 0.30);
    fill_rounded(x * S, y * S, w * S, h * S, 6 * S, PANEL, PANEL, 0.9);
    outline_rounded(x * S, y * S, w * S, h * S, 6 * S, 1.5 * S, accent, 1.0);
}

void kit_rule(int x, int y, int w) {
    const colour white = rgb(KIT_TEXT);
    for (int px = x * S; px < (x + w) * S; px++) blend(px, y * S, white, 0.10);
}

void kit_mark(int x, int y) {
    fill_rounded(x * S, y * S, 24 * S, 6 * S, 3 * S, rgb(KIT_ACCENT), rgb(KIT_ACCENT), 1.0);
}

void kit_text(int x, int y, int height, int bold, uint32_t c, const wchar_t *s, int align) {
    write(x * S, y * S, height, bold, 0, c, s, align);
}

void kit_spaced(int x, int y, int height, uint32_t c, const wchar_t *s) {
    write(x * S, y * S, height, 1, height * S / 4, c, s, KIT_LEFT);
}

int kit_text_width(int height, int bold, const wchar_t *s) {
    return measure(height, bold, s);
}

void kit_flag(int cx, int cy, int radius, kit_flag_fn fn) {
    const double x0 = cx * S, y0 = cy * S, r0 = radius * S;
    for (int py = (int)(y0 - r0) - 2; py <= (int)(y0 + r0) + 2; py++) {
        for (int px = (int)(x0 - r0) - 2; px <= (int)(x0 + r0) + 2; px++) {
            double r = 0, g = 0, b = 0, inside = 0;
            for (int sy = 0; sy < SUBSAMPLES; sy++) {
                for (int sx = 0; sx < SUBSAMPLES; sx++) {
                    const double x = (px + (sx + 0.5) / SUBSAMPLES - x0) / r0;
                    const double y = (py + (sy + 0.5) / SUBSAMPLES - y0) / r0;
                    if (x * x + y * y > 1) continue;
                    double fr, fg, fb;
                    fn(x, y, &fr, &fg, &fb);
                    r += fr;
                    g += fg;
                    b += fb;
                    inside += 1;
                }
            }
            if (inside <= 0) continue;
            blend(px, py, (colour){r / inside, g / inside, b / inside}, inside / (SUBSAMPLES * SUBSAMPLES));
        }
    }
    ring(x0, y0, r0 - 0.5 * S, 1.0 * S, rgb(KIT_TEXT), 0.35);
}

void kit_glyph(int cx, int cy, int radius, int glyph, uint32_t ring_rgb, uint32_t sign_rgb) {
    const double x = cx * S, y = cy * S, r = radius * S, width = radius > 20 ? 3.0 * S : 1.0 * S;
    const colour sign = rgb(sign_rgb);
    ring(x, y, r - width / 2, width, rgb(ring_rgb), 0.9);
    const double k = r * 0.42, stroke = radius > 20 ? r * 0.13 : 1.1 * S;
    switch (glyph) {
    case KIT_GLYPH_CIRCLE:
        ring(x, y, k, stroke, sign, 1.0);
        break;
    case KIT_GLYPH_CROSS:
        segment(x - k, y - k, x + k, y + k, stroke, sign, 1.0);
        segment(x + k, y - k, x - k, y + k, stroke, sign, 1.0);
        break;
    case KIT_GLYPH_SQUARE: {
        const double q = k * 0.85;
        outline_rounded(x - q, y - q, q * 2, q * 2, stroke * 0.3, stroke, sign, 1.0);
        break;
    }
    case KIT_GLYPH_TRIANGLE: {
        const double ax = x, ay = y - k * 1.0, bx = x + k * 1.0, by = y + k * 0.72, cx2 = x - k * 1.0;
        segment(ax, ay, bx, by, stroke, sign, 1.0);
        segment(bx, by, cx2, by, stroke, sign, 1.0);
        segment(cx2, by, ax, ay, stroke, sign, 1.0);
        break;
    }
    default: {
        /* Four circles, as the d-pad is drawn in the game's menus. */
        const double d = r * 0.40, h = r * 0.17;
        const double at[4][2] = {{0, -d}, {0, d}, {-d, 0}, {d, 0}};
        for (int i = 0; i < 4; i++) disc(x + at[i][0], y + at[i][1], h, sign, 1.0);
        break;
    }
    }
}

void kit_halo(int cx, int cy, int radius) {
    const colour accent = rgb(KIT_ACCENT);
    const double x0 = cx * S, y0 = cy * S, r0 = radius * S;
    for (int py = (int)(y0 - r0); py <= (int)(y0 + r0); py++) {
        for (int px = (int)(x0 - r0); px <= (int)(x0 + r0); px++) {
            const double d = 1 - sqrt((px - x0) * (px - x0) + (py - y0) * (py - y0)) / r0;
            if (d > 0) blend(px, py, accent, d * d * 0.30);
        }
    }
}

#define HINT_Y 518
#define HINT_TEXT 15
#define HINT_GAP 26

int kit_hint(int right, int glyph, const wchar_t *word) {
    const int w = kit_text_width(HINT_TEXT, 0, word);
    kit_text(right, HINT_Y, HINT_TEXT, 0, KIT_TEXT, word, KIT_RIGHT);
    const int cx = right - w - 8 - 11;
    kit_glyph(cx, HINT_Y, 11, glyph, KIT_TEXT, KIT_ACCENT);
    return cx - 11 - HINT_GAP;
}

int kit_hint_key(int right, const wchar_t *key, const wchar_t *word) {
    const int w = kit_text_width(HINT_TEXT, 0, word);
    kit_text(right, HINT_Y, HINT_TEXT, 0, KIT_TEXT, word, KIT_RIGHT);
    const int kw = kit_text_width(12, 1, key) + 14, kx = right - w - 8 - kw;
    outline_rounded(kx * S, (HINT_Y - 11) * S, kw * S, 22 * S, 5 * S, 1.0 * S, rgb(KIT_TEXT), 0.9);
    kit_text(kx + kw / 2, HINT_Y, 12, 1, KIT_ACCENT, key, KIT_CENTRE);
    return kx - HINT_GAP;
}

void kit_hint_note(const wchar_t *s) {
    kit_text(KIT_CANVAS_W - 32, HINT_Y, HINT_TEXT, 1, KIT_ACCENT, s, KIT_RIGHT);
}

/* ---- the window ---------------------------------------------------------- */

static LRESULT CALLBACK screen_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static void pump(void) {
    MSG m;
    while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

/* The canvas on the window, keeping its shape. */
static void show(HWND window) {
    RECT client;
    GetClientRect(window, &client);
    const int w = client.right, h = client.bottom;
    if (w <= 0 || h <= 0) return;
    int shown_w = w, shown_h = w * CH / CW;
    if (shown_h > h) {
        shown_h = h;
        shown_w = h * CW / CH;
    }
    const int x = (w - shown_w) / 2, y = (h - shown_h) / 2;
    HDC dc = GetDC(window);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, NULL);
    /* What the shape leaves over continues the canvas's own edges, so the window has no bars. */
    if (y > 0) {
        StretchBlt(dc, 0, 0, w, y, g_canvas, 0, 0, CW, 1, SRCCOPY);
        StretchBlt(dc, 0, y + shown_h, w, h - y - shown_h, g_canvas, 0, CH - 1, CW, 1, SRCCOPY);
    }
    if (x > 0) {
        StretchBlt(dc, 0, 0, x, h, g_canvas, 0, 0, 1, CH, SRCCOPY);
        StretchBlt(dc, x + shown_w, 0, w - x - shown_w, h, g_canvas, CW - 1, 0, 1, CH, SRCCOPY);
    }
    StretchBlt(dc, x, y, shown_w, shown_h, g_canvas, 0, 0, CW, CH, SRCCOPY);
    ReleaseDC(window, dc);
}

static HWND g_window, g_parent;
static HBITMAP g_bitmap;
static HGDIOBJ g_old_bitmap;
static RECT g_last_client;
static int g_frames;
static uint32_t g_shown_sum;

int kit_open(const char *disc) {
    g_parent = (HWND)psp_window_handle();
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = CW;
    bi.bmiHeader.biHeight = -CH;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pixels, NULL, 0);
    g_base = (uint32_t *)malloc((size_t)CW * CH * 4);
    if (!bitmap || !g_base) {
        fprintf(stderr, "screen: no memory for it\n");
        if (bitmap) DeleteObject(bitmap);
        free(g_base);
        g_base = NULL;
        return 0;
    }
    g_bitmap = bitmap;
    g_px = (uint32_t *)pixels;
    g_canvas = CreateCompatibleDC(NULL);
    g_old_bitmap = SelectObject(g_canvas, bitmap);
    SetBkMode(g_canvas, TRANSPARENT);

    if (!g_logo) g_logo = load_picture(disc, LOGO_PICTURE);
    if (!g_logo) fprintf(stderr, "screen: the game's logo could not be read\n");
    draw_base();

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = screen_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"diva_screen";
    RegisterClassW(&wc);
    /* Over the game's window, which must not draw over this in turn. */
    SetWindowLongPtrW(g_parent, GWL_STYLE, GetWindowLongPtrW(g_parent, GWL_STYLE) | WS_CLIPCHILDREN);
    GetClientRect(g_parent, &g_last_client);
    g_window = CreateWindowW(wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE, 0, 0, g_last_client.right, g_last_client.bottom,
                             g_parent, NULL, wc.hInstance, NULL);
    if (!g_window) {
        fprintf(stderr, "screen: no window for it\n");
        return 0;
    }
    g_frames = 0;
    g_shown_sum = 0;
    return 1;
}

void kit_close(void) {
    DestroyWindow(g_window);
    pump();
    SelectObject(g_canvas, g_old_bitmap);
    DeleteDC(g_canvas);
    DeleteObject(g_bitmap);
    free(g_base);
    g_base = NULL;
    g_px = NULL;
}

/* A cheap sum of a few rows, to know when the canvas has changed. */
static uint32_t canvas_sum(void) {
    uint32_t sum = 0;
    for (int y = 0; y < CH; y += 7) {
        const uint32_t *row = g_px + (size_t)y * CW;
        for (int x = 0; x < CW; x += 3) sum = sum * 31 + row[x];
    }
    return sum;
}

/* Windows can paint over a child of a window that is being redrawn, so it is put there again
 * every so often even when nothing has changed. */
void kit_frame(void) {
    if (psp_window_closed()) _Exit(0);
    pump();
    RECT client;
    GetClientRect(g_parent, &client);
    if (client.right != g_last_client.right || client.bottom != g_last_client.bottom) {
        MoveWindow(g_window, 0, 0, client.right, client.bottom, FALSE);
        g_last_client = client;
        g_frames = 0;
    }
    const uint32_t sum = canvas_sum();
    if (sum != g_shown_sum) {
        g_shown_sum = sum;
        g_frames = 0;
    }
    if (g_frames < 10 || g_frames % 10 == 0) show(g_window);
    g_frames++;
}

void kit_wait_for_release(void) {
    for (;;) {
        int down = psp_window_buttons() != 0 || psp_window_pad_raw() != 0;
        for (int vk = 0x08; vk <= 0xFE && !down; vk++) down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (!down || psp_window_closed()) return;
        kit_frame();
        Sleep(16);
    }
}

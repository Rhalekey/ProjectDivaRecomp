/* The question between the game's intros and its title.
 *
 * The game is held while this shows, in a window of its own over the game's. It draws for
 * itself: the logo is the game's, read from its archive, and the rest is made here. To add
 * a language, add a line to LANGUAGES and a function for its flag.
 *
 * Set language_prompt = 0 to see it only when no choice has been saved.
 *
 * The libraries are looked up when needed, so a system without one still gets the screen,
 * with the game's name in plain text rather than its logo.
 */

#include "language_screen.h"

#include "screen_kit.h"

#include <psprecomp/window.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define COBJMACROS
#include <windows.h>

enum { B_START = 0x0008, B_UP = 0x0010, B_DOWN = 0x0040, B_CIRCLE = 0x2000, B_CROSS = 0x4000 };

/* Everything is laid out on this, twice the PSP's screen, and scaled to the window. */
#define CANVAS_W 960
#define CANVAS_H 544

#define LOGO_PICTURE "title.png"
#define LOGO_X 0
#define LOGO_Y 64
#define LOGO_W 262
#define LOGO_H 104
#define LOGO_SHOWN_H 92

#define BAND_H 112
#define QUESTION_Y 122
#define QUESTION_NOTE_Y 164
#define ROW_LEFT 150
#define ROW_W 430
#define ROW_H 64
#define ROW_GAP 14
#define ROWS_TOP 214
#define ROW_RADIUS 32
#define FLAG_RADIUS 25
#define FLAG_X 39
#define NAME_X 80
#define NAME_H 31
#define BOX_LEFT 612
#define BOX_TOP 214
#define BOX_W 310
#define BOX_H 150
#define BOX_RADIUS 12
#define BOX_TEXT_H 20
#define BOX_LINE 28
#define LINE_BOTTOM 496
#define HINT_Y 515
#define SUBSAMPLES 3

typedef void (*flag_fn)(double x, double y, double *r, double *g, double *b);

static void set(double *r, double *g, double *b, int cr, int cg, int cb) {
    *r = cr;
    *g = cg;
    *b = cb;
}

/* x and y run from -1 to 1 across the flag. */
static void flag_uk(double x, double y, double *r, double *g, double *b) {
    const double ax = fabs(x), ay = fabs(y);
    set(r, g, b, 1, 33, 105);
    if (fabs(ax - ay) < 0.22) set(r, g, b, 255, 255, 255);
    if (fabs(x - y) < 0.09 || fabs(x + y) < 0.09) set(r, g, b, 200, 16, 46);
    if (ax < 0.33 || ay < 0.33) set(r, g, b, 255, 255, 255);
    if (ax < 0.2 || ay < 0.2) set(r, g, b, 200, 16, 46);
}

static void flag_japan(double x, double y, double *r, double *g, double *b) {
    if (x * x + y * y < 0.3) set(r, g, b, 188, 0, 45);
    else set(r, g, b, 245, 245, 245);
}

static void flag_germany(double x, double y, double *r, double *g, double *b) {
    (void)x;
    if (y < -1.0 / 3) set(r, g, b, 10, 10, 10);
    else if (y < 1.0 / 3) set(r, g, b, 221, 0, 0);
    else set(r, g, b, 255, 206, 0);
}

static const struct {
    const char *code;
    const wchar_t *name;
    flag_fn flag;
    const wchar_t *question;
    const wchar_t *note[4];
    const wchar_t *select;
} LANGUAGES[] = {
    {"en", L"ENGLISH", flag_uk, L"SELECT A LANGUAGE",
     {L"Menus and dialogue will", L"use this language. Songs", L"keep their Japanese audio.", L"Change it later in Options."},
     L"SELECT"},
    {"ja", L"日本語 (JAPANESE)", flag_japan, L"言語を選択してください",
     {L"メニューと会話の言語を", L"設定します。楽曲の音声は", L"日本語のままです。", L"オプションで変更できます。"},
     L"決定"},
    {"de", L"DEUTSCH", flag_germany, L"SPRACHE WÄHLEN",
     {L"Menüs und Dialoge nutzen", L"diese Sprache. Die Lieder", L"bleiben auf Japanisch.", L"Änderbar unter Optionen."},
     L"AUSWÄHLEN"},
};
#define NUM_LANGUAGES ((int)(sizeof LANGUAGES / sizeof LANGUAGES[0]))

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

/* ---- drawing ------------------------------------------------------------- */

static uint32_t *g_px;     /* the canvas */
static uint32_t *g_base;   /* the same with what never changes, and nothing else */
static HDC g_canvas;

static void blend(int x, int y, double r, double g, double b, double a) {
    if (x < 0 || y < 0 || x >= CANVAS_W || y >= CANVAS_H || a <= 0) return;
    if (a > 1) a = 1;
    const uint32_t p = g_px[y * CANVAS_W + x];
    const double pr = (p >> 16) & 255, pg = (p >> 8) & 255, pb = p & 255;
    const int nr = (int)(pr + (r - pr) * a), ng = (int)(pg + (g - pg) * a), nb = (int)(pb + (b - pb) * a);
    g_px[y * CANVAS_W + x] = (uint32_t)nr << 16 | (uint32_t)ng << 8 | (uint32_t)nb;
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

typedef struct {
    int r, g, b;
} colour;

/* From one colour at the top to another at the bottom. */
static void rounded(int x, int y, int w, int h, int radius, colour top, colour bottom, double alpha) {
    for (int py = y - 1; py <= y + h; py++) {
        const double t = (double)(py - y) / h;
        for (int px = x - 1; px <= x + w; px++) {
            const double a = coverage(rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius)) * alpha;
            blend(px, py, top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t, top.b + (bottom.b - top.b) * t, a);
        }
    }
}

static void outline(int x, int y, int w, int h, int radius, double width, colour c, double alpha) {
    for (int py = y - 2; py <= y + h + 2; py++) {
        for (int px = x - 2; px <= x + w + 2; px++) {
            const double d = fabs(rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius)) - width / 2;
            blend(px, py, c.r, c.g, c.b, coverage(d) * alpha);
        }
    }
}

/* Light around a rounded rectangle, fading out over `reach`. */
static void glow(int x, int y, int w, int h, int radius, int reach, colour c, double alpha) {
    for (int py = y - reach; py <= y + h + reach; py++) {
        for (int px = x - reach; px <= x + w + reach; px++) {
            const double d = rounded_distance(px + 0.5, py + 0.5, x, y, w, h, radius);
            if (d <= 0 || d >= reach) continue;
            const double t = 1 - d / reach;
            blend(px, py, c.r, c.g, c.b, t * t * alpha);
        }
    }
}

static void line(double ax, double ay, double bx, double by, double width, colour c, double alpha) {
    const int x0 = (int)(min(ax, bx) - width - 1), x1 = (int)(max(ax, bx) + width + 1);
    const int y0 = (int)(min(ay, by) - width - 1), y1 = (int)(max(ay, by) + width + 1);
    const double dx = bx - ax, dy = by - ay, len = dx * dx + dy * dy;
    for (int py = y0; py <= y1; py++) {
        for (int px = x0; px <= x1; px++) {
            double t = len > 0 ? ((px + 0.5 - ax) * dx + (py + 0.5 - ay) * dy) / len : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            const double qx = ax + t * dx - (px + 0.5), qy = ay + t * dy - (py + 0.5);
            blend(px, py, c.r, c.g, c.b, coverage(sqrt(qx * qx + qy * qy) - width / 2) * alpha);
        }
    }
}

static void arc_ring(double cx, double cy, double radius, double width, double from, double to, colour c, double alpha) {
    const int reach = (int)(radius + width + 1);
    for (int py = (int)cy - reach; py <= (int)cy + reach; py++) {
        for (int px = (int)cx - reach; px <= (int)cx + reach; px++) {
            const double qx = px + 0.5 - cx, qy = py + 0.5 - cy;
            double angle = atan2(qy, qx) * 180.0 / 3.14159265358979;
            if (angle < 0) angle += 360;
            if (angle < from || angle > to) continue;
            blend(px, py, c.r, c.g, c.b, coverage(fabs(sqrt(qx * qx + qy * qy) - radius) - width / 2) * alpha);
        }
    }
}

static void dot(double cx, double cy, double radius, colour c, double alpha) {
    arc_ring(cx, cy, radius / 2, radius, 0, 360, c, alpha);
}

static void flag(int cx, int cy, int radius, flag_fn fn) {
    for (int py = cy - radius - 2; py <= cy + radius + 2; py++) {
        for (int px = cx - radius - 2; px <= cx + radius + 2; px++) {
            double r = 0, g = 0, b = 0, inside = 0;
            for (int sy = 0; sy < SUBSAMPLES; sy++) {
                for (int sx = 0; sx < SUBSAMPLES; sx++) {
                    const double x = (px + (sx + 0.5) / SUBSAMPLES - cx) / radius;
                    const double y = (py + (sy + 0.5) / SUBSAMPLES - cy) / radius;
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
            blend(px, py, r / inside, g / inside, b / inside, inside / (SUBSAMPLES * SUBSAMPLES));
        }
    }
    /* Round like a button: light from above, shade below, and a rim. */
    const colour white = {255, 255, 255}, black = {0, 0, 0};
    for (int py = cy - radius; py <= cy + radius; py++) {
        for (int px = cx - radius; px <= cx + radius; px++) {
            const double x = (double)(px - cx) / radius, y = (double)(py - cy) / radius;
            if (x * x + y * y > 1) continue;
            if (y < 0) blend(px, py, white.r, white.g, white.b, -y * 0.28);
            else blend(px, py, black.r, black.g, black.b, y * 0.25);
        }
    }
    arc_ring(cx, cy, radius, 2.2, 0, 360, white, 0.75);
}

static void text(int x, int y, int height, int weight, COLORREF c, const wchar_t *s, int centred) {
    HFONT font = CreateFontW(-height, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Yu Gothic UI");
    HGDIOBJ old = SelectObject(g_canvas, font);
    SetTextColor(g_canvas, c);
    const int length = (int)wcslen(s);
    if (centred) {
        SIZE size;
        GetTextExtentPoint32W(g_canvas, s, length, &size);
        x -= size.cx / 2;
    }
    TextOutW(g_canvas, x, y, s, length);
    SelectObject(g_canvas, old);
    DeleteObject(font);
}

/* What stays the same: the background, its lines, and the game's logo. */
static void draw_base(HDC logo) {
    const colour teal = {60, 190, 210}, pale = {170, 215, 235};
    for (int y = 0; y < CANVAS_H; y++) {
        const double t = (double)y / CANVAS_H;
        const uint32_t r = (uint32_t)(5 + 6 * (1 - t)), g = (uint32_t)(40 - 18 * t), b = (uint32_t)(70 - 28 * t);
        for (int x = 0; x < CANVAS_W; x++) g_px[y * CANVAS_W + x] = r << 16 | g << 8 | b;
    }

    /* A circuit board, faint. */
    static const struct {
        double cx, cy, radius, width, from, to;
    } ARCS[] = {
        {190, 350, 110, 3, 40, 320}, {190, 350, 165, 6, 100, 290}, {190, 350, 225, 2, 60, 330},
        {190, 350, 300, 9, 120, 250}, {850, 350, 90, 3, 130, 420 - 60}, {850, 350, 150, 7, 150, 300},
        {850, 350, 215, 2, 120, 260},
    };
    for (size_t i = 0; i < sizeof ARCS / sizeof ARCS[0]; i++) {
        arc_ring(ARCS[i].cx, ARCS[i].cy, ARCS[i].radius, ARCS[i].width, ARCS[i].from, ARCS[i].to, teal, 0.10);
    }
    static const double TRACES[][4] = {
        {40, 170, 120, 170}, {120, 170, 150, 200}, {60, 460, 150, 460}, {150, 460, 185, 425},
        {30, 300, 75, 300}, {75, 300, 100, 275}, {680, 130, 790, 130}, {790, 130, 825, 165},
        {825, 165, 925, 165}, {640, 470, 760, 470}, {760, 470, 790, 440}, {790, 440, 930, 440},
    };
    for (size_t i = 0; i < sizeof TRACES / sizeof TRACES[0]; i++) {
        line(TRACES[i][0], TRACES[i][1], TRACES[i][2], TRACES[i][3], 2, teal, 0.20);
        dot(TRACES[i][0], TRACES[i][1], 4, teal, 0.28);
    }

    /* The bands at the top and bottom. */
    const colour shade = {2, 12, 24};
    rounded(-20, -20, CANVAS_W + 40, BAND_H + 20, 2, shade, shade, 0.72);
    line(0, BAND_H, CANVAS_W, BAND_H, 2, pale, 0.65);
    rounded(-20, LINE_BOTTOM, CANVAS_W + 40, CANVAS_H - LINE_BOTTOM + 20, 2, shade, shade, 0.72);
    line(0, LINE_BOTTOM, CANVAS_W, LINE_BOTTOM, 2, pale, 0.65);

    const int shown_w = LOGO_W * LOGO_SHOWN_H / LOGO_H;
    if (logo && g_alpha_blend) {
        const BLENDFUNCTION blending = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        SetStretchBltMode(g_canvas, HALFTONE);
        g_alpha_blend(g_canvas, (CANVAS_W - shown_w) / 2, (BAND_H - LOGO_SHOWN_H) / 2, shown_w, LOGO_SHOWN_H, logo, LOGO_X, LOGO_Y,
                      LOGO_W, LOGO_H, blending);
    } else {
        text(CANVAS_W / 2, 26, 50, FW_BOLD, RGB(240, 245, 250), L"Project DIVA", 1);
    }
    /* What drawing with Windows writes has no alpha, and nothing here uses it. */
    memcpy(g_base, g_px, CANVAS_W * CANVAS_H * 4);
}

static void draw_row(int x, int y, int w, int h, int radius, int on) {
    const colour top = on ? (colour){44, 112, 118} : (colour){36, 80, 118};
    const colour bottom = on ? (colour){20, 72, 84} : (colour){16, 46, 84};
    if (on) glow(x, y, w, h, radius, 13, (colour){90, 255, 180}, 0.55);
    rounded(x, y, w, h, radius, top, bottom, 1.0);
    if (on) {
        outline(x, y, w, h, radius, 4, (colour){110, 250, 185}, 1.0);
        outline(x + 5, y + 5, w - 10, h - 10, radius - 5, 1.2, (colour){225, 255, 240}, 0.65);
    } else {
        outline(x, y, w, h, radius, 2.5, (colour){115, 170, 212}, 0.95);
    }
}

static void draw_screen(int selected) {
    memcpy(g_px, g_base, CANVAS_W * CANVAS_H * 4);
    const COLORREF white = RGB(245, 248, 252), dim = RGB(195, 215, 228);
    const int language = selected;

    text(CANVAS_W / 2, QUESTION_Y, 33, FW_SEMIBOLD, white, LANGUAGES[language].question, 1);
    /* The same question in another language, for whoever cannot read this one. */
    text(CANVAS_W / 2, QUESTION_NOTE_Y, 19, FW_NORMAL, dim, LANGUAGES[language == 0 ? 1 : 0].question, 1);

    int y = ROWS_TOP;
    for (int i = 0; i < NUM_LANGUAGES; i++) {
        draw_row(ROW_LEFT, y, ROW_W, ROW_H, ROW_RADIUS, i == selected);
        flag(ROW_LEFT + FLAG_X, y + ROW_H / 2, FLAG_RADIUS, LANGUAGES[i].flag);
        text(ROW_LEFT + NAME_X, y + (ROW_H - NAME_H) / 2 - 5, NAME_H, FW_SEMIBOLD, white, LANGUAGES[i].name, 0);
        y += ROW_H + ROW_GAP;
    }

    const colour box = {6, 26, 46}, edge = {130, 180, 215};
    rounded(BOX_LEFT, BOX_TOP, BOX_W, BOX_H, BOX_RADIUS, box, box, 0.78);
    outline(BOX_LEFT, BOX_TOP, BOX_W, BOX_H, BOX_RADIUS, 1.5, edge, 0.85);
    for (int i = 0; i < 4; i++) {
        text(BOX_LEFT + 18, BOX_TOP + 16 + i * BOX_LINE, BOX_TEXT_H, FW_NORMAL, white, LANGUAGES[language].note[i], 0);
    }

    /* The hint: the game confirms with the circle button. */
    const int hint_x = CANVAS_W - 180;
    const colour red = {235, 70, 90};
    dot(hint_x, HINT_Y + 11, 13, (colour){30, 45, 70}, 1.0);
    arc_ring(hint_x, HINT_Y + 11, 7, 2.6, 0, 360, red, 1.0);
    arc_ring(hint_x, HINT_Y + 11, 13, 1.4, 0, 360, (colour){170, 195, 215}, 0.9);
    text(hint_x + 22, HINT_Y - 3, 22, FW_SEMIBOLD, white, LANGUAGES[language].select, 0);
}

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
    int shown_w = w, shown_h = w * CANVAS_H / CANVAS_W;
    if (shown_h > h) {
        shown_h = h;
        shown_w = h * CANVAS_W / CANVAS_H;
    }
    const int x = (w - shown_w) / 2, y = (h - shown_h) / 2;
    HDC dc = GetDC(window);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, NULL);
    /* What the shape leaves over continues the canvas's own edges, so the window has no bars. */
    if (y > 0) {
        StretchBlt(dc, 0, 0, w, y, g_canvas, 0, 0, CANVAS_W, 1, SRCCOPY);
        StretchBlt(dc, 0, y + shown_h, w, h - y - shown_h, g_canvas, 0, CANVAS_H - 1, CANVAS_W, 1, SRCCOPY);
    }
    if (x > 0) {
        StretchBlt(dc, 0, 0, x, h, g_canvas, 0, 0, 1, CANVAS_H, SRCCOPY);
        StretchBlt(dc, x + shown_w, 0, w - x - shown_w, h, g_canvas, CANVAS_W - 1, 0, 1, CANVAS_H, SRCCOPY);
    }
    StretchBlt(dc, x, y, shown_w, shown_h, g_canvas, 0, 0, CANVAS_W, CANVAS_H, SRCCOPY);
    ReleaseDC(window, dc);
}

static HWND g_window, g_parent;
static HBITMAP g_bitmap;
static HGDIOBJ g_old_bitmap;
static RECT g_last_client;
static int g_frames;

int kit_open(const char *disc) {
    g_parent = (HWND)psp_window_handle();
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = CANVAS_W;
    bi.bmiHeader.biHeight = -CANVAS_H;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *pixels = NULL;
    HBITMAP bitmap = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &pixels, NULL, 0);
    g_base = (uint32_t *)malloc(CANVAS_W * CANVAS_H * 4);
    if (!bitmap || !g_base) {
        fprintf(stderr, "screen: no memory for it\n");
        return 0;
    }
    g_bitmap = bitmap;
    g_px = (uint32_t *)pixels;
    g_canvas = CreateCompatibleDC(NULL);
    g_old_bitmap = SelectObject(g_canvas, bitmap);
    SetBkMode(g_canvas, TRANSPARENT);

    HDC logo = load_picture(disc, LOGO_PICTURE);
    if (!logo) fprintf(stderr, "screen: the game's logo could not be read\n");
    draw_base(logo);

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = screen_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"diva_language_screen";
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

void kit_clear(void) {
    memcpy(g_px, g_base, CANVAS_W * CANVAS_H * 4);
    g_frames = 0;
}

void kit_row(int x, int y, int w, int h, int radius, int on) {
    draw_row(x, y, w, h, radius, on);
}

void kit_text(int x, int y, int height, int bold, uint32_t rgb, const wchar_t *s, int centred) {
    text(x, y, height, bold ? FW_SEMIBOLD : FW_NORMAL, RGB(rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255), s, centred);
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

const char *language_screen_run(const char *current, const char *disc) {
    int selected = 0;
    for (int i = 0; i < NUM_LANGUAGES; i++) {
        if (!strcmp(LANGUAGES[i].code, current)) selected = i;
    }
    if (!kit_open(disc)) return current;
    fprintf(stderr, "language screen: shown, between the intros and the title\n");

    draw_screen(selected);
    g_frames = 0;
    /* A key already down from skipping an intro is not an answer. */
    uint32_t held = ~(uint32_t)0;
    int chosen = 0;
    while (!chosen) {
        kit_frame();
        const uint32_t buttons = psp_window_buttons();
        const uint32_t pressed = buttons & ~held;
        held = buttons;
        int changed = 0;
        if (pressed & B_UP) {
            selected = (selected + NUM_LANGUAGES - 1) % NUM_LANGUAGES;
            changed = 1;
        }
        if (pressed & B_DOWN) {
            selected = (selected + 1) % NUM_LANGUAGES;
            changed = 1;
        }
        if (changed) {
            draw_screen(selected);
            g_frames = 0;
        }
        if (pressed & (B_START | B_CIRCLE | B_CROSS)) chosen = 1;
        Sleep(16);
    }
    /* The press that answered is not for the title screen that comes next. */
    while (psp_window_buttons() && !psp_window_closed()) {
        pump();
        Sleep(16);
    }
    kit_close();
    return LANGUAGES[selected].code;
}

/* Pictures read from files: PNG, JPEG, BMP and GIF, by GDI+. */
#include "pictures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ---- reading a picture, by GDI+ -------------------------------------------------------- */

typedef struct {
    UINT32 version;
    void *callback;
    BOOL no_thread;
    BOOL no_codecs;
} gdip_start;
typedef struct {
    INT x, y, w, h;
} gdip_rect;
typedef struct {
    UINT w, h;
    INT stride;
    INT format;
    void *pixels;
    UINT_PTR reserved;
} gdip_data;

#define GDIP_LOCK_READ 1
#define GDIP_ARGB 0x0026200A /* 32 bits, not premultiplied: B, G, R, A in memory */

static struct {
    int ready;
    int(WINAPI *from_file)(const WCHAR *, void **);
    int(WINAPI *get_w)(void *, UINT *);
    int(WINAPI *get_h)(void *, UINT *);
    int(WINAPI *lock)(void *, const gdip_rect *, UINT, INT, gdip_data *);
    int(WINAPI *unlock)(void *, gdip_data *);
    int(WINAPI *dispose)(void *);
} G;

static int gdip_ready(void) {
    if (G.ready) return G.ready > 0;
    G.ready = -1;
    HMODULE lib = LoadLibraryA("gdiplus.dll");
    if (!lib) return 0;
    int(WINAPI *startup)(ULONG_PTR *, const gdip_start *, void *) = (void *)GetProcAddress(lib, "GdiplusStartup");
    G.from_file = (void *)GetProcAddress(lib, "GdipCreateBitmapFromFile");
    G.get_w = (void *)GetProcAddress(lib, "GdipGetImageWidth");
    G.get_h = (void *)GetProcAddress(lib, "GdipGetImageHeight");
    G.lock = (void *)GetProcAddress(lib, "GdipBitmapLockBits");
    G.unlock = (void *)GetProcAddress(lib, "GdipBitmapUnlockBits");
    G.dispose = (void *)GetProcAddress(lib, "GdipDisposeImage");
    if (!startup || !G.from_file || !G.get_w || !G.get_h || !G.lock || !G.unlock || !G.dispose) return 0;
    ULONG_PTR token;
    const gdip_start start = {1, NULL, FALSE, FALSE};
    if (startup(&token, &start, NULL) != 0) return 0;
    G.ready = 1;
    return 1;
}

/* Its pixels as R, G, B, A, top row first, or NULL. */
/* ---- TGA, which GDI+ does not read: true colour or grey, plain or run-length ------------- */

static uint8_t *read_tga(const char *path, UINT *w, UINT *h) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc(size > 18 ? (size_t)size : 18);
    if (!b || size < 18 || fread(b, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(b); return NULL; }
    fclose(f);
    const int idlen = b[0], cmap = b[1], type = b[2], bpp = b[16], desc = b[17];
    *w = (UINT)(b[12] | b[13] << 8);
    *h = (UINT)(b[14] | b[15] << 8);
    const int grey = type == 3 || type == 11, rle = type >= 9, bytes = bpp / 8;
    if (cmap || (type != 2 && type != 3 && type != 10 && type != 11) || !*w || !*h || *w > 8192 || *h > 8192 ||
        (grey ? bytes != 1 : bytes != 3 && bytes != 4)) {
        free(b);
        return NULL;
    }
    const size_t n = (size_t)*w * *h;
    uint8_t *out = (uint8_t *)malloc(n * 4);
    size_t at = 18 + (size_t)idlen, i = 0;
    while (out && i < n && at < (size_t)size) {
        int count = 1, raw = 1;
        if (rle) {
            const int c = b[at++];
            count = (c & 0x7F) + 1;
            raw = !(c & 0x80);
        }
        if (!rle) count = (int)(n - i);
        const uint8_t *px = b + at;
        for (int k = 0; k < count && i < n; k++, i++) {
            if (raw || k == 0) {
                if (at + (size_t)bytes > (size_t)size) { i = n; break; }
                px = b + at;
                at += (size_t)bytes;
            }
            uint8_t *o = out + i * 4;
            if (grey) { o[0] = o[1] = o[2] = px[0]; o[3] = 255; }
            else { o[0] = px[2]; o[1] = px[1]; o[2] = px[0]; o[3] = bytes == 4 ? px[3] : 255; }
        }
    }
    free(b);
    if (!out) return NULL;
    /* Rows from the bottom unless the descriptor says from the top. */
    if (!(desc & 0x20)) {
        const size_t row = (size_t)*w * 4;
        uint8_t *tmp = (uint8_t *)malloc(row);
        for (UINT y = 0; tmp && y < *h / 2; y++) {
            memcpy(tmp, out + y * row, row);
            memcpy(out + y * row, out + (*h - 1 - y) * row, row);
            memcpy(out + (*h - 1 - y) * row, tmp, row);
        }
        free(tmp);
    }
    return out;
}

uint8_t *picture_read(const char *path, UINT *w, UINT *h) {
    const char *dot = strrchr(path, '.');
    if (dot && !_stricmp(dot, ".tga")) return read_tga(path, w, h);
    if (!gdip_ready()) return NULL;
    WCHAR wide[1100];
    if (!MultiByteToWideChar(CP_ACP, 0, path, -1, wide, 1100)) return NULL;
    void *bitmap = NULL;
    if (G.from_file(wide, &bitmap) != 0 || !bitmap) return NULL;
    uint8_t *out = NULL;
    G.get_w(bitmap, w);
    G.get_h(bitmap, h);
    if (*w && *h && *w <= 4096 && *h <= 4096) {
        const gdip_rect all = {0, 0, (INT)*w, (INT)*h};
        gdip_data data;
        if (G.lock(bitmap, &all, GDIP_LOCK_READ, GDIP_ARGB, &data) == 0) {
            out = (uint8_t *)malloc((size_t)*w * *h * 4);
            for (UINT y = 0; out && y < *h; y++) {
                const uint8_t *from = (const uint8_t *)data.pixels + (INT)y * data.stride;
                uint8_t *to = out + (size_t)y * *w * 4;
                for (UINT x = 0; x < *w; x++) {
                    to[x * 4 + 0] = from[x * 4 + 2];
                    to[x * 4 + 1] = from[x * 4 + 1];
                    to[x * 4 + 2] = from[x * 4 + 0];
                    to[x * 4 + 3] = from[x * 4 + 3];
                }
            }
            G.unlock(bitmap, &data);
        }
    }
    G.dispose(bitmap);
    return out;
}


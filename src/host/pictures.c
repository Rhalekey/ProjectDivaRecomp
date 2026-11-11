/* Pictures read from files: PNG, JPEG, BMP and GIF, by GDI+. */
#include "pictures.h"

#include <stdlib.h>
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
uint8_t *picture_read(const char *path, UINT *w, UINT *h) {
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


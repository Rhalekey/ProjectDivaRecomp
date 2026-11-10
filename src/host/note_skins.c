/* Note skins: pictures a player drops in the mods folder, offered as more note styles.
 *
 *     mods\notes\<name>\circle.png     the note of each button, its shape centred in a square
 *     mods\notes\<name>\cross.png
 *     mods\notes\<name>\square.png
 *     mods\notes\<name>\triangle.png
 *     mods\notes\<name>\targets.png    optional: where the notes fly to, one in each quarter,
 *                                      circle top left, triangle top right, square bottom left,
 *                                      cross bottom right, on a clear ground
 *
 * A picture left out keeps the game's own. Any size works; the game's own note is 64 pixels
 * across, and the template drawn by the build is 256. Each start reads them again into the
 * renderer's format, rows bottom first under the game's names for those textures, so changing a
 * picture needs only a restart. mods\notes\_template holds the template, written there when
 * missing; a folder whose name starts with '_' is not offered.
 */

#include "note_skins.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

#define MAX_SKINS 32
#define NAME_LEN 48

typedef struct {
    char name[NAME_LEN];
    char folder[1100];
} skin;

static skin g_skins[MAX_SKINS];
static int g_count;

/* The game's names for the four note textures, by colour hash. See NOTE_FILES in Tools/hud_vector.py. */
static const struct {
    const char *picture;
    const char *texture;
} NOTES[] = {
    {"circle.png", "AC18437B.rgba"},
    {"cross.png", "17820130.rgba"},
    {"square.png", "D50BEC6E.rgba"},
    {"triangle.png", "0FD2D4FB.rgba"},
};

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
static uint8_t *read_picture(const char *path, UINT *w, UINT *h) {
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

/* The renderer's format: width, height, for targets their side in texture units, then the rows
 * from the bottom up, the order the game keeps its textures in. */
static int write_texture(const char *picture, const char *texture, int with_side) {
    UINT w = 0, h = 0;
    uint8_t *pixels = read_picture(picture, &w, &h);
    if (!pixels) {
        fprintf(stderr, "note skins: cannot read %s\n", picture);
        return 0;
    }
    FILE *f = fopen(texture, "wb");
    int ok = 0;
    if (f) {
        const uint32_t head[3] = {w, h, w};
        ok = fwrite(head, 4, with_side ? 3 : 2, f) == (size_t)(with_side ? 3 : 2);
        for (UINT y = h; ok && y-- > 0;) ok = fwrite(pixels + (size_t)y * w * 4, 4, w, f) == w;
        ok = fclose(f) == 0 && ok;
    }
    free(pixels);
    if (!ok) fprintf(stderr, "note skins: cannot write %s\n", texture);
    return ok;
}

static int exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

/* Reads one skin's pictures into `out`. Returns how many it found. */
static int convert(const char *from, const char *out) {
    int found = 0;
    char picture[1200], texture[1200];
    for (size_t i = 0; i < sizeof NOTES / sizeof NOTES[0]; i++) {
        snprintf(picture, sizeof picture, "%s\\%s", from, NOTES[i].picture);
        snprintf(texture, sizeof texture, "%s\\%s", out, NOTES[i].texture);
        if (exists(picture)) found += write_texture(picture, texture, 0);
        else DeleteFileA(texture);
    }
    snprintf(picture, sizeof picture, "%s\\targets.png", from);
    snprintf(texture, sizeof texture, "%s\\targets.rgba", out);
    if (exists(picture)) found += write_texture(picture, texture, 1);
    else DeleteFileA(texture);
    return found;
}

static const char README[] =
    "Note skins for Project DIVA Recomp\r\n"
    "\r\n"
    "Each folder here beside this one is a note style, offered in Options > Notes after the\r\n"
    "game's own. Its name is the folder's name. Put in it, as PNG:\r\n"
    "\r\n"
    "  circle.png  cross.png  square.png  triangle.png   one note each, centred in a square\r\n"
    "  targets.png   optional: the targets, circle top left, triangle top right,\r\n"
    "                square bottom left, cross bottom right, on a clear ground\r\n"
    "\r\n"
    "A picture left out keeps the game's own. The pictures in this folder are a template at\r\n"
    "the size and place the game expects. Folders whose names start with _ are not offered.\r\n"
    "Changes show after the game is started again.\r\n";

/* Puts the template drawn by the build in mods\notes\_template, if it is not there. */
static void write_template(const char *notes_dir, const char *data_dir) {
    char dir[1100], from[1200], to[1200];
    snprintf(dir, sizeof dir, "%s\\_template", notes_dir);
    if (exists(dir)) return;
    char source[1100];
    snprintf(source, sizeof source, "%s\\..\\textures_hd\\notes\\template", data_dir);
    if (!exists(source)) return;
    _mkdir(dir);
    static const char *const FILES[] = {"circle.png", "cross.png", "square.png", "triangle.png", "targets.png"};
    for (size_t i = 0; i < sizeof FILES / sizeof FILES[0]; i++) {
        snprintf(from, sizeof from, "%s\\%s", source, FILES[i]);
        snprintf(to, sizeof to, "%s\\%s", dir, FILES[i]);
        CopyFileA(from, to, TRUE);
    }
    snprintf(to, sizeof to, "%s\\README.txt", dir);
    FILE *f = fopen(to, "wb");
    if (f) {
        fwrite(README, 1, sizeof README - 1, f);
        fclose(f);
    }
    fprintf(stderr, "note skins: template written to %s\n", dir);
}

int note_skins_scan(const char *mods_dir, const char *data_dir) {
    g_count = 0;
    char notes_dir[1100], out_root[1100];
    snprintf(notes_dir, sizeof notes_dir, "%s\\notes", mods_dir);
    _mkdir(mods_dir);
    _mkdir(notes_dir);
    write_template(notes_dir, data_dir);

    snprintf(out_root, sizeof out_root, "%s\\..\\textures_hd\\notes\\mods", data_dir);
    _mkdir(out_root);
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s\\*", notes_dir);
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.' || found.cFileName[0] == '_') continue;
        if (strlen(found.cFileName) >= NAME_LEN || g_count == MAX_SKINS) continue;
        skin *s = &g_skins[g_count];
        char from[1200];
        snprintf(from, sizeof from, "%s\\%s", notes_dir, found.cFileName);
        snprintf(s->folder, sizeof s->folder, "%s\\%s", out_root, found.cFileName);
        _mkdir(s->folder);
        const int pictures = convert(from, s->folder);
        if (!pictures) {
            fprintf(stderr, "note skins: \"%s\" has none of circle, cross, square, triangle or targets .png\n", found.cFileName);
            continue;
        }
        snprintf(s->name, sizeof s->name, "%s", found.cFileName);
        fprintf(stderr, "note skins: \"%s\", %d pictures\n", s->name, pictures);
        g_count++;
    } while (FindNextFileA(h, &found));
    FindClose(h);
    return g_count;
}

int note_skins_count(void) { return g_count; }

const char *note_skins_name(int skin) { return skin >= 0 && skin < g_count ? g_skins[skin].name : ""; }

const char *note_skins_folder(int skin) { return skin >= 0 && skin < g_count ? g_skins[skin].folder : NULL; }

int note_skins_find(const char *name) {
    for (int i = 0; i < g_count; i++)
        if (_stricmp(g_skins[i].name, name) == 0) return i;
    return -1;
}

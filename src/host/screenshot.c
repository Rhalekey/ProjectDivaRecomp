/* Screenshots, kept as PNG files in the player's Pictures folder.
 *
 * The game asks for one through the system's screenshot dialog, which on a PSP saved the picture
 * to the Memory Stick. Here the renderer hands over the next frame as the player sees it, at the
 * size it is drawn, and a thread of its own writes it with GDI+, which every Windows has and
 * which is loaded only when wanted, as the language screen does. */
#include "screenshot.h"

#include <psprecomp/render.h>

#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

#define FOLDER_NAME L"Project DIVA Recomp"
#define FILE_PREFIX L"Project DIVA"
#define PIXEL_FORMAT_32BPP_RGB 0x00022009
#define MOST_SAME_NAME 100

typedef struct {
    uint8_t *bgra;
    int w, h;
    SYSTEMTIME when;
} job;

/* Where the player's pictures go, with this game's folder made inside it. */
static int picture_folder(wchar_t *out, size_t size) {
    static const GUID PICTURES = {0x33E28130, 0x4E1E, 0x4676, {0x83, 0x5A, 0x98, 0x39, 0x5C, 0x3B, 0xC3, 0xBB}};
    HMODULE shell = LoadLibraryW(L"shell32.dll");
    HMODULE ole = LoadLibraryW(L"ole32.dll");
    HRESULT(WINAPI *known)(const GUID *, DWORD, HANDLE, wchar_t **) =
        shell ? (void *)GetProcAddress(shell, "SHGetKnownFolderPath") : NULL;
    void(WINAPI *task_free)(void *) = ole ? (void *)GetProcAddress(ole, "CoTaskMemFree") : NULL;
    wchar_t *found = NULL;
    out[0] = 0;
    if (known && known(&PICTURES, 0, NULL, &found) == S_OK && found) {
        swprintf(out, size, L"%ls\\" FOLDER_NAME, found);
        if (task_free) task_free(found);
    } else {
        const wchar_t *home = _wgetenv(L"USERPROFILE");
        if (home) swprintf(out, size, L"%ls\\Pictures\\" FOLDER_NAME, home);
    }
    if (!out[0]) return -1;
    CreateDirectoryW(out, NULL);
    return GetFileAttributesW(out) & FILE_ATTRIBUTE_DIRECTORY ? 0 : -1;
}

/* Named for when it was taken; a second in the same second gets a number. */
static int file_name(const job *j, wchar_t *out, size_t size) {
    wchar_t folder[MAX_PATH];
    if (picture_folder(folder, MAX_PATH) != 0) return -1;
    for (int n = 1; n <= MOST_SAME_NAME; n++) {
        wchar_t extra[16] = L"";
        if (n > 1) swprintf(extra, 16, L" (%d)", n);
        swprintf(out, size, L"%ls\\" FILE_PREFIX L" %04u-%02u-%02u %02u-%02u-%02u%ls.png", folder, j->when.wYear,
                 j->when.wMonth, j->when.wDay, j->when.wHour, j->when.wMinute, j->when.wSecond, extra);
        if (GetFileAttributesW(out) == INVALID_FILE_ATTRIBUTES) return 0;
    }
    return -1;
}

typedef struct {
    UINT32 version;
    void *debug_callback;
    BOOL no_background_thread;
    BOOL no_external_codecs;
} gdiplus_start;

static int write_png(const job *j, const wchar_t *path) {
    static const GUID PNG_ENCODER = {0x557CF406, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
    HMODULE gdiplus = LoadLibraryW(L"gdiplus.dll");
    if (!gdiplus) return -1;
    int(WINAPI *startup)(ULONG_PTR *, const gdiplus_start *, void *) = (void *)GetProcAddress(gdiplus, "GdiplusStartup");
    void(WINAPI *shutdown)(ULONG_PTR) = (void *)GetProcAddress(gdiplus, "GdiplusShutdown");
    int(WINAPI *from_scan0)(INT, INT, INT, INT, BYTE *, void **) = (void *)GetProcAddress(gdiplus, "GdipCreateBitmapFromScan0");
    int(WINAPI *save)(void *, const wchar_t *, const GUID *, const void *) = (void *)GetProcAddress(gdiplus, "GdipSaveImageToFile");
    int(WINAPI *dispose)(void *) = (void *)GetProcAddress(gdiplus, "GdipDisposeImage");
    if (!startup || !shutdown || !from_scan0 || !save || !dispose) return -1;
    ULONG_PTR token;
    const gdiplus_start start = {1, NULL, FALSE, FALSE};
    if (startup(&token, &start, NULL) != 0) return -1;
    void *bitmap = NULL;
    int failed = from_scan0(j->w, j->h, j->w * 4, PIXEL_FORMAT_32BPP_RGB, j->bgra, &bitmap) != 0;
    if (!failed) {
        failed = save(bitmap, path, &PNG_ENCODER, NULL) != 0;
        dispose(bitmap);
    }
    shutdown(token);
    return failed ? -1 : 0;
}

static unsigned __stdcall save_thread(void *arg) {
    job *j = arg;
    wchar_t path[MAX_PATH];
    if (file_name(j, path, MAX_PATH) == 0 && write_png(j, path) == 0) {
        /* Logs get shared, so the home folder is named, not spelled out. */
        const wchar_t *home = _wgetenv(L"USERPROFILE");
        const size_t home_len = home ? wcslen(home) : 0;
        if (home_len && _wcsnicmp(path, home, home_len) == 0) {
            fprintf(stderr, "screenshot: saved %%USERPROFILE%%%ls (%dx%d)\n", path + home_len, j->w, j->h);
        } else {
            fprintf(stderr, "screenshot: saved in the Pictures folder (%dx%d)\n", j->w, j->h);
        }
    } else {
        fprintf(stderr, "screenshot: could not be saved\n");
    }
    free(j->bgra);
    free(j);
    return 0;
}

/* On the drawing thread: copy the picture and let it be written elsewhere. */
static void taken(const uint8_t *bgra, int w, int h) {
    job *j = malloc(sizeof *j);
    uint8_t *copy = malloc((size_t)w * h * 4);
    if (!j || !copy) {
        free(j);
        free(copy);
        return;
    }
    memcpy(copy, bgra, (size_t)w * h * 4);
    j->bgra = copy;
    j->w = w;
    j->h = h;
    GetLocalTime(&j->when);
    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, save_thread, j, 0, NULL);
    if (thread) {
        CloseHandle(thread);
    } else {
        free(copy);
        free(j);
    }
}

void screenshot_take(void) { psp_render_gl_take_screenshot(taken); }

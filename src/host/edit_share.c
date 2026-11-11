/* Rhythm Game Edit's data shared by song. See edit_share.h.
 *
 * The zip is stored, not compressed, as the MP3 in it would not shrink: "music/<the MP3>" if the
 * song is the player's own, then each Edit's save folder as "edit1/", "edit2/" and so on. The
 * Edits are matched by the song named in their description, and the MP3 by its file name, which
 * is what the game keeps of it (the first 36 or so letters of it). An imported Edit goes into the
 * first free slot; its MP3 goes into MUSIC unless one of that name is there already. */
#include "edit_share.h"

#include "language.h"
#include "ui_text.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define GAME_ID L"ULJM05472"
#define MOST_FILES 128
#define MOST_EDITS 90

/* ---- where things are ------------------------------------------------------------------- */
static void data_path(const wchar_t *tail, wchar_t *out, size_t cap) {
    wchar_t data[1100];
    if (!MultiByteToWideChar(CP_ACP, 0, language_data_dir(), -1, data, 1100)) data[0] = 0;
    swprintf(out, cap, L"%ls\\%ls", data, tail);
}

/* Documents\Project DIVA Recomp\<name>, made if it is not there. */
static int share_folder(const wchar_t *name, wchar_t *out, size_t cap) {
    static const GUID DOCUMENTS = {0xFDD39AD0, 0x238F, 0x46AF, {0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7}};
    HMODULE shell = LoadLibraryW(L"shell32.dll"), ole = LoadLibraryW(L"ole32.dll");
    HRESULT(WINAPI *known)(const GUID *, DWORD, HANDLE, wchar_t **) = shell ? (void *)GetProcAddress(shell, "SHGetKnownFolderPath") : NULL;
    void(WINAPI *task_free)(void *) = ole ? (void *)GetProcAddress(ole, "CoTaskMemFree") : NULL;
    wchar_t base[MAX_PATH] = L"", *found = NULL;
    if (known && known(&DOCUMENTS, 0, NULL, &found) == S_OK && found) {
        swprintf(base, MAX_PATH, L"%ls", found);
        if (task_free) task_free(found);
    } else if (_wgetenv(L"USERPROFILE")) {
        swprintf(base, MAX_PATH, L"%ls\\Documents", _wgetenv(L"USERPROFILE"));
    }
    if (!base[0]) return -1;
    swprintf(out, cap, L"%ls\\Project DIVA Recomp", base);
    CreateDirectoryW(out, NULL);
    swprintf(out, cap, L"%ls\\Project DIVA Recomp\\%ls", base, name);
    CreateDirectoryW(out, NULL);
    return GetFileAttributesW(out) & FILE_ATTRIBUTE_DIRECTORY ? 0 : -1;
}

/* The song an Edit was made with: the second line of its description. */
static void song_of(const psp_save_slot *s, wchar_t *out, int cap) {
    wchar_t all[1100];
    out[0] = 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, s->detail, -1, all, 1100)) return;
    wchar_t *line = wcschr(all, L'\n');
    if (!line) return;
    line++;
    int n = 0;
    while (line[n] && line[n] != L'\n' && line[n] != L'\r' && n < cap - 1) {
        out[n] = line[n];
        n++;
    }
    out[n] = 0;
}

/* Whether a file is music the Memory Stick plays: an MP3, or another kind shown to the game as one. */
static int is_music(const wchar_t *name) {
    static const wchar_t *const KINDS[] = {L".mp3", L".m4a", L".aac", L".mp4", L".ogg", L".oga", L".opus", L".flac", L".wav", L".wma"};
    const wchar_t *dot = wcsrchr(name, L'.');
    for (size_t k = 0; dot && k < sizeof KINDS / sizeof KINDS[0]; k++)
        if (!_wcsicmp(dot, KINDS[k])) return 1;
    return 0;
}

/* The player's music a song names, by the start of its file name. */
static int mp3_of(const wchar_t *song, wchar_t *path, size_t path_cap, wchar_t *name, size_t name_cap) {
    wchar_t pattern[1200];
    data_path(L"ms\\MUSIC\\*", pattern, 1200);
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileW(pattern, &d);
    if (h == INVALID_HANDLE_VALUE || !song[0]) return 0;
    int found = 0;
    do {
        if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && is_music(d.cFileName) && _wcsnicmp(d.cFileName, song, wcslen(song)) == 0) {
            swprintf(name, name_cap, L"%ls", d.cFileName);
            wchar_t folder[1200];
            data_path(L"ms\\MUSIC", folder, 1200);
            swprintf(path, path_cap, L"%ls\\%ls", folder, d.cFileName);
            found = 1;
        }
    } while (!found && FindNextFileW(h, &d));
    FindClose(h);
    return found;
}

static void slot_folder(const char *name, wchar_t *out, size_t cap) {
    wchar_t tail[200], wide[40];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wide, 40);
    swprintf(tail, 200, L"ms\\PSP\\SAVEDATA\\" GAME_ID L"%ls", wide);
    data_path(tail, out, cap);
}

/* ---- a stored zip --------------------------------------------------------------------------- */
static uint32_t crc32_of(const uint8_t *p, size_t n) {
    static uint32_t table[256];
    if (!table[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static uint8_t *read_all(const wchar_t *path, uint32_t *size) {
    FILE *f = _wfopen(path, L"rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = n >= 0 ? (uint8_t *)malloc((size_t)n + 1) : NULL;
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (uint32_t)n;
    return data;
}

static void put16(FILE *f, uint32_t v) { fputc((int)(v & 0xFF), f); fputc((int)((v >> 8) & 0xFF), f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }

typedef struct {
    FILE *f;
    int n;
    struct {
        char name[300];
        uint32_t crc, size, at;
    } e[MOST_FILES];
} zip_out;

static int zip_add(zip_out *z, const char *name, const wchar_t *path) {
    if (z->n >= MOST_FILES) return 0;
    uint32_t size = 0;
    uint8_t *data = read_all(path, &size);
    if (!data) return 0;
    const uint32_t crc = crc32_of(data, size), at = (uint32_t)ftell(z->f), len = (uint32_t)strlen(name);
    put32(z->f, 0x04034B50u);
    put16(z->f, 20);
    put16(z->f, 0x0800);      /* the name is UTF-8 */
    put16(z->f, 0);           /* stored */
    put16(z->f, 0);
    put16(z->f, 0x21);        /* 1980-01-01 */
    put32(z->f, crc);
    put32(z->f, size);
    put32(z->f, size);
    put16(z->f, len);
    put16(z->f, 0);
    fwrite(name, 1, len, z->f);
    fwrite(data, 1, size, z->f);
    free(data);
    snprintf(z->e[z->n].name, sizeof z->e[z->n].name, "%s", name);
    z->e[z->n].crc = crc;
    z->e[z->n].size = size;
    z->e[z->n].at = at;
    z->n++;
    return 1;
}

static void zip_finish(zip_out *z) {
    const uint32_t start = (uint32_t)ftell(z->f);
    for (int i = 0; i < z->n; i++) {
        const uint32_t len = (uint32_t)strlen(z->e[i].name);
        put32(z->f, 0x02014B50u);
        put16(z->f, 20);
        put16(z->f, 20);
        put16(z->f, 0x0800);
        put16(z->f, 0);
        put16(z->f, 0);
        put16(z->f, 0x21);
        put32(z->f, z->e[i].crc);
        put32(z->f, z->e[i].size);
        put32(z->f, z->e[i].size);
        put16(z->f, len);
        put16(z->f, 0);
        put16(z->f, 0);
        put16(z->f, 0);
        put16(z->f, 0);
        put32(z->f, 0);
        put32(z->f, z->e[i].at);
        fwrite(z->e[i].name, 1, len, z->f);
    }
    const uint32_t end = (uint32_t)ftell(z->f);
    put32(z->f, 0x06054B50u);
    put16(z->f, 0);
    put16(z->f, 0);
    put16(z->f, (uint32_t)z->n);
    put16(z->f, (uint32_t)z->n);
    put32(z->f, end - start);
    put32(z->f, start);
    put16(z->f, 0);
}

/* Every file of a folder into the zip under `prefix`. Gives back how many. */
static int zip_folder(zip_out *z, const wchar_t *folder, const char *prefix) {
    wchar_t pattern[1200];
    swprintf(pattern, 1200, L"%ls\\*", folder);
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileW(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = 0;
    do {
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char file[260], name[300];
        wchar_t path[1300];
        WideCharToMultiByte(CP_UTF8, 0, d.cFileName, -1, file, 260, NULL, NULL);
        snprintf(name, sizeof name, "%s/%s", prefix, file);
        swprintf(path, 1300, L"%ls\\%ls", folder, d.cFileName);
        n += zip_add(z, name, path);
    } while (FindNextFileW(h, &d));
    FindClose(h);
    return n;
}

/* ---- words --------------------------------------------------------------------------------- */

/* ---- export --------------------------------------------------------------------------------- */
int edit_share_export(const psp_save_slot *slots, int count, int chosen, wchar_t *message, int cap) {
    const int ui = ui_text_language();
    wchar_t song[200], folder[MAX_PATH], file[MAX_PATH + 200], safe[200];
    song_of(&slots[chosen], song, 200);
    if (!song[0] || share_folder(L"Export", folder, MAX_PATH) != 0) {
        swprintf(message, cap, L"%ls", ui_wtext_in(ui, "share.failed"));
        return 0;
    }
    /* A file name of the song's, without what Windows does not allow in one. */
    int n = 0;
    for (int i = 0; song[i] && n < 120; i++) safe[n++] = wcschr(L"<>:\"/\\|?*", song[i]) ? L'_' : song[i];
    safe[n] = 0;
    swprintf(file, MAX_PATH + 200, L"%ls\\%ls.zip", folder, safe);
    for (int k = 2; GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES && k < 100; k++)
        swprintf(file, MAX_PATH + 200, L"%ls\\%ls (%d).zip", folder, safe, k);
    zip_out *z = (zip_out *)calloc(1, sizeof *z);
    if (!z || !(z->f = _wfopen(file, L"wb"))) {
        free(z);
        swprintf(message, cap, L"%ls", ui_wtext_in(ui, "share.failed"));
        return 0;
    }
    wchar_t mp3[1300], mp3_name[300];
    if (mp3_of(song, mp3, 1300, mp3_name, 300)) {
        char utf8[600], name[700];
        WideCharToMultiByte(CP_UTF8, 0, mp3_name, -1, utf8, sizeof utf8, NULL, NULL);
        snprintf(name, sizeof name, "music/%s", utf8);
        zip_add(z, name, mp3);
    }
    int edits = 0;
    for (int i = 0; i < count; i++) {
        wchar_t other[200];
        if (!slots[i].used || strncmp(slots[i].name, "EDIT", 4) != 0) continue;
        song_of(&slots[i], other, 200);
        if (wcscmp(other, song) != 0) continue;
        wchar_t from[1200];
        char prefix[32];
        slot_folder(slots[i].name, from, 1200);
        snprintf(prefix, sizeof prefix, "edit%d", edits + 1);
        if (zip_folder(z, from, prefix)) edits++;
    }
    zip_finish(z);
    fclose(z->f);
    free(z);
    const wchar_t *shown = wcsrchr(file, L'\\');
    swprintf(message, cap, ui_wtext_in(ui, "share.exported"), shown ? shown + 1 : file);
    fprintf(stderr, "edits: exported %d Edit(s) of a song%s\n", edits, mp3_name[0] ? ", with its MP3" : "");
    return 1;
}

/* ---- import --------------------------------------------------------------------------------- */
static int write_file(const wchar_t *path, const uint8_t *data, uint32_t size) {
    FILE *f = _wfopen(path, L"wb");
    if (!f) return 0;
    const int ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

/* One zip: its Edits into free slots, its MP3 into MUSIC. Gives back the Edits brought in, or -1
 * if there was no room, when it is left where it is. */
static int import_zip(const wchar_t *path, const psp_save_slot *slots, int count, int *taken) {
    uint32_t size = 0;
    uint8_t *zip = read_all(path, &size);
    if (!zip) return 0;
    /* How many Edits it holds, by the folders its names are in, and whether there is room. */
    int most = 0;
    for (uint32_t at = 0; at + 30 <= size;) {
        const uint8_t *h = zip + at;
        if (h[0] != 'P' || h[1] != 'K' || h[2] != 3 || h[3] != 4) break;
        const uint32_t csize = h[18] | h[19] << 8 | h[20] << 16 | (uint32_t)h[21] << 24, nlen = h[26] | h[27] << 8, xlen = h[28] | h[29] << 8;
        int k = 0;
        if (nlen > 4 && sscanf((const char *)h + 30, "edit%d/", &k) == 1 && k > most) most = k;
        at += 30 + nlen + xlen + csize;
    }
    int free_slots = 0;
    for (int i = 0; i < count; i++) free_slots += !slots[i].used && !taken[i] && !strncmp(slots[i].name, "EDIT", 4);
    if (most > free_slots) {
        free(zip);
        return -1;
    }
    int slot_for[MOST_EDITS + 1] = {0};
    for (int k = 1; k <= most && k <= MOST_EDITS; k++) {
        slot_for[k] = -1;
        for (int i = 0; i < count; i++) {
            if (!slots[i].used && !taken[i] && !strncmp(slots[i].name, "EDIT", 4)) {
                taken[i] = 1;
                slot_for[k] = i;
                break;
            }
        }
    }
    int edits = 0;
    for (uint32_t at = 0; at + 30 <= size;) {
        const uint8_t *h = zip + at;
        if (h[0] != 'P' || h[1] != 'K' || h[2] != 3 || h[3] != 4) break;
        const uint32_t method = h[8] | h[9] << 8, csize = h[18] | h[19] << 8 | h[20] << 16 | (uint32_t)h[21] << 24;
        const uint32_t nlen = h[26] | h[27] << 8, xlen = h[28] | h[29] << 8;
        char name[300];
        snprintf(name, sizeof name, "%.*s", (int)(nlen < 299 ? nlen : 299), (const char *)h + 30);
        const uint8_t *data = h + 30 + nlen + xlen;
        at += 30 + nlen + xlen + csize;
        if (method != 0 || data + csize > zip + size) continue;   /* only stored files, as exported */
        const char *file = strchr(name, '/');
        if (!file || !file[1] || strstr(name, "..")) continue;
        wchar_t wfile[300], target[1400];
        MultiByteToWideChar(CP_UTF8, 0, file + 1, -1, wfile, 300);
        int k = 0;
        if (!strncmp(name, "music/", 6)) {
            wchar_t folder[1200];
            data_path(L"ms\\MUSIC", folder, 1200);
            CreateDirectoryW(folder, NULL);
            swprintf(target, 1400, L"%ls\\%ls", folder, wfile);
            if (GetFileAttributesW(target) == INVALID_FILE_ATTRIBUTES) write_file(target, data, csize);
        } else if (sscanf(name, "edit%d/", &k) == 1 && k >= 1 && k <= most && slot_for[k] >= 0) {
            wchar_t folder[1200];
            slot_folder(slots[slot_for[k]].name, folder, 1200);
            CreateDirectoryW(folder, NULL);
            swprintf(target, 1400, L"%ls\\%ls", folder, wfile);
            write_file(target, data, csize);
        }
    }
    for (int k = 1; k <= most; k++) edits += slot_for[k] >= 0;
    free(zip);
    return edits;
}

int edit_share_import(const psp_save_slot *slots, int count, wchar_t *message, int cap) {
    wchar_t folder[MAX_PATH], pattern[MAX_PATH + 8];
    message[0] = 0;
    if (share_folder(L"Import", folder, MAX_PATH) != 0) return 0;
    swprintf(pattern, MAX_PATH + 8, L"%ls\\*.zip", folder);
    WIN32_FIND_DATAW d;
    HANDLE h = FindFirstFileW(pattern, &d);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int taken[1001] = {0}, total = 0;
    const int ui = ui_text_language();
    do {
        wchar_t path[MAX_PATH + 300];
        swprintf(path, MAX_PATH + 300, L"%ls\\%ls", folder, d.cFileName);
        const int got = import_zip(path, slots, count < 1001 ? count : 1001, taken);
        if (got < 0) {
            swprintf(message, cap, ui_wtext_in(ui, "share.full"), d.cFileName);
            continue;
        }
        total += got;
        /* Moved aside, so that it is brought in once. */
        wchar_t done[MAX_PATH + 40], moved[MAX_PATH + 400];
        swprintf(done, MAX_PATH + 40, L"%ls\\Imported", folder);
        CreateDirectoryW(done, NULL);
        swprintf(moved, MAX_PATH + 400, L"%ls\\%ls", done, d.cFileName);
        for (int k = 2; GetFileAttributesW(moved) != INVALID_FILE_ATTRIBUTES && k < 100; k++)
            swprintf(moved, MAX_PATH + 400, L"%ls\\%d %ls", done, k, d.cFileName);
        MoveFileW(path, moved);
    } while (FindNextFileW(h, &d));
    FindClose(h);
    if (total) swprintf(message, cap, ui_wtext_in(ui, "share.imported"), total);
    fprintf(stderr, "edits: imported %d Edit(s)\n", total);
    return total;
}

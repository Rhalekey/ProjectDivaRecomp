/* Mods: replacing the game's files without touching them.
 *
 * A mod is a folder under the mods directory, and every file in it replaces the
 * game's file of the same name:
 *
 *     mods/<any name>/logo_SEGA.png               an entry inside an AFS archive
 *     mods/<any name>/disc/PSP_GAME/USRDIR/x.ini  any other file on the disc
 *
 * Folders inside a mod are only for tidiness, except `disc`. When two mods replace
 * one file, the later folder in alphabetical order wins.
 *
 * Nothing on disk changes. Most of the game's data is in three AFS archives: a table
 * of offsets and sizes, then the files one after another. When the game opens one
 * that a mod touches, it gets a virtual archive instead, with the replacement in the
 * place of the original and everything after it moved along.
 *
 * Moving along matters. CRI's file system keeps only each entry's size, and works
 * out where an entry is by adding up the ones before it, so an archive whose offsets
 * are anything but consecutive sends the game reading the wrong data. That is also
 * where a limit comes from: it refuses any one file of 128 MB or more.
 *
 * So a replacement may be any size below that, as far as the game can cope with it,
 * and with no mods the game reads its files untouched.
 */

#include "mods.h"

#include <psprecomp/hle.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define AFS_SECTOR 0x800u
#define AFS_NAME_RECORD 48u
#define AFS_NAME_LEN 32u

typedef struct {
    char *name;      /* an AFS entry name, or a path relative to the disc */
    char *path;      /* where it is on the host */
} mod_file;

static mod_file *g_entries, *g_loose;
static int g_nentries, g_nloose, g_cap_entries, g_cap_loose;

static char *dup_string(const char *s) {
    const size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* A later mod replaces an earlier one's file. */
static void add_file(mod_file **list, int *count, int *cap, const char *name, const char *path) {
    for (int i = 0; i < *count; i++) {
        if (_stricmp((*list)[i].name, name) == 0) {
            free((*list)[i].path);
            (*list)[i].path = dup_string(path);
            return;
        }
    }
    if (*count == *cap) {
        const int grown = *cap ? *cap * 2 : 64;
        mod_file *bigger = (mod_file *)realloc(*list, (size_t)grown * sizeof *bigger);
        if (!bigger) return;
        *list = bigger;
        *cap = grown;
    }
    (*list)[*count].name = dup_string(name);
    (*list)[*count].path = dup_string(path);
    (*count)++;
}

/* `disc_rel` is non-NULL when inside a mod's disc folder: the path so far. */
static void scan_dir(const char *dir, const char *disc_rel, int top_of_mod) {
    char pattern[1100];
    WIN32_FIND_DATAA found;
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    HANDLE h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (found.cFileName[0] == '.') continue;
        char path[1100], rel[1100];
        snprintf(path, sizeof path, "%s\\%s", dir, found.cFileName);
        if (disc_rel) snprintf(rel, sizeof rel, "%s%s%s", disc_rel, disc_rel[0] ? "/" : "", found.cFileName);

        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (disc_rel) scan_dir(path, rel, 0);
            else if (top_of_mod && _stricmp(found.cFileName, "disc") == 0) scan_dir(path, "", 0);
            else scan_dir(path, NULL, 0);
        } else if (disc_rel) {
            add_file(&g_loose, &g_nloose, &g_cap_loose, rel, path);
        } else {
            add_file(&g_entries, &g_nentries, &g_cap_entries, found.cFileName, path);
        }
    } while (FindNextFileA(h, &found));
    FindClose(h);
}

static const char *find_file(const mod_file *list, int count, const char *name) {
    for (int i = 0; i < count; i++)
        if (_stricmp(list[i].name, name) == 0) return list[i].path;
    return NULL;
}

/* ---- a plain file ------------------------------------------------------- */

static size_t file_read(void *ctx, uint64_t offset, void *dst, size_t n) {
    FILE *f = (FILE *)ctx;
    if (_fseeki64(f, (long long)offset, SEEK_SET) != 0) return 0;
    return fread(dst, 1, n, f);
}

static void file_close(void *ctx) { fclose((FILE *)ctx); }

static uint64_t file_size(FILE *f) {
    _fseeki64(f, 0, SEEK_END);
    return (uint64_t)_ftelli64(f);
}

/* ---- a virtual AFS archive ---------------------------------------------- */

#define AFS_MAX_ENTRY (128u * 1024u * 1024u)

static uint64_t sector_up(uint64_t n) {
    return (n + AFS_SECTOR - 1) & ~(uint64_t)(AFS_SECTOR - 1);
}

typedef struct {
    const char *path;
    FILE *f;
    uint64_t size;
} afs_part;

typedef struct {
    uint64_t offset;       /* where it is in the virtual archive */
    uint64_t source;       /* where it is in the original, if `part` is negative */
    uint32_t size;
    int part;
} afs_entry;

typedef struct {
    FILE *original;
    uint8_t *header;       /* everything before the first entry, table patched */
    uint32_t header_size;
    afs_entry *entries;
    uint32_t count;
    afs_part *parts;
    int nparts;
    uint8_t *names;
    uint32_t names_size;
    uint64_t names_offset;
    uint64_t watched_at, watched_size;
} afs_virtual;

static size_t read_part(afs_part *p, uint64_t offset, void *dst, size_t n) {
    if (!p->f) p->f = fopen(p->path, "rb");
    return p->f ? file_read(p->f, offset, dst, n) : 0;
}

/* Fill `n` bytes starting at `at`, within one stretch of the archive. Returns how
 * many it could: parts of it are a single entry, the padding after one, a table. */
static size_t afs_read_some(afs_virtual *a, uint64_t at, uint8_t *out, size_t n) {
    if (at < a->header_size) {
        const size_t count = n < a->header_size - at ? n : (size_t)(a->header_size - at);
        memcpy(out, a->header + at, count);
        return count;
    }
    if (at >= a->names_offset) {
        const uint64_t past = at - a->names_offset;
        if (past >= a->names_size) {
            /* Padding to the end of the file's last sector. */
            memset(out, 0, n);
            return n;
        }
        const size_t count = n < a->names_size - past ? n : (size_t)(a->names_size - past);
        memcpy(out, a->names + past, count);
        return count;
    }

    /* Entries are in offset order. */
    uint32_t lo = 0, hi = a->count;
    while (hi - lo > 1) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (a->entries[mid].offset <= at) lo = mid;
        else hi = mid;
    }
    const afs_entry *e = &a->entries[lo];
    const uint64_t within = at - e->offset;
    if (within >= e->size) {
        /* Padding up to the next entry, or to the names. */
        const uint64_t next = lo + 1 < a->count ? a->entries[lo + 1].offset : a->names_offset;
        const size_t count = n < next - at ? n : (size_t)(next - at);
        memset(out, 0, count);
        return count;
    }
    const size_t want = n < e->size - within ? n : (size_t)(e->size - within);
    if (e->part >= 0) return read_part(&a->parts[e->part], within, out, want);
    return file_read(a->original, e->source + within, out, want);
}

static int on_open(const char *guest, const char *host, psp_io_virtual *out);
static const char *g_watched_entry;
static void (*g_watch_function)(void);

void mods_on_first_read(const char *entry, void (*function)(void)) {
    g_watched_entry = entry;
    g_watch_function = function;
    psp_io_set_open_hook(on_open);
}

static size_t afs_read(void *ctx, uint64_t offset, void *dst, size_t n) {
    afs_virtual *watching = (afs_virtual *)ctx;
    if (g_watch_function && watching->watched_size && offset < watching->watched_at + watching->watched_size &&
        offset + n > watching->watched_at) {
        void (*function)(void) = g_watch_function;
        g_watch_function = NULL;
        function();
    }
    uint8_t *out = (uint8_t *)dst;
    size_t done = 0;
    while (done < n) {
        const size_t got = afs_read_some((afs_virtual *)ctx, offset + done, out + done, n - done);
        if (!got) break;
        done += got;
    }
    return done;
}

static void afs_close(void *ctx) {
    afs_virtual *a = (afs_virtual *)ctx;
    fclose(a->original);
    for (int i = 0; i < a->nparts; i++)
        if (a->parts[i].f) fclose(a->parts[i].f);
    free(a->parts);
    free(a->entries);
    free(a->names);
    free(a->header);
    free(a);
}

static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* The part for this path, added if it is the first use. Negative if unusable. */
static int get_part(afs_virtual *a, const char *path) {
    for (int k = 0; k < a->nparts; k++)
        if (a->parts[k].path == path) return k;

    FILE *m = fopen(path, "rb");
    if (!m) { fprintf(stderr, "mods: cannot read %s\n", path); return -1; }
    const uint64_t size = file_size(m);
    fclose(m);
    if (size >= AFS_MAX_ENTRY) {
        fprintf(stderr, "mods: %s is too big: an archive entry must be under 128 MB\n", path);
        return -1;
    }
    afs_part *bigger = (afs_part *)realloc(a->parts, (size_t)(a->nparts + 1) * sizeof *bigger);
    if (!bigger) return -1;
    a->parts = bigger;
    a->parts[a->nparts].path = path;
    a->parts[a->nparts].f = NULL;
    a->parts[a->nparts].size = size;
    return a->nparts++;
}

/* Returns 1 and fills `out` if this archive has anything replaced. */
static int open_afs(const char *host, psp_io_virtual *out) {
    FILE *f = fopen(host, "rb");
    if (!f) return 0;
    afs_virtual *a = (afs_virtual *)calloc(1, sizeof *a);
    if (!a) { fclose(f); return 0; }
    a->original = f;

    uint8_t head[8];
    if (fread(head, 1, 8, f) != 8 || memcmp(head, "AFS\0", 4) != 0) goto none;
    a->count = read_u32(head + 4);
    const uint32_t table_size = 8u + a->count * 8u + 8u;

    uint8_t *table = (uint8_t *)malloc(table_size);
    if (!table) goto none;
    _fseeki64(f, 0, SEEK_SET);
    if (fread(table, 1, table_size, f) != table_size || !a->count) { free(table); goto none; }

    /* What sits before the first entry is kept whole, the table inside it. */
    a->header_size = read_u32(table + 8);
    const uint32_t names_at = read_u32(table + 8 + a->count * 8);
    a->names_size = read_u32(table + 12 + a->count * 8);
    free(table);
    if (a->header_size < table_size || !names_at || a->names_size < a->count * AFS_NAME_RECORD) goto none;

    a->header = (uint8_t *)malloc(a->header_size);
    a->names = (uint8_t *)malloc(a->names_size);
    a->entries = (afs_entry *)calloc(a->count, sizeof *a->entries);
    if (!a->header || !a->names || !a->entries) goto none;
    _fseeki64(f, 0, SEEK_SET);
    if (fread(a->header, 1, a->header_size, f) != a->header_size) goto none;
    _fseeki64(f, names_at, SEEK_SET);
    if (fread(a->names, 1, a->names_size, f) != a->names_size) goto none;

    /* Lay the entries out again in order, each from its original or its mod. */
    uint64_t at = a->header_size;
    int replaced = 0;
    for (uint32_t i = 0; i < a->count; i++) {
        char name[AFS_NAME_LEN + 1];
        memcpy(name, a->names + i * AFS_NAME_RECORD, AFS_NAME_LEN);
        name[AFS_NAME_LEN] = '\0';

        afs_entry *e = &a->entries[i];
        uint8_t *slot = a->header + 8 + i * 8;
        e->source = read_u32(slot);
        e->size = read_u32(slot + 4);
        e->part = -1;
        const char *path = find_file(g_entries, g_nentries, name);
        if (path) {
            e->part = get_part(a, path);
            if (e->part >= 0) {
                e->size = (uint32_t)a->parts[e->part].size;
                replaced++;
            }
        }
        e->offset = at;
        if (g_watched_entry && _strnicmp(name, g_watched_entry, AFS_NAME_LEN) == 0) {
            a->watched_at = at;
            a->watched_size = e->size;
        }
        write_u32(slot, (uint32_t)at);
        write_u32(slot + 4, e->size);
        at += sector_up(e->size);
        if (at > 0xFFFFFFFFull) { fprintf(stderr, "mods: %s would pass 4 GB\n", host); goto none; }
    }
    if (!replaced && !a->watched_size) goto none;

    a->names_offset = at;
    write_u32(a->header + 8 + a->count * 8, (uint32_t)at);
    fprintf(stderr, "mods: %s: %d entries replaced, by %d files\n", host, replaced, a->nparts);
    out->size = sector_up(at + a->names_size);
    out->ctx = a;
    out->read = afs_read;
    out->close = afs_close;
    return 1;

none:
    afs_close(a);
    return 0;
}

static const char *after_device(const char *guest) {
    const char *colon = strchr(guest, ':');
    const char *p = colon ? colon + 1 : guest;
    while (*p == '/' || *p == '\\') p++;
    return p;
}

static int on_open(const char *guest, const char *host, psp_io_virtual *out) {
    if (_strnicmp(guest, "disc0:", 6) != 0 && _strnicmp(guest, "umd0:", 5) != 0) return 0;

    /* Loose files first: a mod may even replace a whole archive this way. Paths can
     * come with a "./" in them, which a name search would miss. */
    char rel[512];
    const char *p = after_device(guest);
    size_t n = 0;
    while (*p && n + 1 < sizeof rel) {
        if (p[0] == '.' && (p[1] == '/' || p[1] == '\\')) { p += 2; continue; }
        rel[n++] = *p == '\\' ? '/' : *p;
        p++;
    }
    rel[n] = '\0';
    const char *path = find_file(g_loose, g_nloose, rel);
    if (path) {
        FILE *f = fopen(path, "rb");
        if (!f) return 0;
        fprintf(stderr, "mods: %s -> %s\n", guest, path);
        out->size = file_size(f);
        out->ctx = f;
        out->read = file_read;
        out->close = file_close;
        return 1;
    }

    if (n > 4 && _stricmp(rel + n - 4, ".afs") == 0 && (g_nentries || g_watched_entry)) return open_afs(host, out);
    return 0;
}

int mods_init(const char *dir) {
    char pattern[1100];
    WIN32_FIND_DATAA found;
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    HANDLE h = FindFirstFileA(pattern, &found);
    int mods = 0;
    if (h != INVALID_HANDLE_VALUE) {
        /* NTFS lists names in order, so a later name overrides an earlier one. */
        do {
            if (found.cFileName[0] == '.' || !(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            /* Skins are choices of their own, not replacements: see note_skins.c. */
            if (_stricmp(found.cFileName, "notes") == 0 || _stricmp(found.cFileName, "modules") == 0) continue;
            char path[1100];
            snprintf(path, sizeof path, "%s\\%s", dir, found.cFileName);
            scan_dir(path, NULL, 1);
            fprintf(stderr, "mods: loaded \"%s\"\n", found.cFileName);
            mods++;
        } while (FindNextFileA(h, &found));
        FindClose(h);
    }
    fprintf(stderr, "mods: %d mods in %s, %d archive entries and %d disc files\n", mods, dir, g_nentries, g_nloose);
    if (g_nentries || g_nloose) psp_io_set_open_hook(on_open);
    return g_nentries + g_nloose;
}

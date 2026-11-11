/* Module skins: looks from the mods folder, added to the module screen's list after the game's own.
 *
 *     mods\modules\<name>\body.igb    the body, as the game's skinNN.igb
 *     mods\modules\<name>\head.igb    the head, as skinNN_head.igb (optional: the base's own if left out)
 *     mods\modules\<name>\skin.ini    optional: "base = N", the module it is worn over (0 if left out)
 *
 * A skin is worn over a module of the game's, its base: the game believes that module worn, with
 * its motions and everything else, and only its two models are taken from the skin. They are added
 * to DivaDataList.afs as entries past its end (mods.c), and the game's one loader of models (LOADER)
 * is given their numbers in place of the base's: 142 and 308 past the module, body and head.
 *
 * The list: Module Convert's list has a row for each of the game's 53 modules, its number the
 * module's, in a list with room for 64. As module_sort.c finds it on the screen, the skins' rows are
 * added after, each with its base's number, and the row the cursor is on is followed. The screen
 * changes Miku only into a module other than the one worn (CHANGE_ASKED): a skin over it, or the
 * module itself under a skin, is let through as a change. The screen then puts on the module
 * (ROOM_WEARS, whose loading comes to LOADER), and the skin of the cursor's row is loaded in its
 * place. The skin is kept as worn if its base is worn as the screen closes, and in OPTIONS.INI by
 * its name. Miku's Room makes its own list of the modules (AFTER_ROWS, MODULE_AT, DRESS_MIKU).
 *
 * Addresses are for ULJM05472 v1.01, which Tools\build.ps1 checks.
 */
#include "module_skins.h"

#include "guest.h"
#include "language.h"
#include "mods.h"
#include "model.h"
#include "model_convert.h"
#include "options.h"
#include "pictures.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define MODULE_AT 0x088628A8u       /* (place): the module at that place of the module screen */
#define MODULE_ORDER 0x08B2727Cu
#define MODULES 53
#define AFTER_ROWS 0x08A8C1A8u      /* in the module screen's opening, after its 53 rows are made */
#define LIST_HOLDER 0x08848C84u     /* () -> what LIST_OF takes */
#define LIST_OF 0x08AFC55Cu         /* (holder, 0, 0): the module screen's list */
#define ADD_ROW 0x08AF2840u         /* (list, words, value): a row, if the list has room */
#define DRESS_MIKU 0x08A94C70u      /* (miku, module) */
#define DRESSED_AS 0x10D18u         /* where a Miku keeps the module she is dressed in */
#define LOADER 0x0885BBD0u          /* (loader, body entry, motion entry, head entry, ...) */
#define BODY_ENTRY 142u
#define HEAD_ENTRY 308u
#define CHANGE_ASKED 0x08AC1F0Cu    /* in the module screen's (screen, module): its check against the one worn */
#define SAVE 0x08C2C4A0u
#define WORN 4u

/* Where MODULE_AT is asked from on the module screen as a row is confirmed, and DRESS_MIKU after. */
#define CHOSEN_AT_1 0x08A8C2E0u
#define CHOSEN_AT_2 0x08A8C320u
#define CHOSEN_AT_3 0x08A8C40Cu
#define DRESSED_CHOSEN_1 0x08A8C2ECu
#define DRESSED_CHOSEN_2 0x08A8C424u
#define DRESSED_CANCEL 0x08A8C368u
#define LOADED_FOR_MENU 0x08A94DA4u

#define LIST_CREATE 0x08AF2344u     /* (list, room): every window list is made with room for 64 */
#define LIST_ROOM 128u              /* the room given instead when there are skins past the 11 that fit in 64 */
#define MAX_SKINS 75                /* the list's room past the game's 53 rows */
#define LIST_COUNT 4u
#define LIST_ROW 16u
#define LIST_ROWS 20u
#define ROW_SIZE 24u

typedef struct {
    char name[48];
    int base;
    char body[1200], head[1200];    /* its files; no head for the base's own */
    uint32_t words;                 /* the name, in the game's memory, for its row */
} skin;

static skin g_skins[MAX_SKINS];
static int g_count;
static int g_chosen = -1;           /* on the module screen: the skin of the row confirmed last */
static int g_worn = -1;             /* the skin worn, while its base is */
static int g_dressed = -1;          /* the skin the menu's Miku was last dressed in */
static int g_on_screen;
static int g_cursor = -1;           /* on the module screen: the skin of the row the cursor is on */

static int exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

/* ---- the game's files a conversion starts from, out of DivaDataList.afs ---------------------- */

#define ANIM_ENTRY_NAME "song01_anim_bod"   /* a song's motions, with Miku's skeleton */

static uint8_t *afs_entry(FILE *f, int index, const char *name, size_t *size) {
    uint8_t head[8];
    if (fseek(f, 0, SEEK_SET) || fread(head, 1, 8, f) != 8 || memcmp(head, "AFS", 3)) return NULL;
    const uint32_t count = head[4] | head[5] << 8 | head[6] << 16 | (uint32_t)head[7] << 24;
    uint8_t *table = (uint8_t *)malloc(count * 8u + 8u);
    if (!table || fread(table, 8, count + 1, f) != count + 1) { free(table); return NULL; }
#define U32(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[3] << 24)
    if (name) {
        index = -1;
        const uint32_t names_at = U32(table + count * 8);
        uint8_t record[48];
        for (uint32_t i = 0; i < count && index < 0; i++)
            if (!fseek(f, (long)(names_at + i * 48), SEEK_SET) && fread(record, 1, 48, f) == 48 && !strncmp((const char *)record, name, strlen(name))) index = (int)i;
    }
    uint8_t *data = NULL;
    if (index >= 0 && (uint32_t)index < count) {
        const uint32_t at = U32(table + index * 8), n = U32(table + index * 8 + 4);
        data = (uint8_t *)malloc(n ? n : 1);
        if (data && (fseek(f, (long)at, SEEK_SET) || fread(data, 1, n, f) != n)) { free(data); data = NULL; }
        *size = n;
    }
#undef U32
    free(table);
    return data;
}

static model_templates g_templates;
static int g_templates_read;

static int templates(const char *disc_dir) {
    if (g_templates_read) return g_templates.body && g_templates.head && g_templates.anim;
    g_templates_read = 1;
    char path[1100];
    snprintf(path, sizeof path, "%s\\PSP_GAME\\USRDIR\\media\\afs\\DivaDataList.afs", disc_dir);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    g_templates.body = afs_entry(f, (int)BODY_ENTRY, NULL, &g_templates.body_size);
    g_templates.head = afs_entry(f, (int)HEAD_ENTRY, NULL, &g_templates.head_size);
    g_templates.anim = afs_entry(f, -1, ANIM_ENTRY_NAME, &g_templates.anim_size);
    fclose(f);
    return g_templates.body && g_templates.head && g_templates.anim;
}

/* ---- finding a model's picture -------------------------------------------------------------- */

static void stem(const char *path, char *out, size_t size) {
    const char *base = path;
    for (const char *p = path; *p; p++)
        if (*p == '\\' || *p == '/') base = p + 1;
    snprintf(out, size, "%s", base);
    char *dot = strrchr(out, '.');
    if (dot) *dot = 0;
}

static int is_picture(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot && (!_stricmp(dot, ".png") || !_stricmp(dot, ".jpg") || !_stricmp(dot, ".jpeg") || !_stricmp(dot, ".bmp") || !_stricmp(dot, ".gif") ||
                   !_stricmp(dot, ".tga"));
}

static const char *stristr(const char *hay, const char *needle) {
    const size_t n = strlen(needle);
    for (; *hay; hay++)
        if (!_strnicmp(hay, needle, n)) return hay;
    return NULL;
}

typedef struct {
    char path[1100];
    int depth;
} candidate;

static void gather(const char *dir, int depth, candidate *out, int *count, int most) {
    char pattern[1200];
    snprintf(pattern, sizeof pattern, "%s\\*", dir);
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (found.cFileName[0] == '.') continue;
        char path[1100];
        snprintf(path, sizeof path, "%s\\%s", dir, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < 3) gather(path, depth + 1, out, count, most);
        } else if (is_picture(found.cFileName) && *count < most) {
            snprintf(out[*count].path, sizeof out[0].path, "%s", path);
            out[(*count)++].depth = depth;
        }
    } while (FindNextFileA(h, &found));
    FindClose(h);
}

/* The picture a model shows, in the mod's folder: the one named after the model's own part (its
 * name less what it shares with the folder's: "Robe" for HerRobe in Heroine), as files can name
 * another; else the one the file names, by name; else one whose name is within the model's file
 * name; else one whose name holds the named one; else the only one. Nearer the folder's top is
 * preferred among equals. */
static int find_picture(const char *dir, const char *model_path, const char *own, const char *named, char *out, size_t size) {
    static candidate c[256];
    int n = 0;
    gather(dir, 0, c, &n, 256);
    char want[300], model_stem[300], cand[300];
    stem(named, want, sizeof want);
    stem(model_path, model_stem, sizeof model_stem);
    int best = -1, best_score = 0;
    for (int i = 0; i < n; i++) {
        stem(c[i].path, cand, sizeof cand);
        int score = 0;
        if (own[0] && !_stricmp(cand, own)) score = 4000;
        else if (want[0] && !_stricmp(cand, want)) score = 3000;
        else if (cand[0] && stristr(model_stem, cand)) score = 2000 + (int)strlen(cand);
        else if (want[0] && stristr(cand, want)) score = 1000 - (int)strlen(cand);
        if (!score) continue;
        score -= c[i].depth * 5;
        if (score > best_score) { best_score = score; best = i; }
    }
    if (best < 0 && n == 1) best = 0;
    if (best < 0) return 0;
    snprintf(out, size, "%s", c[best].path);
    return 1;
}

/* ---- converting, once, into a cache ---------------------------------------------------------- */

#define CONVERTER_VERSION 14
#define BOTH_ROOM 1450000u         /* body and head together: 985 + 501 KB loaded, 1030 + 501 and 675 + 940 KB did not */
#define BODY_ROOM 960000u          /* a body file's most bytes: each file has room of about 1 MB (985 KB loaded; 1.03 MB, and 1.4 MB with a head of 24 KB, wrecked the memory) */
#define DEFAULT_TRIANGLES 10000

static void file_mark(const char *path, char *out, size_t size) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a)) { snprintf(out, size, "none"); return; }
    snprintf(out, size, "%lu-%lu-%lu", a.nFileSizeLow, a.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwLowDateTime);
}

static int write_file(const char *path, const uint8_t *data, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const int ok = fwrite(data, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

/* Converts the model into cache\body.igb and head.igb unless they were made from it as it is.
 * made_from.txt: the converter's version and the model's and skin.ini's marks on its first line,
 * then each picture used, its mark and path. */
static int convert(const char *model_path, const char *dir, const char *cache, const char *disc_dir, const char *name, const char *own,
                   const char *chosen) {
    char stamp_path[1200], ini[1200], mark_m[64], mark_i[64], line[1400];
    snprintf(stamp_path, sizeof stamp_path, "%s\\made_from.txt", cache);
    snprintf(ini, sizeof ini, "%s\\skin.ini", dir);
    file_mark(ini, mark_i, sizeof mark_i);
    file_mark(model_path, mark_m, sizeof mark_m);
    FILE *f = fopen(stamp_path, "r");
    if (f) {
        int same = 0, version = 0;
        char old_mark[64] = "", old_imark[64] = "";
        if (fgets(line, sizeof line, f) && sscanf(line, "%d %63s %63s", &version, old_mark, old_imark) == 3 && version == CONVERTER_VERSION &&
            !strcmp(old_mark, mark_m) && !strcmp(old_imark, mark_i)) {
            same = 1;
            char pmark[64], ppath[1100], now[64];
            while (same && fgets(line, sizeof line, f))
                if (sscanf(line, "%63s %1099[^\n]", pmark, ppath) == 2) {
                    file_mark(ppath, now, sizeof now);
                    if (strcmp(now, pmark)) same = 0;
                }
        }
        fclose(f);
        if (same) return 1;
    }
    if (!templates(disc_dir)) {
        fprintf(stderr, "module skins: \"%s\": the game's own module files could not be read\n", name);
        return 0;
    }
    model m;
    char err[300];
    const char *dot = strrchr(model_path, '.');
    const int fbx = dot && !_stricmp(dot, ".fbx"), pmx_file = dot && !_stricmp(dot, ".pmx");
    if (!(fbx ? model_load_fbx(model_path, &m, err, sizeof err) : pmx_file ? model_load_pmx(model_path, &m, err, sizeof err) : 0)) {
        fprintf(stderr, "module skins: \"%s\": %s\n", name, fbx || pmx_file ? err : "not a model this can read");
        return 0;
    }
    /* At most so many triangles (skin.ini's "triangles = N"): the game's memory for models is small. */
    int most = DEFAULT_TRIANGLES;
    FILE *fi = fopen(ini, "r");
    if (fi) {
        char l[256];
        int n;
        while (fgets(l, sizeof l, fi))
            if (sscanf(l, " triangles = %d", &n) == 1 && n >= 500) most = n;
        fclose(fi);
    }
    const int had = m.ncorners / 3;
    if (had > most) fprintf(stderr, "module skins: \"%s\": %d triangles made %d\n", name, had, model_simplify(&m, most));
    /* A picture for each material: skin.ini's or the model's own part's for the first, then each by
     * the name its material gives. Materials naming the same file share it. */
    const int nmat = m.nmaterials > 0 ? m.nmaterials : 1;
    model_picture *pics = (model_picture *)calloc((size_t)nmat, sizeof *pics);
    char (*paths)[1100] = calloc((size_t)nmat, sizeof *paths);
    for (int i = 0; i < m.nmaterials; i++) {
        /* The path the model gives, from its folder, when it is there (as PMX files give them). */
        char relative[1100];
        snprintf(relative, sizeof relative, "%s\\%s", dir, m.picture[i]);
        for (char *c = relative; *c; c++)
            if (*c == '/') *c = '\\';
        const int given = m.picture[i][0] && !strchr(m.picture[i], ':') && m.picture[i][0] != '\\' && exists(relative);
        if (i == 0 && chosen[0]) snprintf(paths[0], sizeof paths[0], "%s\\%s", dir, chosen);
        else if (given && !(i == 0 && own[0])) snprintf(paths[i], sizeof paths[i], "%s", relative);
        else if (!find_picture(dir, model_path, i == 0 ? own : "", m.picture[i], paths[i], sizeof paths[i])) paths[i][0] = 0;
        int earlier = -1;
        for (int j = 0; j < i && earlier < 0; j++)
            if (paths[i][0] && !_stricmp(paths[i], paths[j])) earlier = j;
        if (earlier >= 0) pics[i] = pics[earlier];
        else if (paths[i][0]) pics[i].pixels = picture_read(paths[i], &pics[i].w, &pics[i].h);
        if (!pics[i].pixels) fprintf(stderr, "module skins: \"%s\": no picture found for material %d (it names \"%s\")\n", name, i, m.picture[i]);
    }
    uint8_t *body = NULL, *head = NULL;
    size_t bn = 0, hn = 0;
    int ok = model_convert(&m, pics, &g_templates, &body, &bn, &head, &hn, err, sizeof err);
    /* The game reads each file into room of about 1 MB, and both into about 1.5 MB: past either,
     * fewer triangles, until they fit. */
#define TOO_BIG(b, h) ((b) > BODY_ROOM || (h) > BODY_ROOM || (b) + (h) > BOTH_ROOM)
    for (int tries = 0; ok && TOO_BIG(bn, hn) && tries < 6; tries++) {
        double fit = (double)BODY_ROOM / (double)(bn > hn ? bn : hn);
        if ((double)BOTH_ROOM / (double)(bn + hn) < fit) fit = (double)BOTH_ROOM / (double)(bn + hn);
        const int now = m.ncorners / 3, want = (int)((double)now * fit * 0.9);
        if (want < 300) break;
        fprintf(stderr, "module skins: \"%s\": body %zu and head %zu bytes, too big for the game; %d triangles made %d\n", name, bn, hn, now,
                model_simplify(&m, want));
        free(body);
        free(head);
        body = head = NULL;
        ok = model_convert(&m, pics, &g_templates, &body, &bn, &head, &hn, err, sizeof err);
    }
    if (ok && TOO_BIG(bn, hn)) {
        snprintf(err, sizeof err, "still %zu and %zu bytes after making it simpler; its pictures may be too many", bn, hn);
        free(body);
        free(head);
        ok = 0;
    }
    const int materials = m.nmaterials;
    model_free(&m);
    for (int i = 0; i < materials; i++) {
        int earlier = 0;
        for (int j = 0; j < i; j++)
            if (pics[j].pixels == pics[i].pixels) earlier = 1;
        if (!earlier) free((void *)pics[i].pixels);
    }
    free(pics);
    if (!ok) {
        fprintf(stderr, "module skins: \"%s\": %s\n", name, err);
        free(paths);
        return 0;
    }
    char path[1200];
    CreateDirectoryA(cache, NULL);
    snprintf(path, sizeof path, "%s\\body.igb", cache);
    int written = write_file(path, body, bn);
    snprintf(path, sizeof path, "%s\\head.igb", cache);
    written = written && write_file(path, head, hn);
    free(body);
    free(head);
    if (written && (f = fopen(stamp_path, "w"))) {
        fprintf(f, "%d %s %s\n", CONVERTER_VERSION, mark_m, mark_i);
        for (int i = 0; i < materials; i++)
            if (paths[i][0]) {
                char mark[64];
                file_mark(paths[i], mark, sizeof mark);
                fprintf(f, "%s %s\n", mark, paths[i]);
            }
        fclose(f);
    }
    const char *model_file = strrchr(model_path, '\\'), *picture_file = materials && paths[0][0] ? strrchr(paths[0], '\\') : NULL;
    fprintf(stderr, "module skins: \"%s\" converted from %s, %d material%s%s%s\n", name, model_file ? model_file + 1 : model_path, materials,
            materials == 1 ? "" : "s", picture_file ? ", first with " : "", picture_file ? picture_file + 1 : "");
    free(paths);
    return written;
}

/* ---- the skins found ------------------------------------------------------------------------ */

static void add_skin(const char *name, const char *body, const char *head, int base) {
    if (g_count == MAX_SKINS) {
        fprintf(stderr, "module skins: only %d fit in the list, \"%s\" left out\n", MAX_SKINS, name);
        return;
    }
    skin *s = &g_skins[g_count];
    snprintf(s->name, sizeof s->name, "%s", name);
    s->base = base;
    snprintf(s->body, sizeof s->body, "%s", body);
    snprintf(s->head, sizeof s->head, "%s", exists(head) ? head : "");
    fprintf(stderr, "module skins: \"%s\" over module %d%s\n", s->name, s->base, s->head[0] ? "" : ", its head the base's");
    g_count++;
}

/* The archive's room for them: one entry for the body and one for the head, each as big as the
 * biggest, given a skin's files as it is loaded. Only two: the game keeps something for each entry
 * by its number with room for two past its own 1757, and entries further on wreck its memory.
 * The loader does not load again the body it holds (LOADER_BODY), so when another skin's files
 * come under the same number, it is made to forget it. */
#define LOADER_BODY 628u
static int g_slot_body = -1, g_slot_head = -1, g_slot_skin = -1;

static uint32_t file_size_of(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    return path[0] && GetFileAttributesExA(path, GetFileExInfoStandard, &a) ? a.nFileSizeLow : 0;
}

static void make_room(void) {
    uint32_t body = 0, head = 0;
    for (int i = 0; i < g_count; i++) {
        const uint32_t b = file_size_of(g_skins[i].body), h = file_size_of(g_skins[i].head);
        if (b > body) body = b;
        if (h > head) head = h;
    }
    if (!g_count) return;
    g_slot_body = mods_add_entry("skinmod.igb", body);
    g_slot_head = mods_add_entry("skinmod_head.igb", head ? head : 4);
}

/* A skin's name for one of several models in a folder: the folder's, then the model's, without
 * what it shares with the folder's at its start ("Heroine" and "HerWinterCoat": "Heroine WinterCoat"). */
static const char *own_part(const char *folder, const char *model_stem) {
    size_t common = 0;
    while (folder[common] && model_stem[common] && tolower((unsigned char)folder[common]) == tolower((unsigned char)model_stem[common])) common++;
    return common >= 3 && model_stem[common] ? model_stem + common : model_stem;
}

static void model_skin_name(const char *folder, const char *model_stem, char *out, size_t size) {
    snprintf(out, size, "%s %s", folder, own_part(folder, model_stem));
}

/* skin.ini's "picture = file" for a folder of one model, or "picture <model> = file" for one of several. */
static void chosen_picture(const char *ini, const char *model_stem, int only, char *out, size_t size) {
    out[0] = 0;
    FILE *f = fopen(ini, "r");
    if (!f) return;
    char line[600], key[300], value[520];
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, " picture %299[^=] = %519[^\r\n]", key, value) == 2) {
            char *end = key + strlen(key);
            while (end > key && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
            if (!_stricmp(key, model_stem) || (only && !key[0])) snprintf(out, size, "%s", value);
        } else if (only && sscanf(line, " picture = %519[^\r\n]", value) == 1) {
            snprintf(out, size, "%s", value);
        }
    }
    fclose(f);
}

int module_skins_scan(const char *mods_dir, const char *disc_dir, const char *data_dir) {
    char pattern[1200], cache_root[1100];
    snprintf(pattern, sizeof pattern, "%s\\modules\\*", mods_dir);
    snprintf(cache_root, sizeof cache_root, "%s\\..\\modules_cache", data_dir);
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.' || found.cFileName[0] == '_') continue;
        char dir[1100], body[1200], head[1200], ini[1200];
        snprintf(dir, sizeof dir, "%s\\modules\\%s", mods_dir, found.cFileName);
        snprintf(body, sizeof body, "%s\\body.igb", dir);
        snprintf(head, sizeof head, "%s\\head.igb", dir);
        snprintf(ini, sizeof ini, "%s\\skin.ini", dir);
        int base = 0;
        FILE *f = fopen(ini, "r");
        if (f) {
            char line[256];
            int n;
            while (fgets(line, sizeof line, f))
                if (sscanf(line, " base = %d", &n) == 1 && n >= 0 && n < MODULES) base = n;
            fclose(f);
        }
        if (exists(body)) {
            add_skin(found.cFileName, body, head, base);
            continue;
        }
        /* Models in it, each a skin. */
        char models[32][300];
        int nmodels = 0;
        char inner[1200];
        snprintf(inner, sizeof inner, "%s\\*", dir);
        WIN32_FIND_DATAA mf;
        HANDLE mh = FindFirstFileA(inner, &mf);
        if (mh != INVALID_HANDLE_VALUE) {
            do {
                const char *ext = strrchr(mf.cFileName, '.');
                if (!(mf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ext && (!_stricmp(ext, ".fbx") || !_stricmp(ext, ".pmx")) && nmodels < 32)
                    snprintf(models[nmodels++], sizeof models[0], "%s", mf.cFileName);
            } while (FindNextFileA(mh, &mf));
            FindClose(mh);
        }
        if (nmodels) CreateDirectoryA(cache_root, NULL);
        for (int i = 0; i < nmodels; i++) {
            char model_path[1200], name[48], st[300], cache[1300], cbody[1400], chead[1400];
            snprintf(model_path, sizeof model_path, "%s\\%s", dir, models[i]);
            stem(models[i], st, sizeof st);
            if (nmodels == 1) snprintf(name, sizeof name, "%s", found.cFileName);
            else model_skin_name(found.cFileName, st, name, sizeof name);
            snprintf(cache, sizeof cache, "%s\\%s", cache_root, name);
            char chosen[520];
            chosen_picture(ini, st, nmodels == 1, chosen, sizeof chosen);
            if (!convert(model_path, dir, cache, disc_dir, name, nmodels == 1 ? "" : own_part(found.cFileName, st), chosen)) continue;
            snprintf(cbody, sizeof cbody, "%s\\body.igb", cache);
            snprintf(chead, sizeof chead, "%s\\head.igb", cache);
            add_skin(name, cbody, chead, base);
        }
    } while (FindNextFileA(h, &found));
    FindClose(h);
    make_room();
    /* The skin worn last time, by name. */
    for (int i = 0; i < g_count; i++)
        if (g_options.module_skin[0] && _stricmp(g_options.module_skin, g_skins[i].name) == 0) g_worn = i;
    return g_count;
}

static int place_of(int module) {
    for (int p = 0; p < MODULES; p++)
        if ((int)psp_read32(MODULE_ORDER + (uint32_t)p * 4) == module) return p;
    return 0;
}

static int worn_over(int module) {
    if (g_worn < 0) return -1;
    return g_skins[g_worn].base == module && (int)psp_read32(SAVE + WORN) == module ? g_worn : -1;
}

static void keep_worn(int s) {
    if (s == g_worn) return;
    g_worn = s;
    snprintf(g_options.module_skin, sizeof g_options.module_skin, "%s", s >= 0 ? g_skins[s].name : "");
    options_save(language_data_dir());
    fprintf(stderr, "module skins: wearing %s\n", s >= 0 ? g_skins[s].name : "the module itself");
}

/* Its row's words, in the game's memory once it runs: ASCII, which the game's letters have. */
static uint32_t words_of(skin *s) {
    if (s->words) return s->words;
    const size_t len = strlen(s->name);
    s->words = psp_sysmem_alloc((uint32_t)len + 1, 1);
    for (size_t i = 0; s->words && i <= len; i++) {
        const unsigned char c = (unsigned char)s->name[i];
        psp_write8(s->words + (uint32_t)i, c && (c < 32 || c > 126) ? '?' : c);
    }
    return s->words;
}

int module_skins_row(uint32_t words) {
    for (int i = 0; i < g_count; i++)
        if (words && g_skins[i].words == words) return i;
    return -1;
}

/* The skins' rows, after the game's, each with its base's number as the game's rows have theirs. */
static void add_rows(uint32_t list) {
    if (!g_count || !guest_in_ram(list)) return;
    const uint32_t count = psp_read32(list + LIST_COUNT), rows = psp_read32(list + LIST_ROWS);
    if (count > LIST_ROOM || !guest_in_ram(rows)) return;
    for (uint32_t i = 0; i < count; i++)
        if (module_skins_row(psp_read32(rows + i * ROW_SIZE)) >= 0) return;   /* added already */
    for (int i = 0; i < g_count; i++) {
        const uint32_t words = words_of(&g_skins[i]);
        if (!words || !guest_call(ADD_ROW, list, words, (uint32_t)g_skins[i].base, 0)) {
            fprintf(stderr, "module skins: no room in the list (%u rows) for \"%s\"\n", psp_read32(list + LIST_COUNT), g_skins[i].name);
            break;
        }
    }
    fprintf(stderr, "module skins: rows added, the list has %u\n", psp_read32(list + LIST_COUNT));
}

void module_skins_screen(uint32_t list) {
    const int open = list != 0;
    if (open && !g_on_screen) {
        add_rows(list);
        g_chosen = worn_over((int)psp_read32(SAVE + WORN));
    }
    if (open) {
        /* The skin of the row the cursor is on, if it is one. */
        const uint32_t cursor = psp_read32(list + LIST_ROW), rows = psp_read32(list + LIST_ROWS);
        g_cursor = cursor < psp_read32(list + LIST_COUNT) && guest_in_ram(rows) ? module_skins_row(psp_read32(rows + cursor * ROW_SIZE)) : -1;
    }
    if (open == g_on_screen) return;
    g_on_screen = open;
    if (open) return;
    /* Closed: what was confirmed is worn, if its base is. */
    const int worn = (int)psp_read32(SAVE + WORN);
    keep_worn(g_chosen >= 0 && g_skins[g_chosen].base == worn ? g_chosen : -1);
}

static void on_module_at(void) {
    const uint32_t place = psp_cpu.r[PSP_REG_A0], from = psp_cpu.r[PSP_REG_RA];
    int s = -1;
    if (place >= MODULES && place < (uint32_t)(MODULES + g_count)) {
        s = (int)place - MODULES;
        psp_cpu.r[PSP_REG_A0] = (uint32_t)place_of(g_skins[s].base);
    }
    if (from == CHOSEN_AT_1 || from == CHOSEN_AT_2 || from == CHOSEN_AT_3) g_chosen = s;
}

/* On the module screen, Miku is dressed in the row confirmed: a skin's if the cursor is on one.
 * Elsewhere, in the skin worn if she is dressed in its base. */
static void on_dress(void) {
    const uint32_t miku = psp_cpu.r[PSP_REG_A0], from = psp_cpu.r[PSP_REG_RA];
    const int module = (int)psp_cpu.r[PSP_REG_A1];
    int s;
    if (g_on_screen) s = g_chosen = g_cursor;
    else if (from == DRESSED_CHOSEN_1 || from == DRESSED_CHOSEN_2) s = g_chosen;
    else if (from == DRESSED_CANCEL) s = g_chosen = worn_over(module);
    else s = worn_over(module);
    if (s >= 0 && g_skins[s].base != module) s = -1;
    if (g_on_screen) g_chosen = s;
    /* She is not dressed again in the module she wears; a change of skin alone must be. */
    if (s != g_dressed && guest_in_ram(miku + DRESSED_AS)) psp_write32(miku + DRESSED_AS, 0xFFFFFFFFu);
    if (s >= 0) fprintf(stderr, "module skins: Miku dressed in module %d%s%s (from %08X)\n", module, s >= 0 ? ", skin " : "", s >= 0 ? g_skins[s].name : "", from);
    g_dressed = s;
}

static void on_load(void) {
    const uint32_t from = psp_cpu.r[PSP_REG_RA];
    const int module = (int)psp_cpu.r[PSP_REG_A1] - (int)BODY_ENTRY;
    int s;
    if (g_on_screen) {
        /* The module screen puts on the row confirmed (ROOM_WEARS): a skin's if the cursor is on one. */
        s = g_cursor >= 0 && g_skins[g_cursor].base == module ? g_cursor : -1;
        g_chosen = s;
        g_dressed = s;
    } else {
        s = from == LOADED_FOR_MENU ? g_dressed : worn_over(module);
    }
    if (s < 0 || g_skins[s].base != module) return;
    const int body = mods_added_entry(g_slot_body), head = g_skins[s].head[0] ? mods_added_entry(g_slot_head) : -1;
    const uint32_t loader = psp_cpu.r[PSP_REG_A0];
    if (g_slot_skin != s) {
        g_slot_skin = s;
        mods_set_entry_file(g_slot_body, g_skins[s].body);
        mods_set_entry_file(g_slot_head, g_skins[s].head);
    }
    /* Each loader keeps the number it last loaded; the files under it may be another skin's now. */
    static uint32_t loaders[8];
    static int loader_skin[8];
    int at = -1;
    for (int i = 0; i < 8 && at < 0; i++)
        if (loaders[i] == loader) at = i;
    if (at < 0)
        for (int i = 0; i < 8 && at < 0; i++)
            if (!loaders[i]) { at = i; loaders[i] = loader; loader_skin[i] = -1; }
    if (at >= 0 && loader_skin[at] != s && guest_in_ram(loader + LOADER_BODY) && (int)psp_read32(loader + LOADER_BODY) == body)
        psp_write32(loader + LOADER_BODY, 0xFFFFFFFFu);
    if (at >= 0) loader_skin[at] = s;
    if (body >= 0) psp_cpu.r[PSP_REG_A1] = (uint32_t)body;
    if (head >= 0) psp_cpu.r[PSP_REG_A3] = (uint32_t)head;
    static int told = -1;
    if (told != s) {
        told = s;
        fprintf(stderr, "module skins: models of \"%s\", entries %d and %d\n", g_skins[s].name, body, head);
    }
}

/* The module screen changes Miku only into a module other than the one worn (CHANGE_ASKED, the
 * module in s0 and the one worn just read in v0). A skin over the module worn, or the module itself
 * when a skin over it is worn, is a change too. */
static void on_change_asked(void) {
    const int module = (int)psp_cpu.r[PSP_REG_S0], worn = (int)psp_cpu.r[PSP_REG_V0];
    if (!g_on_screen || module != worn) return;
    const int wearing = worn_over(worn);
    const int wanted = g_cursor >= 0 && g_skins[g_cursor].base == module ? g_cursor : -1;
    if (wanted == wearing) return;
    psp_cpu.r[PSP_REG_V0] = 0xFFFFFFFEu;
    fprintf(stderr, "module skins: changing into %s\n", wanted >= 0 ? g_skins[wanted].name : "the module itself");
}

/* The window manager makes every list with room for 64 rows (LIST_CREATE, as its windows open):
 * more, if the module list needs it for the skins. */
static void on_list_create(void) {
    if (psp_cpu.r[PSP_REG_A1] == 64 && g_count > 64 - MODULES) psp_cpu.r[PSP_REG_A1] = LIST_ROOM;
}

int module_skins_hook(uint32_t addr) {
    switch (addr) {
    case LIST_CREATE: on_list_create(); return 1;
    case CHANGE_ASKED: on_change_asked(); return 1;
    case MODULE_AT: on_module_at(); return 1;
    case AFTER_ROWS: add_rows(guest_call(LIST_OF, guest_call(LIST_HOLDER, 0, 0, 0, 0), 0, 0, 0)); return 1;
    case DRESS_MIKU: on_dress(); return 1;
    case LOADER: on_load(); return 1;
    default: return 0;
    }
}

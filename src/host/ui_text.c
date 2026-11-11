/* The project's own words, read from lang\ (see ui_text.h). Everything shown in a language is in
 * those files, so that a language is added by adding them, and no code. */

#include "ui_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    char *key, *utf8;
    wchar_t *wide;      /* made when first asked for */
    char *game;
} word;

typedef struct {
    char code[8];
    wchar_t *name;
    word *words;
    int count, room;
} language;

static language g_language[UI_TEXT_MOST_LANGUAGES];
static int g_languages, g_current, g_english;
static wchar_t g_letter[256];     /* the letter each byte of the game's font stands for, if any */

static char *trimmed_copy(const char *s, size_t n) {
    while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) n--;
    char *c = (char *)malloc(n + 1);
    if (!c) return NULL;
    memcpy(c, s, n);
    c[n] = '\0';
    return c;
}

static wchar_t *to_wide(const char *utf8) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *w = (wchar_t *)malloc((size_t)(n > 0 ? n : 1) * sizeof(wchar_t));
    if (!w) return NULL;
    if (n <= 0 || !MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, n)) w[0] = 0;
    return w;
}

static FILE *open_in(const char *dir, const char *name) {
    char path[1200];
    snprintf(path, sizeof path, "%s\\%s", dir, name);
    return fopen(path, "rb");
}

/* Its lines, without a UTF-8 file's mark, comments and blank lines left out. */
static int next_line(FILE *f, char *line, size_t size) {
    while (fgets(line, (int)size, f)) {
        if (!memcmp(line, "\xEF\xBB\xBF", 3)) memmove(line, line + 3, strlen(line + 3) + 1);
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] && line[0] != '#') return 1;
    }
    return 0;
}

static void read_letters(const char *dir) {
    FILE *f = open_in(dir, "letters.txt");
    if (!f) {
        fprintf(stderr, "ui text: no letters.txt in %s\n", dir);
        return;
    }
    char line[256];
    int n = 0;
    while (next_line(f, line, sizeof line)) {
        char *end;
        const unsigned long byte = strtoul(line, &end, 16);
        wchar_t w[4];
        if (byte < 0xA1 || byte > 0xDF || *end != ' ') continue;
        if (MultiByteToWideChar(CP_UTF8, 0, end + 1, -1, w, 4) >= 2) {
            g_letter[byte] = w[0];
            n++;
        }
    }
    fclose(f);
    fprintf(stderr, "ui text: %d letters of letters.txt\n", n);
}

static void read_words(const char *dir, language *l) {
    char name[32];
    snprintf(name, sizeof name, "%s_ui.txt", l->code);
    FILE *f = open_in(dir, name);
    if (!f) {
        fprintf(stderr, "ui text: no %s\n", name);
        return;
    }
    char line[2048];
    while (next_line(f, line, sizeof line)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        char *key_end = eq;
        while (key_end > line && key_end[-1] == ' ') key_end--;
        const char *text = eq[1] == ' ' ? eq + 2 : eq + 1;
        if (l->count == l->room) {
            const int room = l->room ? l->room * 2 : 256;
            word *grown = (word *)realloc(l->words, (size_t)room * sizeof(word));
            if (!grown) break;
            l->words = grown;
            l->room = room;
        }
        word *w = &l->words[l->count];
        memset(w, 0, sizeof *w);
        w->key = trimmed_copy(line, (size_t)(key_end - line));
        w->utf8 = trimmed_copy(text, strlen(text));
        if (w->key && w->utf8) l->count++;
    }
    fclose(f);
    fprintf(stderr, "ui text: %s, %d words\n", name, l->count);
}

void ui_text_start(const char *lang_dir) {
    read_letters(lang_dir);
    FILE *f = open_in(lang_dir, "languages.txt");
    char line[256];
    while (f && g_languages < UI_TEXT_MOST_LANGUAGES && next_line(f, line, sizeof line)) {
        char *bar = strchr(line, '|');
        if (!bar) continue;
        language *l = &g_language[g_languages];
        size_t n = (size_t)(bar - line);
        while (n && line[n - 1] == ' ') n--;
        if (!n || n >= sizeof l->code) continue;
        memcpy(l->code, line, n);
        l->code[n] = '\0';
        const char *native = bar + 1;
        while (*native == ' ') native++;
        l->name = to_wide(native);
        read_words(lang_dir, l);
        g_languages++;
    }
    if (f) fclose(f);
    else fprintf(stderr, "ui text: no languages.txt in %s\n", lang_dir);
    if (!g_languages) {
        /* With no files, still a language, so that every word comes back as its key. */
        snprintf(g_language[0].code, sizeof g_language[0].code, "en");
        g_language[0].name = to_wide("English");
        g_languages = 1;
    }
    g_english = ui_text_find("en");
    if (g_english < 0) g_english = 0;
    g_current = g_english;
}

int ui_text_languages(void) { return g_languages; }
const char *ui_text_code(int language) { return language >= 0 && language < g_languages ? g_language[language].code : "en"; }
const wchar_t *ui_text_native_name(int language) {
    return language >= 0 && language < g_languages && g_language[language].name ? g_language[language].name : L"";
}

int ui_text_find(const char *code) {
    for (int i = 0; i < g_languages; i++)
        if (!strcmp(g_language[i].code, code)) return i;
    return -1;
}

void ui_text_set_language(const char *code) {
    const int i = ui_text_find(code);
    g_current = i >= 0 ? i : g_english;
}

int ui_text_language(void) { return g_current; }

static word *find_word(int n, const char *key) {
    if (n < 0 || n >= g_languages) n = g_english;
    language *l = &g_language[n];
    for (int i = 0; i < l->count; i++)
        if (!strcmp(l->words[i].key, key)) return &l->words[i];
    if (n != g_english) return find_word(g_english, key);
    return NULL;
}

/* A key with no word anywhere: itself, kept so that it can be given back as all three. */
static word *missing(const char *key) {
    static word kept[64];
    static int nkept;
    for (int i = 0; i < nkept; i++)
        if (!strcmp(kept[i].key, key)) return &kept[i];
    if (nkept == 64) return &kept[63];
    fprintf(stderr, "ui text: no word for %s\n", key);
    word *w = &kept[nkept++];
    w->key = _strdup(key);
    w->utf8 = w->key;
    return w;
}

static word *get(int language, const char *key) {
    word *w = find_word(language, key);
    return w ? w : missing(key);
}

const char *ui_text_in(int language, const char *key) { return get(language, key)->utf8; }

const wchar_t *ui_wtext_in(int language, const char *key) {
    word *w = get(language, key);
    if (!w->wide) w->wide = to_wide(w->utf8);
    return w->wide ? w->wide : L"";
}

void ui_text_to_game(const char *utf8, char *out, size_t size) {
    if (!size) return;
    wchar_t *wide = to_wide(utf8);
    size_t at = 0;
    for (const wchar_t *c = wide; c && *c && at + 1 < size; c++) {
        if (*c < 0x80) {
            out[at++] = (char)*c;
            continue;
        }
        int byte = 0;
        for (int b = 0xA1; b <= 0xDF && !byte; b++)
            if (g_letter[b] == *c) byte = b;
        if (byte) {
            out[at++] = (char)byte;
            continue;
        }
        char sjis[4];
        BOOL lost = FALSE;
        const int n = WideCharToMultiByte(932, 0, c, 1, sjis, sizeof sjis, NULL, &lost);
        if (n <= 0 || lost || at + (size_t)n >= size) {
            out[at++] = '?';
            continue;
        }
        memcpy(out + at, sjis, (size_t)n);
        at += (size_t)n;
    }
    out[at] = '\0';
    free(wide);
}

const char *ui_game_text_in(int language, const char *key) {
    word *w = get(language, key);
    if (!w->game) {
        const size_t size = strlen(w->utf8) * 2 + 1;
        w->game = (char *)malloc(size);
        if (w->game) ui_text_to_game(w->utf8, w->game, size);
    }
    return w->game ? w->game : "";
}

wchar_t ui_text_letter_of(unsigned char byte) { return g_letter[byte]; }

void ui_text_from_game(const char *game, int letters, wchar_t *out, size_t cap) {
    size_t n = 0;
    if (!cap) return;
    for (const unsigned char *c = (const unsigned char *)game; *c && n + 1 < cap;) {
        if (letters && g_letter[*c]) {
            out[n++] = g_letter[*c++];
            continue;
        }
        /* Shift-JIS: a lead byte and the one after it, or one byte. */
        const int pair = ((*c >= 0x81 && *c <= 0x9F) || (*c >= 0xE0 && *c <= 0xFC)) && c[1];
        wchar_t got[2];
        const int made = MultiByteToWideChar(932, 0, (const char *)c, pair ? 2 : 1, got, 2);
        for (int k = 0; k < made && n + 1 < cap; k++) out[n++] = got[k];
        c += pair ? 2 : 1;
    }
    out[n] = 0;
}

const char *ui_text(const char *key) { return ui_text_in(g_current, key); }
const wchar_t *ui_wtext(const char *key) { return ui_wtext_in(g_current, key); }
const char *ui_game_text(const char *key) { return ui_game_text_in(g_current, key); }

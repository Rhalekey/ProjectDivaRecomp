/* Playing the game in another language, with its Japanese left where it is.
 *
 * Nearly all of the game's interface text reaches the screen through three functions,
 * which the recompiler calls this file from (Tools\build.ps1 names them):
 *
 *   TEXT_DRAW   turns finished text into glyphs. Its text is swapped here, for the
 *               translation in memory of ours, so length does not matter.
 *   TEXT_QUEUE  formats text for later drawing, printf style. A format with numbers
 *               in it has to be swapped before that happens, and since the result can
 *               grow, the game's 1 KB buffer is exchanged for a larger one.
 *   TEXT_SPRINTF builds text into one of the game's own buffers, of a size chosen for
 *               the Japanese. Its format is only swapped for one that is no longer.
 *   TEXT_TICKER scrolls a line along the bottom of the screen, queueing it one letter
 *               at a time. The line is swapped where it is kept, before it gets split.
 *   TEXT_CENTRE works out where a line of a description starts, from its length. It
 *               measures the translation instead, so the line stays in the middle.
 *
 * lang\<language>.txt holds lines of
 *
 *   08B70558 = Free Play
 *
 * where the address is that of the original text in the executable. The text there is
 * what gets matched, so the same words anywhere else are translated too, and the
 * repository needs to contain none of the game's text. Tools\lang.py shows both sides.
 * Song names and lyrics have no lines there. Those in romaji are tables of their own, made by
 * Tools\romaji.py, that the player can turn on and off in the game.
 */

#include "translation.h"

#include <psprecomp/render.h>

#include "presence.h"

#include "letters.h"
#include "ui_text.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/mem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define TEXT_DRAW  0x0883D598u
#define TEXT_QUEUE 0x08851CD8u
#define TEXT_SPRINTF 0x08815FF4u
#define TEXT_TICKER 0x08AF1C54u
#define TEXT_CENTRE 0x08ABF1B8u

/* The game's queue: how many entries are waiting, its buffer, and where the first
 * entry's text goes. */
#define QUEUE_COUNT      0x08C496E0u
#define QUEUE_BUFFER     0x08C492E0u
#define QUEUE_FIRST_TEXT 0x08C45F08u

#define BIGGER_QUEUE_BUFFER 16384u
#define TABLE_SIZE 32768u
#define SEEN_SIZE  16384u

enum { REG_A0 = 4, REG_A1 = 5, REG_A2 = 6, REG_A3 = 7, REG_T3 = 11 };

/* The interface in each language, by its number in lang\languages.txt, then what the player turns
 * on by itself. */
#define KIND_SONG UI_TEXT_MOST_LANGUAGES
#define KIND_LYRIC (KIND_SONG + 1)
#define KIND_COUNT (KIND_SONG + 2)

typedef struct {
    char *original;
    char *text;     /* what is shown, kept here for a language's interface until it is played */
    uint32_t guest;
    int kind;
    int center;     /* a line in a dialog, placed by the game for the length of the original */
    int middle;     /* or, if not zero, where its middle goes */
    uint32_t center_at;  /* the same words are a line in a dialog only at this address */
} entry;

static entry g_table[TABLE_SIZE];
static int g_loaded;
static int g_shown[KIND_COUNT];
static int g_has[KIND_COUNT];
/* The interface's words go into guest memory only for the language being played, all into one
 * block, as large as the largest language needs: more languages cost the game no memory. */
static uint32_t g_kind_bytes[KIND_COUNT];
static uint32_t g_interface_block, g_interface_room;
static int g_active;
static uint32_t g_queue_buffer;
static FILE *g_missing;
static char g_data_dir[1100];
static int g_interface = -1;   /* the kind being shown, if any */
static uint32_t g_seen[SEEN_SIZE];

static uint32_t hash(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
    return h;
}

/* Guest memory as a C string, or NULL if it is not inside RAM and terminated. */
static const char *guest_text(uint32_t addr) {
    const uint32_t off = (addr & PSP_ADDR_MASK) - PSP_RAM_BASE;
    if (off >= PSP_RAM_SIZE) return NULL;
    const char *s = (const char *)psp_mem.ram + off;
    return memchr(s, 0, PSP_RAM_SIZE - off) ? s : NULL;
}

/* The same original can be in several tables: this is the one being shown, if any.
 * *script says whether it is a song name or lyric that is being left as it is. */
static const entry *find_entry(const char *text, int *script) {
    for (uint32_t i = hash(text) % TABLE_SIZE;; i = (i + 1) % TABLE_SIZE) {
        if (!g_table[i].original) return NULL;
        if (strcmp(g_table[i].original, text)) continue;
        /* Shown only with a place in guest memory: a text the game cannot reach is left as it is. */
        if (g_shown[g_table[i].kind] && g_table[i].guest) return &g_table[i];
        if (script && g_table[i].kind >= KIND_SONG) *script = 1;
    }
}

static entry *in_table(const char *text, int kind) {
    for (uint32_t i = hash(text) % TABLE_SIZE;; i = (i + 1) % TABLE_SIZE) {
        if (!g_table[i].original) return NULL;
        if (g_table[i].kind == kind && !strcmp(g_table[i].original, text)) return &g_table[i];
    }
}

static uint32_t find(const char *text) {
    const entry *e = find_entry(text, NULL);
    return e ? e->guest : 0;
}

/* With DIVA_LANG_LOG set, each different text with no translation goes to a file,
 * which shows what is left to do. Only what holds something other than ASCII. */
static void note_missing(const char *text) {
    if (!g_missing || g_interface < 0) return;
    int foreign = 0;
    for (const char *p = text; *p; p++) foreign |= (uint8_t)*p >= 0x80;
    if (!foreign) return;
    const uint32_t h = hash(text) | 1;
    for (uint32_t i = h % SEEN_SIZE, probes = 0;; i = (i + 1) % SEEN_SIZE, probes++) {
        if (g_seen[i] == h) return;
        if (probes == SEEN_SIZE) return;
        if (!g_seen[i]) {
            g_seen[i] = h;
            break;
        }
    }
    for (const char *p = text; *p; p++) fprintf(g_missing, "%02X", (uint8_t)*p);
    fputc('\n', g_missing);
    fflush(g_missing);
}

/* Some text is drawn one letter at a time, which no table can translate. So that it
 * can be caught before it is split, say which code is doing it, once per caller. */
#define MAX_LETTER_CALLERS 32

static void note_letter(const char *text) {
    if (!g_missing || !text || (uint8_t)text[0] < 0x80 || strlen(text) != 2) return;
    uint32_t callers[4] = {psp_cpu.r[PSP_REG_RA]};
    int depth = 1;
    for (const psp_frame *f = psp_cpu.frame; f && depth < 4; f = f->prev) {
        if (f->ret) callers[depth++] = f->ret;
    }
    static uint32_t seen[MAX_LETTER_CALLERS];
    static int nseen;
    for (int i = 0; i < nseen; i++) {
        if (seen[i] == callers[0]) return;
    }
    if (nseen == MAX_LETTER_CALLERS) return;
    seen[nseen++] = callers[0];
    fprintf(stderr, "translation: letter by letter from %08X %08X %08X %08X\n", callers[0], callers[1], callers[2],
            callers[3]);
}

/* The next conversion in a printf format, or NULL. Its letter goes to *conv. */
static const char *next_conversion(const char *f, char *conv) {
    for (; *f; f++) {
        if (*f != '%') continue;
        f++;
        if (*f == '%') continue;
        while (*f && strchr("-+ #0123456789.hlL", *f)) f++;
        if (!*f) return NULL;
        *conv = *f;
        return f + 1;
    }
    return NULL;
}

/* A translated format has to take the same arguments, or the game would crash. */
static int same_conversions(const char *a, const char *b) {
    for (;;) {
        char ca = 0, cb = 0;
        a = next_conversion(a, &ca);
        b = next_conversion(b, &cb);
        if (!a || !b) return !a && !b;
        if (ca != cb) return 0;
    }
}

static void on_text_queue(void) {
    if (psp_read32(QUEUE_COUNT) == 0 && psp_read32(QUEUE_FIRST_TEXT) == QUEUE_BUFFER) {
        psp_write32(QUEUE_FIRST_TEXT, g_queue_buffer);
    }
    const char *format = guest_text(psp_cpu.r[REG_A2]);
    if (!format) return;
    note_letter(strcmp(format, "%s") == 0 ? guest_text(psp_cpu.r[REG_A3]) : format);
    /* Queued text is drawn later from the queue's buffer, so this is where its own address is seen. */
    if (!strcmp(format, "%s")) {
        const char *queued = guest_text(psp_cpu.r[REG_A3]);
        if (queued) presence_text(psp_cpu.r[REG_A3] & PSP_ADDR_MASK, queued, 0);
    } else if (!strchr(format, '%')) {
        presence_text(psp_cpu.r[REG_A2] & PSP_ADDR_MASK, format, 0);
    }
    if (!strchr(format, '%')) return;

    /* A bare "%s" is finished text, which is translated when it is drawn. */
    if (!strcmp(format, "%s")) return;

    /* Words passed for %s, in the registers that hold the first variable arguments. */
    int reg = REG_A3;
    const char *f = format;
    char conv = 0;
    while (reg <= REG_T3 && (f = next_conversion(f, &conv)) != NULL) {
        if (strchr("eEfgG", conv)) break;
        if (conv == 's') {
            const char *arg = guest_text(psp_cpu.r[reg]);
            const uint32_t translated = arg ? find(arg) : 0;
            if (translated) psp_cpu.r[reg] = translated;
        }
        reg++;
    }

    const uint32_t translated = find(format);
    if (translated) psp_cpu.r[REG_A2] = translated;
    else note_missing(format);
}

/* Lyrics are lit in step with the song, by what the game takes their width to be. */
static int g_drawing_lyric;
static int g_drawing_placed;

static void on_text_draw(void) {
    g_drawing_lyric = 0;
    g_drawing_placed = 0;
    const char *text = guest_text(psp_cpu.r[REG_A1]);
    if (!text) return;
    int script = 0;
    const entry *e = find_entry(text, &script);
    if (!e) {
        if (!script) note_missing(text);
        return;
    }
    g_drawing_lyric = e->kind == KIND_LYRIC;
    const uint32_t shown_from = psp_cpu.r[REG_A1];
    psp_cpu.r[REG_A1] = e->guest;
    if (e->center || e->center_at == shown_from) {
        /* Placed by the game for the width of the original. */
        const uint32_t size = psp_cpu.r[REG_A2];
        const int32_t spacing = (int32_t)psp_cpu.r[REG_A3];
        const float width = letters_width(guest_text(e->guest), size, spacing);
        if (e->middle) psp_cpu.f[12] = (float)e->middle - width / 2.0f;
        else psp_cpu.f[12] += (letters_room(e->original, size, spacing) - width) / 2.0f;
        g_drawing_placed = 1;
    }
}

/* Lines that TEXT_CENTRE placed: it counts their bytes, and they are narrower than that now. */
#define CENTRED_SIZE 1024u
static uint32_t g_centred[CENTRED_SIZE];

static void note_centred(const char *text) {
    const uint32_t h = hash(text) | 1;
    for (uint32_t i = h % CENTRED_SIZE, probes = 0; probes < CENTRED_SIZE; i = (i + 1) % CENTRED_SIZE, probes++) {
        if (g_centred[i] == h) return;
        if (!g_centred[i]) {
            g_centred[i] = h;
            return;
        }
    }
}

static int was_centred(const char *text) {
    const uint32_t h = hash(text) | 1;
    for (uint32_t i = h % CENTRED_SIZE, probes = 0; probes < CENTRED_SIZE; i = (i + 1) % CENTRED_SIZE, probes++) {
        if (g_centred[i] == h) return 1;
        if (!g_centred[i]) return 0;
    }
    return 0;
}

/* Text the game put in the middle of the screen, going by what it takes the width to be. */
#define SCREEN_MIDDLE 240.0f
#define MIDDLE_WITHIN 1.5f

static int is_on_screen_middle(const char *text, uint32_t size, int32_t spacing) {
    const float off = psp_cpu.f[12] + letters_room(text, size, spacing) / 2.0f - SCREEN_MIDDLE;
    return off > -MIDDLE_WITHIN && off < MIDDLE_WITHIN;
}

/* The middle of the Ad Hoc menu's window, less its scroll bar. */
#define AD_HOC_LINES_MIDDLE 238.0f

static void text_draw_hook(void) {
    const uint32_t asked = psp_cpu.r[REG_A1] & PSP_ADDR_MASK;
    const uint32_t size = psp_cpu.r[REG_A2];
    const int32_t spacing = (int32_t)psp_cpu.r[REG_A3];
    const char *before = guest_text(psp_cpu.r[REG_A1]);
    const int in_middle = before && is_on_screen_middle(before, size, spacing);
    on_text_draw();
    const char *text = guest_text(psp_cpu.r[REG_A1]);
    if (!text) return;
    if (!g_drawing_placed && !g_drawing_lyric && in_middle) {
        psp_cpu.f[12] = SCREEN_MIDDLE - letters_width(text, size, spacing) / 2.0f;
        g_drawing_placed = 1;
    }
    if (!g_drawing_placed && was_centred(text)) {
        psp_cpu.f[12] += (letters_room(text, size, spacing) - letters_width(text, size, spacing)) / 2.0f;
        g_drawing_placed = 1;
    }
    /* The Ad Hoc menu's lines are placed by the width of the Japanese. */
    if (asked == 0x08B717D8u || asked == 0x08B717E8u || asked == 0x08B717FCu) {
        psp_cpu.f[12] = AD_HOC_LINES_MIDDLE - letters_width(text, size, spacing) / 2.0f;
        g_drawing_placed = 1;
    }
    /* A line a message's window holds, centred. The game sized the window for the Japanese, before
     * this line was known, so the window is drawn wider where the line is wider, in any language. */
    if (g_drawing_placed && !g_drawing_lyric) psp_render_gl_dialog_line(letters_width(text, size, spacing));
    /* What is on screen tells Discord what the player is doing. */
    presence_text(asked, text, g_drawing_placed);
    /* These sit low on their plates in this font. */
    if (!strcmp(text, "RETRY") || !strcmp(text, "RETURN")) psp_cpu.f[13] -= 1.5f;
    /* Lyrics are lit in step with the song, by what the game takes their width to be. */
    letters_begin(psp_cpu.r[REG_A1], size, spacing, g_drawing_lyric, g_drawing_placed);
}

static void on_text_sprintf(void) {
    const char *format = guest_text(psp_cpu.r[REG_A1]);
    if (!format || !strchr(format, '%')) return;

    /* Words for %s, under the same rule. */
    int reg = REG_A2;
    const char *f = format;
    char conv = 0;
    while (reg <= REG_T3 && (f = next_conversion(f, &conv)) != NULL) {
        if (strchr("eEfgG", conv)) break;
        if (conv == 's') {
            const char *arg = guest_text(psp_cpu.r[reg]);
            const uint32_t word = arg ? find(arg) : 0;
            if (word && strlen(guest_text(word)) <= strlen(arg)) psp_cpu.r[reg] = word;
        }
        reg++;
    }

    const uint32_t translated = find(format);
    if (!translated) {
        note_missing(format);
        return;
    }
    if (strlen(guest_text(translated)) <= strlen(format)) psp_cpu.r[REG_A1] = translated;
}

static void on_text_centre(void) {
    const char *text = guest_text(psp_cpu.r[REG_A0]);
    const uint32_t translated = text ? find(text) : 0;
    if (translated) psp_cpu.r[REG_A0] = translated;
    if (translated || text) note_centred(guest_text(psp_cpu.r[REG_A0]));
}

/* Its first argument is the ticker, which starts with a pointer to its line. */
static void on_text_ticker(void) {
    const uint32_t ticker = psp_cpu.r[REG_A0];
    const uint32_t line = psp_read32(ticker);
    const char *text = line ? guest_text(line) : NULL;
    if (!text) return;
    const uint32_t translated = find(text);
    if (translated) psp_write32(ticker, translated);
    else note_missing(text);
}

void translation_hook(uint32_t addr) {
    if (!g_active) return;
    if (letters_hook(addr)) return;
    if (addr == TEXT_DRAW) text_draw_hook();
    else if (addr == TEXT_QUEUE) on_text_queue();
    else if (addr == TEXT_TICKER) on_text_ticker();
    else if (addr == TEXT_SPRINTF) on_text_sprintf();
    else if (addr == TEXT_CENTRE) on_text_centre();
}

/* The first use of any table gets the larger queue buffer. Nonzero if it is ready. */
static int get_ready(void) {
    if (!g_queue_buffer) g_queue_buffer = psp_sysmem_alloc(BIGGER_QUEUE_BUFFER, 1);
    if (!g_queue_buffer) {
        fprintf(stderr, "translation: no memory\n");
        return 0;
    }
    g_active = 1;
    return 1;
}

/* Returns how many lines it loaded, or -1 if there is no such file. */
static int load_table(const char *path, int kind) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    /* Song names and lyrics are always in guest memory. Two passes: it is allocated once its size
     * is known. */
    char line[1024];
    uint32_t next = 0;
    if (kind >= KIND_SONG) {
        uint32_t bytes = 0;
        while (fgets(line, sizeof line, f)) bytes += (uint32_t)strlen(line) + 1;
        next = bytes ? psp_sysmem_alloc(bytes, 1) : 0;
        if (!next) {
            fprintf(stderr, "translation: no memory for %s\n", path);
            fclose(f);
            return 0;
        }
        rewind(f);
    }
    int loaded = 0, skipped = 0;
    char letters[sizeof line];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || !line[0]) continue;
        char *end;
        const uint32_t addr = (uint32_t)strtoul(line, &end, 16);
        const char *eq = strchr(end, '=');
        const char *original = guest_text(addr);
        /* 'a+b' is a text the game joins from two pieces before drawing it. */
        char joined[sizeof line];
        if (*end == '+' && original) {
            const char *second = guest_text((uint32_t)strtoul(end + 1, NULL, 16));
            snprintf(joined, sizeof joined, "%s%s", original, second ? second : "");
            original = second ? joined : NULL;
        }
        if (!eq || !original) {
            fprintf(stderr, "translation: bad line: %s\n", line);
            skipped++;
            continue;
        }
        const char *text = eq[1] == ' ' ? eq + 2 : eq + 1;
        /* '^' marks a line the game centres by hand, in a dialog. '^350|' says where the
         * middle is, for lines the game starts from no centre at all. */
        const int center = text[0] == '^';
        int middle = 0;
        if (center) {
            text++;
            char *after;
            const long value = strtol(text, &after, 10);
            if (after != text && *after == '|') {
                middle = (int)value;
                text = after + 1;
            }
        }
        /* Song names and lyrics are already in the game's own encoding; the rest is UTF-8. */
        if (kind < KIND_SONG) {
            ui_text_to_game(text, letters, sizeof letters);
            text = letters;
        }
        if (g_loaded + 1 >= (int)(TABLE_SIZE / 2)) {
            fprintf(stderr, "translation: table full at: %s\n", line);
            skipped++;
            continue;
        }
        if (!same_conversions(original, text)) {
            fprintf(stderr, "translation: arguments do not match: %s\n", line);
            skipped++;
            continue;
        }
        entry *known = in_table(original, kind);
        if (known) {
            if (center && !known->center) known->center_at = addr;
            continue;
        }

        const uint32_t size = (uint32_t)strlen(text) + 1;
        if (next) memcpy(psp_mem_ptr(next, size), text, size);
        uint32_t i = hash(original) % TABLE_SIZE;
        while (g_table[i].original) i = (i + 1) % TABLE_SIZE;
        g_table[i].original = _strdup(original);
        g_table[i].text = next ? NULL : _strdup(text);
        g_table[i].guest = next;
        g_kind_bytes[kind] += size;
        g_table[i].kind = kind;
        g_table[i].center = center;
        g_table[i].middle = middle;
        g_table[i].center_at = 0;
        if (next) next += size;
        loaded++;
        g_loaded++;
    }
    fclose(f);
    fprintf(stderr, "translation: %s, %d texts, %d lines skipped\n", path, loaded, skipped);
    return loaded;
}

/* The words of a language's interface, into the block, where the game can read them. */
static void place_interface(int kind) {
    uint32_t at = g_interface_block;
    for (uint32_t i = 0; i < TABLE_SIZE; i++) {
        entry *e = &g_table[i];
        if (!e->original || e->kind >= KIND_SONG) continue;
        if (e->kind != kind || !e->text) {
            e->guest = 0;
            continue;
        }
        const uint32_t size = (uint32_t)strlen(e->text) + 1;
        if (at + size > g_interface_block + g_interface_room) {
            e->guest = 0;
            continue;
        }
        memcpy(psp_mem_ptr(at, size), e->text, size);
        e->guest = at;
        at += size;
    }
}

void translation_set_language(const char *code) {
    if (g_interface >= 0) g_shown[g_interface] = 0;
    g_interface = -1;
    if (g_missing) fclose(g_missing);
    g_missing = NULL;
    memset(g_seen, 0, sizeof g_seen);
    const int kind = ui_text_find(code);
    if (kind < 0 || !g_has[kind] || !g_interface_block) return;
    place_interface(kind);
    g_interface = kind;
    g_shown[kind] = 1;
    if (getenv("DIVA_LANG_LOG")) {
        char path[1200];
        snprintf(path, sizeof path, "%s\\lang_missing_%s.txt", g_data_dir, code);
        g_missing = fopen(path, "a");
    }
}

void translation_start(const char *dir, const char *data_dir) {
    snprintf(g_data_dir, sizeof g_data_dir, "%s", data_dir);
    if (!get_ready()) return;
    for (int i = 0; i < ui_text_languages() && i < UI_TEXT_MOST_LANGUAGES; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s\\%s.txt", dir, ui_text_code(i));
        const int loaded = load_table(path, i);
        /* Japanese is the game's own, with no table. */
        if (loaded < 0 && strcmp(ui_text_code(i), "ja")) fprintf(stderr, "translation: cannot read %s\n", path);
        g_has[i] = loaded > 0;
        if (g_kind_bytes[i] > g_interface_room) g_interface_room = g_kind_bytes[i];
    }
    if (g_interface_room) g_interface_block = psp_sysmem_alloc(g_interface_room, 1);
    if (g_interface_room && !g_interface_block) fprintf(stderr, "translation: no memory for the interface's words\n");
}

void translation_romaji_start(const char *dir) {
    if (!get_ready()) return;
    char path[1200];
    snprintf(path, sizeof path, "%s\\songs.txt", dir);
    g_has[KIND_SONG] = load_table(path, KIND_SONG) > 0;
    snprintf(path, sizeof path, "%s\\lyrics.txt", dir);
    g_has[KIND_LYRIC] = load_table(path, KIND_LYRIC) > 0;
}

int translation_has_song_romaji(void) { return g_has[KIND_SONG]; }

const char *translation_shown(const char *original) {
    const uint32_t guest = find(original);
    return guest ? guest_text(guest) : NULL;
}

void translation_shown_utf8(const char *original, char *out, size_t size) {
    const char *shown = translation_shown(original);
    wchar_t wide[512];
    ui_text_from_game(shown ? shown : original, shown != NULL, wide, sizeof wide / sizeof wide[0]);
    if (!size) return;
    if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)size, NULL, NULL)) out[0] = '\0';
}

const char *translation_song_romaji(const char *original) {
    const entry *e = in_table(original, KIND_SONG);
    return e ? guest_text(e->guest) : NULL;
}
int translation_has_lyric_romaji(void) { return g_has[KIND_LYRIC]; }

void translation_set_romaji(int song_names, int lyrics) {
    g_shown[KIND_SONG] = song_names;
    g_shown[KIND_LYRIC] = lyrics;
}

/* Alchemy IGB files: see igb.h, and Tools/igb.py for the layout. */
#include "igb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0xFADAu
#define FLAG_INFO 0x8000u
#define FLAG_EXTERNAL 0x4000u
#define FLAG_SHARED 0x2000u
#define FLAG_POOLS 0x1000u

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t pad4(uint32_t n) { return (n + 3u) & ~3u; }

static igb_bytes copy_bytes(const uint8_t *p, uint32_t n) {
    igb_bytes b = {NULL, n};
    b.data = (uint8_t *)malloc(n ? n : 1);
    if (b.data && n) memcpy(b.data, p, n);
    return b;
}

static void free_record(igb_record *r) {
    for (int i = 0; i < r->nvalues; i++) free(r->values[i].data);
    free(r->values);
    r->values = NULL;
    r->nvalues = 0;
}

static igb_record copy_record(const igb_record *r) {
    igb_record c = {r->meta, r->nvalues, (igb_bytes *)calloc((size_t)(r->nvalues ? r->nvalues : 1), sizeof(igb_bytes))};
    for (int i = 0; i < r->nvalues; i++) c.values[i] = copy_bytes(r->values[i].data, r->values[i].size);
    return c;
}

/* Reads a record at p: its class, its size, then its fields in 4 byte steps. */
static int read_record(const igb *g, const uint8_t *b, size_t size, size_t p, igb_record *out, uint32_t *length) {
    if (p + 8 > size) return 0;
    const uint32_t meta = rd32(b + p), len = rd32(b + p + 4);
    if (meta >= (uint32_t)g->nmeta_objects || len < 8 || p + len > size) return 0;
    const igb_meta_object *m = &g->meta_objects[meta];
    out->meta = meta;
    out->values = (igb_bytes *)calloc((size_t)m->nfields + 1, sizeof(igb_bytes));
    out->nvalues = 0;
    size_t q = p + 8;
    for (int k = 0; k < m->nfields; k++) {
        const uint32_t fsize = m->fields[k][2];
        if (q + fsize > p + len) return 0;
        out->values[out->nvalues++] = copy_bytes(b + q, fsize);
        q += pad4(fsize);
    }
    if (q < p + len) out->values[out->nvalues++] = copy_bytes(b + q, (uint32_t)(p + len - q));
    *length = len;
    return 1;
}

static const char *class_of_record(const igb *g, const igb_record *r) { return g->meta_objects[r->meta].name; }

static int entry_is_object(const igb *g, uint32_t entry) {
    return !strcmp(class_of_record(g, &g->entries[entry]), "igObjectDirEntry");
}

static uint32_t entry_memory_size(const igb *g, uint32_t entry) {
    const igb_record *e = &g->entries[entry];
    return e->nvalues > 1 && e->values[1].size == 4 ? rd32(e->values[1].data) : 0;
}

static int grow(void **list, int *cap, int count, size_t each) {
    if (count < *cap) return 1;
    const int bigger = *cap ? *cap * 2 : 64;
    void *p = realloc(*list, (size_t)bigger * each);
    if (!p) return 0;
    *list = p;
    *cap = bigger;
    return 1;
}

igb *igb_read(const uint8_t *b, size_t size) {
    if (size < 0x30) return NULL;
    uint32_t h[12];
    for (int i = 0; i < 12; i++) h[i] = rd32(b + i * 4);
    if (h[10] != MAGIC || (h[11] & 0xFFFF) != 8) return NULL;
    igb *g = (igb *)calloc(1, sizeof *g);
    if (!g) return NULL;
    g->version = h[11] & 0xFFFF;
    g->flags = h[11] >> 16;
    g->info = -1;
    size_t p = 0x30;
#define NEED(n) do { if (p + (n) > size) goto bad; } while (0)

    /* strings */
    NEED(8);
    const uint32_t ssize = rd32(b + p), scount = rd32(b + p + 4);
    NEED(ssize);
    /* A count the block could not hold is not believed: each string takes at least 4 bytes. */
    if (ssize < 8 || scount > (ssize - 8) / 4) goto bad;
    g->strings = (char **)calloc(scount ? scount : 1, sizeof(char *));
    if (!g->strings) goto bad;
    g->nstrings = (int)scount;
    size_t q = p + 8;
    for (uint32_t i = 0; i < scount; i++) {
        if (q + 4 > p + ssize) goto bad;
        const uint32_t n = rd32(b + q);
        if (q + 4 + n > p + ssize) goto bad;
        if (n) {
            g->strings[i] = (char *)calloc(n + 1, 1);
            if (!g->strings[i]) goto bad;
            memcpy(g->strings[i], b + q + 4, n);
        }
        q += 4 + n;
    }
    g->strings_tail = copy_bytes(b + q, (uint32_t)(p + ssize - q));
    p += ssize;

    /* meta fields */
    NEED(h[8]);
    if ((uint64_t)h[9] * 12u > h[8]) goto bad;     /* its table inside its block */
    g->meta_fields = (igb_meta_field *)calloc(h[9] ? h[9] : 1, sizeof(igb_meta_field));
    if (!g->meta_fields) goto bad;
    g->nmeta_fields = (int)h[9];
    size_t names = p + h[9] * 12u;
    for (uint32_t i = 0; i < h[9]; i++) {
        const uint32_t n = rd32(b + p + i * 12);
        if (!n || names + n > p + h[8]) goto bad;
        g->meta_fields[i].name = (char *)calloc(n + 1, 1);
        if (!g->meta_fields[i].name) goto bad;
        memcpy(g->meta_fields[i].name, b + names, n - 1);
        g->meta_fields[i].major = rd32(b + p + i * 12 + 4);
        g->meta_fields[i].minor = rd32(b + p + i * 12 + 8);
        names += n;
    }
    p += h[8];

    /* alignments */
    NEED(4);
    const uint32_t asize = rd32(b + p);
    NEED(asize);
    g->alignments = copy_bytes(b + p, asize);
    p += asize;

    /* meta objects */
    NEED(h[2]);
    if ((uint64_t)h[3] * 24u > h[2]) goto bad;
    g->meta_objects = (igb_meta_object *)calloc(h[3] ? h[3] : 1, sizeof(igb_meta_object));
    if (!g->meta_objects) goto bad;
    g->nmeta_objects = (int)h[3];
    size_t rest = p + h[3] * 24u;
    for (uint32_t i = 0; i < h[3]; i++) {
        igb_meta_object *m = &g->meta_objects[i];
        const uint8_t *t = b + p + i * 24;
        m->name_len = rd32(t);
        m->major = rd32(t + 4);
        m->minor = rd32(t + 8);
        m->nfields = (int)rd32(t + 12);
        m->parent = (int32_t)rd32(t + 16);
        m->slots = rd32(t + 20);
        if (m->nfields < 0 || rest + m->name_len + (size_t)m->nfields * 6 > p + h[2]) {
            m->nfields = 0;
            goto bad;
        }
        m->name = (char *)calloc(m->name_len + 1, 1);
        m->fields = (uint16_t(*)[3])calloc((size_t)(m->nfields ? m->nfields : 1), sizeof *m->fields);
        if (!m->name || !m->fields) goto bad;
        memcpy(m->name, b + rest, m->name_len);
        rest += m->name_len;
        for (int k = 0; k < m->nfields; k++) {
            m->fields[k][0] = rd16(b + rest);
            m->fields[k][1] = rd16(b + rest + 2);
            m->fields[k][2] = rd16(b + rest + 4);
            rest += 6;
            if (m->fields[k][0] >= g->nmeta_fields) goto bad;
        }
    }
    p += h[2];

    if (g->flags & FLAG_EXTERNAL) {
        NEED(4);
        const uint32_t n = rd32(b + p);
        NEED(n);
        g->externals = copy_bytes(b + p, n);
        p += n;
    }
    if (g->flags & FLAG_POOLS) {
        NEED(4);
        const uint32_t n = rd32(b + p);
        NEED(n);
        g->pools = copy_bytes(b + p, n);
        p += n;
    }

    /* entries */
    const size_t entries_end = p + h[0];
    NEED(h[0]);
    for (uint32_t i = 0; i < h[1]; i++) {
        if (!grow((void **)&g->entries, &g->cap_entries, g->nentries, sizeof(igb_record))) goto bad;
        uint32_t len;
        if (!read_record(g, b, entries_end, p, &g->entries[g->nentries], &len)) goto bad;
        g->nentries++;
        p += len;
    }
    if (p != entries_end) goto bad;

    /* the directory's order */
    uint32_t nslots = h[1];
    uint32_t *order = NULL;
    if (g->flags & FLAG_SHARED) {
        NEED(8);
        const uint32_t isize = rd32(b + p), icount = rd32(b + p + 4);
        NEED(isize);
        if (8 + icount * 2u > isize) goto bad;
        nslots = icount;
        order = (uint32_t *)malloc((icount ? icount : 1) * sizeof(uint32_t));
        for (uint32_t i = 0; i < icount; i++) {
            order[i] = rd16(b + p + 8 + i * 2);
            if (order[i] >= (uint32_t)g->nentries) { free(order); goto bad; }
        }
        g->indexes_tail = copy_bytes(b + p + 8 + icount * 2, isize - 8 - icount * 2);
        p += isize;
    } else {
        order = (uint32_t *)malloc((nslots ? nslots : 1) * sizeof(uint32_t));
        for (uint32_t i = 0; i < nslots; i++) order[i] = i;
    }
    if (g->flags & FLAG_INFO) {
        NEED(4);
        g->info = (int32_t)rd32(b + p);
        p += 4;
    }
    g->indexes = order;
    g->items = (igb_item *)calloc(nslots ? nslots : 1, sizeof(igb_item));
    g->nitems = g->cap_items = (int)nslots;

    /* objects in the directory's order, then memory */
    const size_t objects_end = p + h[4];
    NEED(h[4]);
    for (uint32_t s = 0; s < nslots; s++) {
        if (!entry_is_object(g, order[s])) {
            g->items[s].meta = 0xFFFFFFFFu;
            continue;
        }
        uint32_t len;
        if (!read_record(g, b, objects_end, p, &g->items[s].object, &len)) goto bad;
        g->items[s].meta = g->items[s].object.meta;
        p += len;
    }
    if (p != objects_end) goto bad;
    const size_t mem_at = p;
    for (uint32_t s = 0; s < nslots; s++) {
        if (g->items[s].meta != 0xFFFFFFFFu) continue;
        const uint32_t n = entry_memory_size(g, order[s]);
        const size_t start = mem_at + pad4((uint32_t)(p - mem_at));
        if (start + n > size) goto bad;
        g->items[s].memory = copy_bytes(b + start, n);
        p = start + n;
    }
    g->tail = copy_bytes(b + p, (uint32_t)(size - p));
    return g;
#undef NEED
bad:
    igb_free(g);
    return NULL;
}

/* ---- writing ------------------------------------------------------------------------- */

typedef struct {
    uint8_t *data;
    size_t size, cap;
} buf;

static void put(buf *o, const void *p, size_t n) {
    if (o->size + n > o->cap) {
        size_t c = o->cap ? o->cap : 4096;
        while (c < o->size + n) c *= 2;
        uint8_t *d = (uint8_t *)realloc(o->data, c);
        if (!d) return;
        o->data = d;
        o->cap = c;
    }
    if (n) memcpy(o->data + o->size, p, n);
    o->size += n;
}

static void put32(buf *o, uint32_t v) {
    const uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    put(o, b, 4);
}

static void put16(buf *o, uint16_t v) {
    const uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    put(o, b, 2);
}

static void put_zero(buf *o, size_t n) {
    static const uint8_t zero[16];
    while (n) {
        const size_t k = n < 16 ? n : 16;
        put(o, zero, k);
        n -= k;
    }
}

static void put_record(const igb *g, buf *o, const igb_record *r) {
    const int nfields = g->meta_objects[r->meta].nfields;
    uint32_t len = 8;
    for (int i = 0; i < r->nvalues; i++) len += i < nfields ? pad4(r->values[i].size) : r->values[i].size;
    put32(o, r->meta);
    put32(o, len);
    for (int i = 0; i < r->nvalues; i++) {
        put(o, r->values[i].data, r->values[i].size);
        if (i < nfields) put_zero(o, pad4(r->values[i].size) - r->values[i].size);
    }
}

uint8_t *igb_write(const igb *g, size_t *size) {
    buf out = {0};
    put_zero(&out, 0x30);

    buf s = {0};
    for (int i = 0; i < g->nstrings; i++) {
        if (!g->strings[i]) { put32(&s, 0); continue; }
        const uint32_t n = (uint32_t)strlen(g->strings[i]) + 1;
        put32(&s, n);
        put(&s, g->strings[i], n);
    }
    put(&s, g->strings_tail.data, g->strings_tail.size);
    put32(&out, (uint32_t)s.size + 8);
    put32(&out, (uint32_t)g->nstrings);
    put(&out, s.data, s.size);
    free(s.data);

    const size_t mf_at = out.size;
    for (int i = 0; i < g->nmeta_fields; i++) {
        put32(&out, (uint32_t)strlen(g->meta_fields[i].name) + 1);
        put32(&out, g->meta_fields[i].major);
        put32(&out, g->meta_fields[i].minor);
    }
    for (int i = 0; i < g->nmeta_fields; i++) put(&out, g->meta_fields[i].name, strlen(g->meta_fields[i].name) + 1);
    const uint32_t mf_size = (uint32_t)(out.size - mf_at);
    put(&out, g->alignments.data, g->alignments.size);

    const size_t mo_at = out.size;
    for (int i = 0; i < g->nmeta_objects; i++) {
        const igb_meta_object *m = &g->meta_objects[i];
        put32(&out, m->name_len);
        put32(&out, m->major);
        put32(&out, m->minor);
        put32(&out, (uint32_t)m->nfields);
        put32(&out, (uint32_t)m->parent);
        put32(&out, m->slots);
    }
    for (int i = 0; i < g->nmeta_objects; i++) {
        const igb_meta_object *m = &g->meta_objects[i];
        put(&out, m->name, m->name_len);
        for (int k = 0; k < m->nfields; k++) {
            put16(&out, m->fields[k][0]);
            put16(&out, m->fields[k][1]);
            put16(&out, m->fields[k][2]);
        }
    }
    const uint32_t mo_size = (uint32_t)(out.size - mo_at);
    put(&out, g->externals.data, g->externals.size);
    put(&out, g->pools.data, g->pools.size);

    const size_t en_at = out.size;
    for (int i = 0; i < g->nentries; i++) put_record(g, &out, &g->entries[i]);
    const uint32_t en_size = (uint32_t)(out.size - en_at);
    if (g->flags & FLAG_SHARED) {
        put32(&out, 8u + (uint32_t)g->nitems * 2u + g->indexes_tail.size);
        put32(&out, (uint32_t)g->nitems);
        for (int i = 0; i < g->nitems; i++) put16(&out, (uint16_t)g->indexes[i]);
        put(&out, g->indexes_tail.data, g->indexes_tail.size);
    }
    if (g->flags & FLAG_INFO) put32(&out, (uint32_t)g->info);

    const size_t ob_at = out.size;
    uint32_t nobjects = 0, nblocks = 0;
    for (int i = 0; i < g->nitems; i++)
        if (g->items[i].meta != 0xFFFFFFFFu) {
            put_record(g, &out, &g->items[i].object);
            nobjects++;
        }
    const uint32_t ob_size = (uint32_t)(out.size - ob_at);
    const size_t me_at = out.size;
    for (int i = 0; i < g->nitems; i++) {
        if (g->items[i].meta != 0xFFFFFFFFu) continue;
        put_zero(&out, pad4((uint32_t)(out.size - me_at)) - (out.size - me_at));
        put(&out, g->items[i].memory.data, g->items[i].memory.size);
        nblocks++;
    }
    const uint32_t me_size = (uint32_t)(out.size - me_at);
    put(&out, g->tail.data, g->tail.size);

    const uint32_t head[12] = {en_size, (uint32_t)g->nentries, mo_size, (uint32_t)g->nmeta_objects, ob_size, nobjects, me_size, nblocks,
                               mf_size, (uint32_t)g->nmeta_fields, MAGIC, g->flags << 16 | g->version};
    for (int i = 0; i < 12 && out.data; i++) {
        uint8_t *p = out.data + i * 4;
        p[0] = (uint8_t)head[i];
        p[1] = (uint8_t)(head[i] >> 8);
        p[2] = (uint8_t)(head[i] >> 16);
        p[3] = (uint8_t)(head[i] >> 24);
    }
    *size = out.size;
    return out.data;
}

void igb_free(igb *g) {
    if (!g) return;
    for (int i = 0; i < g->nstrings; i++) free(g->strings[i]);
    free(g->strings);
    free(g->strings_tail.data);
    free(g->alignments.data);
    free(g->externals.data);
    free(g->pools.data);
    free(g->indexes_tail.data);
    free(g->tail.data);
    for (int i = 0; i < g->nmeta_fields; i++) free(g->meta_fields[i].name);
    free(g->meta_fields);
    for (int i = 0; i < g->nmeta_objects; i++) {
        free(g->meta_objects[i].name);
        free(g->meta_objects[i].fields);
    }
    free(g->meta_objects);
    for (int i = 0; i < g->nentries; i++) free_record(&g->entries[i]);
    free(g->entries);
    for (int i = 0; i < g->nitems; i++) {
        free_record(&g->items[i].object);
        free(g->items[i].memory.data);
    }
    free(g->items);
    free(g->indexes);
    free(g);
}

/* ---- looking and changing ------------------------------------------------------------ */

int igb_is_object(const igb *g, int slot) { return slot >= 0 && slot < g->nitems && g->items[slot].meta != 0xFFFFFFFFu; }

const char *igb_class(const igb *g, int slot) { return igb_is_object(g, slot) ? g->meta_objects[g->items[slot].meta].name : NULL; }

const char *igb_memory_type(const igb *g, int slot) {
    if (slot < 0 || slot >= g->nitems || igb_is_object(g, slot)) return NULL;
    const igb_record *e = &g->entries[g->indexes[slot]];
    if (e->nvalues < 3 || e->values[2].size != 4) return NULL;
    const int32_t t = (int32_t)rd32(e->values[2].data);
    return t >= 0 && t < g->nmeta_fields ? g->meta_fields[t].name : NULL;
}

igb_bytes *igb_field(igb *g, int slot, int k) {
    if (!igb_is_object(g, slot) || k < 0 || k >= g->items[slot].object.nvalues) return NULL;
    return &g->items[slot].object.values[k];
}

int32_t igb_field_i32(const igb *g, int slot, int k) {
    if (!igb_is_object(g, slot) || k < 0 || k >= g->items[slot].object.nvalues || g->items[slot].object.values[k].size < 4) return -1;
    return (int32_t)rd32(g->items[slot].object.values[k].data);
}

void igb_set_i32(igb *g, int slot, int k, int32_t v) {
    igb_bytes *f = igb_field(g, slot, k);
    if (!f || f->size < 4) return;
    f->data[0] = (uint8_t)v;
    f->data[1] = (uint8_t)((uint32_t)v >> 8);
    f->data[2] = (uint8_t)((uint32_t)v >> 16);
    f->data[3] = (uint8_t)((uint32_t)v >> 24);
}

static int add_entry_like(igb *g, uint32_t entry) {
    if (!grow((void **)&g->entries, &g->cap_entries, g->nentries, sizeof(igb_record))) return -1;
    g->entries[g->nentries] = copy_record(&g->entries[entry]);
    return g->nentries++;
}

static void set_entry_size(igb *g, uint32_t entry, uint32_t size) {
    igb_record *e = &g->entries[entry];
    if (e->nvalues > 1 && e->values[1].size == 4) {
        uint8_t *p = e->values[1].data;
        p[0] = (uint8_t)size;
        p[1] = (uint8_t)(size >> 8);
        p[2] = (uint8_t)(size >> 16);
        p[3] = (uint8_t)(size >> 24);
    }
}

void igb_set_memory(igb *g, int slot, const void *data, uint32_t size) {
    if (slot < 0 || slot >= g->nitems || igb_is_object(g, slot)) return;
    /* Alike entries are shared: a change to one would change every block using it. */
    int shared = 0;
    for (int i = 0; i < g->nitems; i++)
        if (i != slot && g->indexes[i] == g->indexes[slot]) shared = 1;
    if (shared) {
        const int e = add_entry_like(g, g->indexes[slot]);
        if (e < 0) return;
        g->indexes[slot] = (uint32_t)e;
    }
    set_entry_size(g, g->indexes[slot], size);
    free(g->items[slot].memory.data);
    g->items[slot].memory = copy_bytes((const uint8_t *)data, size);
}

static int add_item(igb *g, uint32_t entry) {
    if (g->nitems == g->cap_items) {
        const int bigger = g->cap_items ? g->cap_items * 2 : 64;
        igb_item *items = (igb_item *)realloc(g->items, (size_t)bigger * sizeof *items);
        uint32_t *indexes = (uint32_t *)realloc(g->indexes, (size_t)bigger * sizeof *indexes);
        if (items) g->items = items;
        if (indexes) g->indexes = indexes;
        if (!items || !indexes) return -1;
        g->cap_items = bigger;
    }
    memset(&g->items[g->nitems], 0, sizeof g->items[0]);
    g->indexes[g->nitems] = entry;
    return g->nitems++;
}

int igb_add_memory(igb *g, int like, const void *data, uint32_t size) {
    if (like < 0 || like >= g->nitems || igb_is_object(g, like)) return -1;
    const int e = add_entry_like(g, g->indexes[like]);
    if (e < 0) return -1;
    set_entry_size(g, (uint32_t)e, size);
    const int slot = add_item(g, (uint32_t)e);
    if (slot < 0) return -1;
    g->items[slot].meta = 0xFFFFFFFFu;
    g->items[slot].memory = copy_bytes((const uint8_t *)data, size);
    return slot;
}

int igb_add_object(igb *g, int like) {
    if (!igb_is_object(g, like)) return -1;
    const uint32_t entry = g->indexes[like];   /* an object's entry holds only its class: shared is fine */
    const int slot = add_item(g, entry);
    if (slot < 0) return -1;
    g->items[slot].object = copy_record(&g->items[like].object);
    g->items[slot].meta = g->items[slot].object.meta;
    return slot;
}

/* ---- leaving out what nothing reaches -------------------------------------------------- */

static int is_ref_field(const igb *g, uint16_t field) {
    const char *n = g->meta_fields[field].name;
    return !strcmp(n, "igObjectRefMetaField") || !strcmp(n, "igMemoryRefMetaField");
}

static int is_ref_block(const igb *g, int slot) {
    const char *t = igb_memory_type(g, slot);
    return t && (!strcmp(t, "igObjectRefMetaField") || !strcmp(t, "igMemoryRefMetaField"));
}

int igb_collect(igb *g) {
    if (g->info < 0 || g->info >= g->nitems) return 0;
    uint8_t *seen = (uint8_t *)calloc((size_t)g->nitems, 1);
    int cap = g->nitems + 16;
    int *todo = (int *)malloc((size_t)cap * sizeof(int));
    int *remap = (int *)malloc((size_t)g->nitems * sizeof(int));
    if (!seen || !todo || !remap) { free(seen); free(todo); free(remap); return 0; }
    int n = 0;
    todo[n++] = g->info;
#define PUSH(v) do { if (n == cap) { int *t = (int *)realloc(todo, (size_t)cap * 2 * sizeof(int)); if (!t) break; todo = t; cap *= 2; } todo[n++] = (v); } while (0)
    while (n) {
        const int s = todo[--n];
        if (s < 0 || s >= g->nitems || seen[s]) continue;
        seen[s] = 1;
        if (igb_is_object(g, s)) {
            const igb_record *r = &g->items[s].object;
            const igb_meta_object *m = &g->meta_objects[r->meta];
            for (int k = 0; k < m->nfields && k < r->nvalues; k++)
                if (is_ref_field(g, m->fields[k][0]) && r->values[k].size == 4) PUSH((int32_t)rd32(r->values[k].data));
        } else if (is_ref_block(g, s)) {
            const igb_bytes *b = &g->items[s].memory;
            for (uint32_t i = 0; i + 4 <= b->size; i += 4) PUSH((int32_t)rd32(b->data + i));
        }
    }
#undef PUSH
    int kept = 0;
    for (int s = 0; s < g->nitems; s++) remap[s] = seen[s] ? kept++ : -1;
#define REMAP(v) ((v) >= 0 && (v) < g->nitems ? remap[v] : (v))
    for (int s = 0; s < g->nitems; s++) {
        if (!seen[s]) continue;
        if (igb_is_object(g, s)) {
            igb_record *r = &g->items[s].object;
            const igb_meta_object *m = &g->meta_objects[r->meta];
            for (int k = 0; k < m->nfields && k < r->nvalues; k++)
                if (is_ref_field(g, m->fields[k][0]) && r->values[k].size == 4) {
                    const int32_t v = (int32_t)rd32(r->values[k].data), w = REMAP(v);
                    uint8_t *p = r->values[k].data;
                    p[0] = (uint8_t)w; p[1] = (uint8_t)((uint32_t)w >> 8); p[2] = (uint8_t)((uint32_t)w >> 16); p[3] = (uint8_t)((uint32_t)w >> 24);
                }
        } else if (is_ref_block(g, s)) {
            igb_bytes *b = &g->items[s].memory;
            for (uint32_t i = 0; i + 4 <= b->size; i += 4) {
                const int32_t v = (int32_t)rd32(b->data + i), w = REMAP(v);
                uint8_t *p = b->data + i;
                p[0] = (uint8_t)w; p[1] = (uint8_t)((uint32_t)w >> 8); p[2] = (uint8_t)((uint32_t)w >> 16); p[3] = (uint8_t)((uint32_t)w >> 24);
            }
        }
    }
    /* Entries still used, in their order. */
    int *entry_map = (int *)malloc((size_t)(g->nentries ? g->nentries : 1) * sizeof(int));
    uint8_t *used = (uint8_t *)calloc((size_t)(g->nentries ? g->nentries : 1), 1);
    for (int s = 0; s < g->nitems; s++)
        if (seen[s]) used[g->indexes[s]] = 1;
    int ne = 0;
    for (int e = 0; e < g->nentries; e++) {
        if (used[e]) {
            entry_map[e] = ne;
            g->entries[ne++] = g->entries[e];
        } else {
            entry_map[e] = -1;
            free_record(&g->entries[e]);
        }
    }
    g->nentries = ne;
    int k = 0;
    for (int s = 0; s < g->nitems; s++) {
        if (!seen[s]) {
            free_record(&g->items[s].object);
            free(g->items[s].memory.data);
            continue;
        }
        g->items[k] = g->items[s];
        g->indexes[k] = (uint32_t)entry_map[g->indexes[s]];
        k++;
    }
    const int dropped = g->nitems - k;
    g->info = remap[g->info];
    g->nitems = k;
#undef REMAP
    free(entry_map);
    free(used);
    free(seen);
    free(todo);
    free(remap);
    return dropped;
}

int igb_add_string(igb *g, const char *s) {
    for (int i = 0; i < g->nstrings; i++)
        if (g->strings[i] && !strcmp(g->strings[i], s)) return i;
    char **bigger = (char **)realloc(g->strings, (size_t)(g->nstrings + 1) * sizeof(char *));
    if (!bigger) return -1;
    g->strings = bigger;
    const size_t n = strlen(s) + 1;
    g->strings[g->nstrings] = (char *)malloc(n);
    if (g->strings[g->nstrings]) memcpy(g->strings[g->nstrings], s, n);
    return g->nstrings++;
}

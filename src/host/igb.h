#ifndef DIVA_IGB_H
#define DIVA_IGB_H

/* Alchemy IGB files, the game's models, read into objects and blocks that can be changed and
 * written again. The same layout as Tools/igb.py, which explains it. */

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data;
    uint32_t size;
} igb_bytes;

typedef struct {
    uint32_t meta;
    int nvalues;            /* the class's fields, and one more if bytes follow them */
    igb_bytes *values;
} igb_record;

typedef struct {
    uint32_t meta;          /* an object's class, or -1 for a block of memory */
    igb_record object;
    igb_bytes memory;
} igb_item;

typedef struct {
    char *name;
    uint32_t major, minor;
} igb_meta_field;

typedef struct {
    char *name;
    uint32_t name_len, major, minor, slots;
    int32_t parent;
    int nfields;
    uint16_t (*fields)[3];  /* meta field, slot, size */
} igb_meta_object;

typedef struct {
    uint32_t version, flags;
    int nstrings;
    char **strings;         /* NULL for an empty one */
    igb_bytes strings_tail, alignments, externals, pools, indexes_tail, tail;
    int nmeta_fields, nmeta_objects;
    igb_meta_field *meta_fields;
    igb_meta_object *meta_objects;
    int nentries, cap_entries;
    igb_record *entries;
    int nitems, cap_items;
    uint32_t *indexes;      /* per item, its entry */
    igb_item *items;
    int32_t info;
} igb;

/* NULL if it cannot be read. */
igb *igb_read(const uint8_t *data, size_t size);
/* The file again; free() it. */
uint8_t *igb_write(const igb *g, size_t *size);
void igb_free(igb *g);

const char *igb_class(const igb *g, int slot);        /* an object's class name, or NULL */
const char *igb_memory_type(const igb *g, int slot);  /* a block's meta field name, or NULL */
int igb_is_object(const igb *g, int slot);
/* An object's field: its bytes. */
igb_bytes *igb_field(igb *g, int slot, int k);
int32_t igb_field_i32(const igb *g, int slot, int k);
void igb_set_i32(igb *g, int slot, int k, int32_t v);

/* A block given new contents; it gets a directory entry of its own. */
void igb_set_memory(igb *g, int slot, const void *data, uint32_t size);
/* New ones of the kind of another; return their slot. */
int igb_add_memory(igb *g, int like, const void *data, uint32_t size);
int igb_add_object(igb *g, int like);
/* A string in the strings table (the one already there if it is); returns its index. */
int igb_add_string(igb *g, const char *s);
/* Leaves out what nothing reaches from the info list. Returns how many. */
int igb_collect(igb *g);

#endif

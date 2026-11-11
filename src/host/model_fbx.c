/* Binary FBX files read into a model (model.h).
 *
 * FBX is a tree of nodes, each with a name, properties (numbers, strings, arrays, arrays often
 * zlib-compressed) and children. What a character needs: under Objects, each Geometry's control
 * points, polygons, normals and UVs; each Model's place (Lcl Translation, Rotation, Scaling, with
 * PreRotation and the geometric offset of meshes); each skin Cluster's points and weights and the
 * bone's place when bound (TransformLink); Materials and the Textures they show. Connections say
 * what belongs to what, by the ids that are each object's first property.
 */
#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- inflate: DEFLATE (RFC 1951) inside zlib (RFC 1950) ---------------------------------- */

typedef struct {
    const uint8_t *in;
    size_t n, at;
    uint32_t bits;
    int nbits;
    uint8_t *out;
    size_t want, done;
    int bad;
} inflater;

static int bit(inflater *s, int need) {
    uint32_t v = s->bits;
    while (s->nbits < need) {
        if (s->at >= s->n) { s->bad = 1; return 0; }
        v |= (uint32_t)s->in[s->at++] << s->nbits;
        s->nbits += 8;
    }
    s->bits = v >> need;
    s->nbits -= need;
    return (int)(v & ((1u << need) - 1));
}

typedef struct {
    short count[16];
    short symbol[320];
} huffman;

static int build(huffman *h, const short *lengths, int n) {
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[lengths[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len < 16; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; len++) offs[len + 1] = (short)(offs[len] + h->count[len]);
    for (int i = 0; i < n; i++)
        if (lengths[i]) h->symbol[offs[lengths[i]]++] = (short)i;
    return left;
}

/* One symbol, read a bit at a time down the canonical code. */
static int decode(inflater *s, const huffman *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bit(s, 1);
        const int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
        if (s->bad) return -1;
    }
    s->bad = 1;
    return -1;
}

static const short LBASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short LEXT[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short DBASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const short DEXT[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(inflater *s, const huffman *lencode, const huffman *distcode) {
    for (;;) {
        int sym = decode(s, lencode);
        if (sym < 0 || s->bad) return -1;
        if (sym < 256) {
            if (s->done >= s->want) return -1;
            s->out[s->done++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) return -1;
            const int len = LBASE[sym] + bit(s, LEXT[sym]);
            const int d = decode(s, distcode);
            if (d < 0 || d >= 30) return -1;
            const size_t dist = (size_t)(DBASE[d] + bit(s, DEXT[d]));
            if (dist > s->done || s->done + (size_t)len > s->want) return -1;
            for (int i = 0; i < len; i++, s->done++) s->out[s->done] = s->out[s->done - dist];
        }
    }
}

static int fixed_block(inflater *s) {
    static huffman lencode, distcode;
    static int ready;
    if (!ready) {
        short lengths[320];
        int i = 0;
        for (; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        build(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        build(&distcode, lengths, 30);
        ready = 1;
    }
    return codes(s, &lencode, &distcode);
}

static int dynamic_block(inflater *s) {
    static const short ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = bit(s, 5) + 257, ndist = bit(s, 5) + 1, ncode = bit(s, 4) + 4;
    if (nlen > 286 || ndist > 30) return -1;
    short lengths[320] = {0};
    for (int i = 0; i < ncode; i++) lengths[ORDER[i]] = (short)bit(s, 3);
    huffman lencode, distcode;
    if (build(&lencode, lengths, 19) != 0) return -1;
    int index = 0;
    while (index < nlen + ndist) {
        int sym = decode(s, &lencode);
        if (sym < 0) return -1;
        if (sym < 16) {
            lengths[index++] = (short)sym;
        } else {
            short len = 0;
            int rep;
            if (sym == 16) {
                if (!index) return -1;
                len = lengths[index - 1];
                rep = 3 + bit(s, 2);
            } else if (sym == 17) {
                rep = 3 + bit(s, 3);
            } else {
                rep = 11 + bit(s, 7);
            }
            if (index + rep > nlen + ndist) return -1;
            while (rep--) lengths[index++] = len;
        }
    }
    if (lengths[256] == 0) return -1;
    const int err = build(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return -1;
    const int derr = build(&distcode, lengths + nlen, ndist);
    if (derr < 0 || (derr > 0 && ndist - distcode.count[0] != 1)) return -1;
    return codes(s, &lencode, &distcode);
}

int model_inflate(const uint8_t *in, size_t n, uint8_t *out, size_t want) {
    if (n < 2 || (in[0] & 0x0F) != 8 || ((in[0] << 8) | in[1]) % 31) return 0;
    inflater s = {in, n, 2, 0, 0, out, want, 0, 0};
    int last;
    do {
        last = bit(&s, 1);
        const int type = bit(&s, 2);
        int r;
        if (type == 0) {
            s.bits = 0;
            s.nbits = 0;
            if (s.at + 4 > s.n) return 0;
            const unsigned len = s.in[s.at] | s.in[s.at + 1] << 8;
            s.at += 4;
            if (s.at + len > s.n || s.done + len > s.want) return 0;
            memcpy(s.out + s.done, s.in + s.at, len);
            s.at += len;
            s.done += len;
            r = 0;
        } else if (type == 1) {
            r = fixed_block(&s);
        } else if (type == 2) {
            r = dynamic_block(&s);
        } else {
            r = -1;
        }
        if (r || s.bad) return 0;
    } while (!last);
    return s.done == want;
}

/* ---- the node tree ----------------------------------------------------------------------- */

typedef struct {
    char type;
    const uint8_t *at;          /* where its data begins in the file */
    uint32_t len;               /* strings and raw data: their length; arrays: their count */
    uint32_t encoding, stored;  /* arrays: 1 if compressed, and the bytes in the file */
} prop;

typedef struct node node;
struct node {
    char name[48];
    int nprops;
    prop *props;
    int nchildren;
    node *children;
};

typedef struct {
    const uint8_t *b;
    size_t size;
    int wide;
} reader;

static uint64_t rd(const uint8_t *p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

static void free_node(node *n) {
    for (int i = 0; i < n->nchildren; i++) free_node(&n->children[i]);
    free(n->children);
    free(n->props);
}

/* Reads the node at p into n; returns where the next begins, 0 at the end of a list, -1 if bad. */
static int64_t read_node(const reader *r, size_t p, node *n, int depth) {
    const int w = r->wide ? 8 : 4;
    if (p + 3u * w + 1 > r->size || depth > 64) return -1;
    const uint64_t end = rd(r->b + p, w), nprops = rd(r->b + p + w, w);
    const int nlen = r->b[p + 3 * w];
    p += 3u * w + 1;
    if (end == 0) return 0;
    if (end > r->size || end <= p || nprops > 100000) return -1;
    memset(n, 0, sizeof *n);
    memcpy(n->name, r->b + p, nlen < 47 ? nlen : 47);
    p += nlen;
    n->props = (prop *)calloc(nprops ? nprops : 1, sizeof(prop));
    n->nprops = (int)nprops;
    for (uint64_t i = 0; i < nprops; i++) {
        if (p >= end) return -1;
        prop *q = &n->props[i];
        q->type = (char)r->b[p++];
        q->at = r->b + p;
        switch (q->type) {
        case 'Y': p += 2; break;
        case 'C': p += 1; break;
        case 'I': case 'F': p += 4; break;
        case 'D': case 'L': p += 8; break;
        case 'f': case 'd': case 'l': case 'i': case 'b':
            if (p + 12 > end) return -1;
            q->len = (uint32_t)rd(r->b + p, 4);
            q->encoding = (uint32_t)rd(r->b + p + 4, 4);
            q->stored = (uint32_t)rd(r->b + p + 8, 4);
            p += 12;
            q->at = r->b + p;
            p += q->stored;
            break;
        case 'S': case 'R':
            if (p + 4 > end) return -1;
            q->len = (uint32_t)rd(r->b + p, 4);
            p += 4;
            q->at = r->b + p;
            p += q->len;
            break;
        default: return -1;
        }
        if (p > end) return -1;
    }
    int cap = 0;
    while (p < end) {
        if (n->nchildren == cap) {
            cap = cap ? cap * 2 : 8;
            node *c = (node *)realloc(n->children, (size_t)cap * sizeof(node));
            if (!c) return -1;
            n->children = c;
        }
        const int64_t next = read_node(r, p, &n->children[n->nchildren], depth + 1);
        if (next < 0) return -1;
        if (next == 0) break;
        n->nchildren++;
        p = (size_t)next;
    }
    return (int64_t)end;
}

static node *child(const node *n, const char *name) {
    for (int i = 0; i < n->nchildren; i++)
        if (!strcmp(n->children[i].name, name)) return &n->children[i];
    return NULL;
}

static int64_t prop_int(const prop *q) {
    switch (q->type) {
    case 'Y': return (int16_t)rd(q->at, 2);
    case 'C': return q->at[0];
    case 'I': return (int32_t)rd(q->at, 4);
    case 'L': return (int64_t)rd(q->at, 8);
    case 'F': { uint32_t u = (uint32_t)rd(q->at, 4); float f; memcpy(&f, &u, 4); return (int64_t)f; }
    case 'D': { uint64_t u = rd(q->at, 8); double d; memcpy(&d, &u, 8); return (int64_t)d; }
    default: return 0;
    }
}

static double prop_num(const prop *q) {
    switch (q->type) {
    case 'F': { uint32_t u = (uint32_t)rd(q->at, 4); float f; memcpy(&f, &u, 4); return f; }
    case 'D': { uint64_t u = rd(q->at, 8); double d; memcpy(&d, &u, 8); return d; }
    default: return (double)prop_int(q);
    }
}

/* A string property, with FBX's "name\0\1Class" cut at the name. */
static void prop_str(const prop *q, char *out, size_t size) {
    size_t n = 0;
    if (q->type == 'S')
        while (n < q->len && n + 1 < size && q->at[n]) { out[n] = (char)q->at[n]; n++; }
    out[n] = 0;
}

/* An array property as doubles (any kind of number); free() it. Its count in *count. */
static double *prop_array(const prop *q, size_t *count) {
    *count = 0;
    int size;
    switch (q->type) {
    case 'f': case 'i': size = 4; break;
    case 'd': case 'l': size = 8; break;
    case 'b': size = 1; break;
    default: return NULL;
    }
    const size_t bytes = (size_t)q->len * (size_t)size;
    uint8_t *raw = (uint8_t *)malloc(bytes ? bytes : 1);
    if (!raw) return NULL;
    if (q->encoding == 1) {
        if (!model_inflate(q->at, q->stored, raw, bytes)) { free(raw); return NULL; }
    } else {
        if (q->stored < bytes) { free(raw); return NULL; }
        memcpy(raw, q->at, bytes);
    }
    double *out = (double *)malloc((q->len ? q->len : 1) * sizeof(double));
    for (uint32_t i = 0; out && i < q->len; i++) {
        const uint8_t *p = raw + (size_t)i * size;
        switch (q->type) {
        case 'f': { uint32_t u = (uint32_t)rd(p, 4); float f; memcpy(&f, &u, 4); out[i] = f; break; }
        case 'd': { uint64_t u = rd(p, 8); double d; memcpy(&d, &u, 8); out[i] = d; break; }
        case 'i': out[i] = (int32_t)rd(p, 4); break;
        case 'l': out[i] = (double)(int64_t)rd(p, 8); break;
        default: out[i] = p[0]; break;
        }
    }
    free(raw);
    *count = out ? q->len : 0;
    return out;
}

static double *array_of(const node *n, const char *name, size_t *count) {
    const node *c = child(n, name);
    *count = 0;
    return c && c->nprops ? prop_array(&c->props[0], count) : NULL;
}

static void string_of(const node *n, const char *name, char *out, size_t size) {
    const node *c = child(n, name);
    out[0] = 0;
    if (c && c->nprops) prop_str(&c->props[0], out, size);
}

/* ---- transforms: 4x4, column vectors, columns in order (as FBX writes them) ---------------- */

typedef struct { double m[16]; } mat4;

static mat4 identity(void) { mat4 r = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; return r; }

static mat4 mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            double s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

static mat4 translate(const double t[3]) { mat4 r = identity(); r.m[12] = t[0]; r.m[13] = t[1]; r.m[14] = t[2]; return r; }
static mat4 scale(const double s[3]) { mat4 r = identity(); r.m[0] = s[0]; r.m[5] = s[1]; r.m[10] = s[2]; return r; }

/* Euler angles in degrees, X then Y then Z (FBX's default order). */
static mat4 rotate(const double e[3]) {
    const double x = e[0] * M_PI / 180, y = e[1] * M_PI / 180, z = e[2] * M_PI / 180;
    mat4 rx = identity(), ry = identity(), rz = identity();
    rx.m[5] = cos(x); rx.m[6] = sin(x); rx.m[9] = -sin(x); rx.m[10] = cos(x);
    ry.m[0] = cos(y); ry.m[2] = -sin(y); ry.m[8] = sin(y); ry.m[10] = cos(y);
    rz.m[0] = cos(z); rz.m[1] = sin(z); rz.m[4] = -sin(z); rz.m[5] = cos(z);
    return mul(rz, mul(ry, rx));
}

static void apply(const mat4 *a, const double p[3], double out[3], int as_direction) {
    for (int r = 0; r < 3; r++) out[r] = a->m[r] * p[0] + a->m[4 + r] * p[1] + a->m[8 + r] * p[2] + (as_direction ? 0 : a->m[12 + r]);
}

/* A Model's own Properties70 values. */
typedef struct {
    double t[3], r[3], s[3], pre[3], gt[3], gr[3], gs[3];
} placing;

static void read_placing(const node *model, placing *p) {
    memset(p, 0, sizeof *p);
    p->s[0] = p->s[1] = p->s[2] = p->gs[0] = p->gs[1] = p->gs[2] = 1;
    const node *props = child(model, "Properties70");
    if (!props) return;
    for (int i = 0; i < props->nchildren; i++) {
        const node *q = &props->children[i];
        if (strcmp(q->name, "P") || q->nprops < 7) continue;
        char name[64];
        prop_str(&q->props[0], name, sizeof name);
        double *dst = !strcmp(name, "Lcl Translation") ? p->t : !strcmp(name, "Lcl Rotation") ? p->r : !strcmp(name, "Lcl Scaling") ? p->s
                    : !strcmp(name, "PreRotation") ? p->pre : !strcmp(name, "GeometricTranslation") ? p->gt
                    : !strcmp(name, "GeometricRotation") ? p->gr : !strcmp(name, "GeometricScaling") ? p->gs : NULL;
        if (dst)
            for (int k = 0; k < 3; k++) dst[k] = prop_num(&q->props[4 + k]);
    }
}

/* ---- the objects and their connections ---------------------------------------------------- */

typedef struct {
    int64_t child, parent;
    char property[32];
} link;

typedef struct {
    const node *objects;
    link *links;
    int nlinks;
} scene;

static const node *object(const scene *s, int64_t id) {
    for (int i = 0; i < s->objects->nchildren; i++) {
        const node *o = &s->objects->children[i];
        if (o->nprops && prop_int(&o->props[0]) == id) return o;
    }
    return NULL;
}

static int64_t id_of(const node *o) { return o->nprops ? prop_int(&o->props[0]) : 0; }

/* The first object connected to `parent` with this node name (and, if given, this subtype). */
static const node *linked_child(const scene *s, int64_t parent, const char *name, const char *subtype, int nth) {
    for (int i = 0; i < s->nlinks; i++) {
        if (s->links[i].parent != parent) continue;
        const node *o = object(s, s->links[i].child);
        if (!o || strcmp(o->name, name)) continue;
        if (subtype) {
            char t[32] = "";
            if (o->nprops > 2) prop_str(&o->props[2], t, sizeof t);
            if (strcmp(t, subtype)) continue;
        }
        if (nth-- == 0) return o;
    }
    return NULL;
}

static const node *linked_parent(const scene *s, int64_t childid, const char *name) {
    for (int i = 0; i < s->nlinks; i++) {
        if (s->links[i].child != childid) continue;
        const node *o = object(s, s->links[i].parent);
        if (o && !strcmp(o->name, name)) return o;
    }
    return NULL;
}

static mat4 global_of(const scene *s, const node *model, int depth) {
    placing p;
    read_placing(model, &p);
    const mat4 local = mul(translate(p.t), mul(rotate(p.pre), mul(rotate(p.r), scale(p.s))));
    const node *parent = depth < 64 ? linked_parent(s, id_of(model), "Model") : NULL;
    return parent ? mul(global_of(s, parent, depth + 1), local) : local;
}

static void name_of(const node *o, char *out, size_t size) {
    out[0] = 0;
    if (o->nprops > 1) prop_str(&o->props[1], out, size);
}

/* Values of a layer element for corner k of control point v: by corner or by point, direct or
 * through an index. */
static int layer_index(const node *layer, const double *index, size_t nindex, size_t k, int v) {
    char mapping[40], reference[40];
    string_of(layer, "MappingInformationType", mapping, sizeof mapping);
    string_of(layer, "ReferenceInformationType", reference, sizeof reference);
    size_t i = !strcmp(mapping, "ByPolygonVertex") ? k : (size_t)v;
    if (!strcmp(mapping, "AllSame")) i = 0;
    if (!strcmp(reference, "IndexToDirect") || !strcmp(reference, "Index")) return index && i < nindex ? (int)index[i] : -1;
    return (int)i;
}

int model_load_fbx(const char *path, model *m, char *err, size_t errlen) {
    memset(m, 0, sizeof *m);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errlen, "cannot open it"); return 0; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc(size > 0 ? (size_t)size : 1);
    if (!b || fread(b, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(b); snprintf(err, errlen, "cannot read it"); return 0; }
    fclose(f);
    if (size < 27 || memcmp(b, "Kaydara FBX Binary", 18)) {
        free(b);
        snprintf(err, errlen, "not a binary FBX (an ASCII one can be saved again as binary)");
        return 0;
    }
    reader r = {b, (size_t)size, rd(b + 23, 4) >= 7500};
    node root = {"", 0, NULL, 0, NULL};
    int cap = 0;
    size_t p = 27;
    for (;;) {
        if (root.nchildren == cap) {
            cap = cap ? cap * 2 : 16;
            root.children = (node *)realloc(root.children, (size_t)cap * sizeof(node));
        }
        const int64_t next = read_node(&r, p, &root.children[root.nchildren], 0);
        if (next <= 0) break;
        root.nchildren++;
        p = (size_t)next;
    }
    const node *objects = child(&root, "Objects"), *connections = child(&root, "Connections");
    int ok = 0;
    if (!objects || !connections) {
        snprintf(err, errlen, "no objects in it");
        goto done;
    }
    scene s = {objects, (link *)calloc((size_t)connections->nchildren + 1, sizeof(link)), 0};
    for (int i = 0; i < connections->nchildren; i++) {
        const node *c = &connections->children[i];
        if (strcmp(c->name, "C") || c->nprops < 3) continue;
        link *l = &s.links[s.nlinks++];
        l->child = prop_int(&c->props[1]);
        l->parent = prop_int(&c->props[2]);
        if (c->nprops > 3) prop_str(&c->props[3], l->property, sizeof l->property);
    }

    /* Bones: every Model that is no mesh, where it is bound or else where it is. */
    int nmodels = 0;
    for (int i = 0; i < objects->nchildren; i++)
        if (!strcmp(objects->children[i].name, "Model")) nmodels++;
    m->bones = (model_bone *)calloc((size_t)nmodels + 1, sizeof(model_bone));
    const node **bone_node = (const node **)calloc((size_t)nmodels + 1, sizeof(node *));
    for (int i = 0; i < objects->nchildren; i++) {
        const node *o = &objects->children[i];
        char type[32] = "";
        if (strcmp(o->name, "Model")) continue;
        if (o->nprops > 2) prop_str(&o->props[2], type, sizeof type);
        if (!strcmp(type, "Mesh")) continue;
        model_bone *bn = &m->bones[m->nbones];
        name_of(o, bn->name, sizeof bn->name);
        const mat4 g = global_of(&s, o, 0);
        bn->head[0] = (float)g.m[12];
        bn->head[1] = (float)g.m[13];
        bn->head[2] = (float)g.m[14];
        bone_node[m->nbones++] = o;
    }
    for (int i = 0; i < m->nbones; i++) {
        const node *parent = linked_parent(&s, id_of(bone_node[i]), "Model");
        m->bones[i].parent = -1;
        for (int j = 0; parent && j < m->nbones; j++)
            if (bone_node[j] == parent) m->bones[i].parent = j;
    }

    /* Each mesh: its geometry, materials, and skin. */
    int corner_cap = 0, tri_cap = 0, material_cap = 0;
    for (int i = 0; i < objects->nchildren; i++) {
        const node *mesh = &objects->children[i];
        char type[32] = "";
        if (strcmp(mesh->name, "Model")) continue;
        if (mesh->nprops > 2) prop_str(&mesh->props[2], type, sizeof type);
        if (strcmp(type, "Mesh")) continue;
        const node *geometry = linked_child(&s, id_of(mesh), "Geometry", "Mesh", 0);
        if (!geometry) continue;
        placing pl;
        read_placing(mesh, &pl);
        const mat4 world = mul(global_of(&s, mesh, 0), mul(translate(pl.gt), mul(rotate(pl.gr), scale(pl.gs))));

        size_t npoints, npoly;
        double *points = array_of(geometry, "Vertices", &npoints);
        double *poly = array_of(geometry, "PolygonVertexIndex", &npoly);
        npoints /= 3;
        const node *normal_layer = child(geometry, "LayerElementNormal"), *uv_layer = child(geometry, "LayerElementUV");
        const node *material_layer = child(geometry, "LayerElementMaterial");
        size_t nn = 0, nni = 0, nuv = 0, nuvi = 0, nmat = 0;
        double *normals = normal_layer ? array_of(normal_layer, "Normals", &nn) : NULL;
        double *normal_index = normal_layer ? array_of(normal_layer, "NormalsIndex", &nni) : NULL;
        double *uvs = uv_layer ? array_of(uv_layer, "UV", &nuv) : NULL;
        double *uv_index = uv_layer ? array_of(uv_layer, "UVIndex", &nuvi) : NULL;
        double *mats = material_layer ? array_of(material_layer, "Materials", &nmat) : NULL;

        /* This mesh's materials, in the order its Materials array counts them. */
        const int first_material = m->nmaterials;
        for (int k = 0;; k++) {
            const node *mat = linked_child(&s, id_of(mesh), "Material", NULL, k);
            if (!mat) break;
            if (m->nmaterials == material_cap) {
                material_cap = material_cap ? material_cap * 2 : 8;
                m->picture = realloc(m->picture, (size_t)material_cap * sizeof *m->picture);
            }
            m->picture[m->nmaterials][0] = 0;
            for (int t = 0; t < s.nlinks; t++) {
                if (s.links[t].parent != id_of(mat)) continue;
                const node *tex = object(&s, s.links[t].child);
                if (!tex || strcmp(tex->name, "Texture")) continue;
                if (s.links[t].property[0] && strcmp(s.links[t].property, "DiffuseColor") && strcmp(s.links[t].property, "Maya|baseColor") &&
                    strcmp(s.links[t].property, "3dsMax|Parameters|base_color_map"))
                    continue;
                string_of(tex, "RelativeFilename", m->picture[m->nmaterials], sizeof m->picture[0]);
                if (!m->picture[m->nmaterials][0]) string_of(tex, "FileName", m->picture[m->nmaterials], sizeof m->picture[0]);
                break;
            }
            m->nmaterials++;
        }
        if (m->nmaterials == first_material) {
            if (m->nmaterials == material_cap) {
                material_cap = material_cap ? material_cap * 2 : 8;
                m->picture = realloc(m->picture, (size_t)material_cap * sizeof *m->picture);
            }
            m->picture[m->nmaterials++][0] = 0;
        }

        /* Weights per control point, from the skin's clusters. */
        int (*wb)[MODEL_WEIGHTS] = calloc(npoints ? npoints : 1, sizeof *wb);
        float (*ww)[MODEL_WEIGHTS] = calloc(npoints ? npoints : 1, sizeof *ww);
        for (size_t v = 0; v < npoints; v++)
            for (int k = 0; k < MODEL_WEIGHTS; k++) wb[v][k] = -1;
        const node *skin = linked_child(&s, id_of(geometry), "Deformer", "Skin", 0);
        for (int c = 0; skin; c++) {
            const node *cluster = linked_child(&s, id_of(skin), "Deformer", "Cluster", c);
            if (!cluster) break;
            const node *bone = NULL;
            for (int t = 0; t < s.nlinks && !bone; t++)
                if (s.links[t].parent == id_of(cluster)) {
                    const node *o = object(&s, s.links[t].child);
                    if (o && !strcmp(o->name, "Model")) bone = o;
                }
            int bi = -1;
            for (int j = 0; bone && j < m->nbones; j++)
                if (bone_node[j] == bone) bi = j;
            if (bi < 0) continue;
            size_t nlinkm, nidx, nw;
            double *bind = array_of(cluster, "TransformLink", &nlinkm);
            if (bind && nlinkm == 16) {
                m->bones[bi].head[0] = (float)bind[12];
                m->bones[bi].head[1] = (float)bind[13];
                m->bones[bi].head[2] = (float)bind[14];
            }
            free(bind);
            double *idx = array_of(cluster, "Indexes", &nidx), *wts = array_of(cluster, "Weights", &nw);
            for (size_t k = 0; idx && wts && k < nidx && k < nw; k++) {
                const int v = (int)idx[k];
                if (v < 0 || (size_t)v >= npoints) continue;
                /* An empty place, or else the lightest, if this weight is heavier. */
                int slot = -1;
                for (int q = 0; q < MODEL_WEIGHTS && slot < 0; q++)
                    if (wb[v][q] < 0) slot = q;
                if (slot < 0) {
                    slot = 0;
                    for (int q = 1; q < MODEL_WEIGHTS; q++)
                        if (ww[v][q] < ww[v][slot]) slot = q;
                    if (ww[v][slot] >= (float)wts[k]) continue;
                }
                wb[v][slot] = bi;
                ww[v][slot] = (float)wts[k];
            }
            free(idx);
            free(wts);
        }

        /* Triangles: each polygon as a fan, its corners whole. */
        char material_mapping[40] = "";
        if (material_layer) string_of(material_layer, "MappingInformationType", material_mapping, sizeof material_mapping);
        size_t start = 0, polygon = 0;
        for (size_t k = 0; poly && k < npoly; k++) {
            if (poly[k] >= 0) continue;
            for (size_t t = start + 1; t + 1 <= k; t++) {
                const size_t pick[3] = {start, t, t + 1};
                if (m->ncorners + 3 > corner_cap) {
                    corner_cap = corner_cap ? corner_cap * 2 : 3072;
                    m->corners = realloc(m->corners, (size_t)corner_cap * sizeof *m->corners);
                }
                if ((m->ncorners / 3) + 1 > tri_cap) {
                    tri_cap = tri_cap ? tri_cap * 2 : 1024;
                    m->material = realloc(m->material, (size_t)tri_cap * sizeof *m->material);
                }
                for (int c = 0; c < 3; c++) {
                    const size_t ki = pick[c];
                    const int v = poly[ki] >= 0 ? (int)poly[ki] : ~(int)poly[ki];
                    model_corner *out = &m->corners[m->ncorners + c];
                    memset(out, 0, sizeof *out);
                    if (v >= 0 && (size_t)v < npoints) {
                        double pos[3];
                        apply(&world, points + (size_t)v * 3, pos, 0);
                        for (int a = 0; a < 3; a++) out->pos[a] = (float)pos[a];
                        for (int q = 0; q < MODEL_WEIGHTS; q++) {
                            out->bone[q] = wb[v][q];
                            out->weight[q] = wb[v][q] >= 0 ? ww[v][q] : 0;
                        }
                    } else {
                        for (int q = 0; q < MODEL_WEIGHTS; q++) out->bone[q] = -1;
                    }
                    const int ni = normal_layer ? layer_index(normal_layer, normal_index, nni, ki, v) : -1;
                    if (ni >= 0 && (size_t)ni * 3 + 2 < nn) {
                        double nrm[3];
                        apply(&world, normals + (size_t)ni * 3, nrm, 1);
                        const double len = sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
                        for (int a = 0; a < 3; a++) out->normal[a] = (float)(len > 0 ? nrm[a] / len : 0);
                    } else {
                        out->normal[1] = 1;
                    }
                    const int ui = uv_layer ? layer_index(uv_layer, uv_index, nuvi, ki, v) : -1;
                    if (ui >= 0 && (size_t)ui * 2 + 1 < nuv) {
                        out->uv[0] = (float)uvs[(size_t)ui * 2];
                        out->uv[1] = (float)uvs[(size_t)ui * 2 + 1];
                    }
                }
                int mi = 0;
                if (mats && nmat) mi = !strcmp(material_mapping, "ByPolygon") ? (polygon < nmat ? (int)mats[polygon] : 0) : (int)mats[0];
                if (mi < 0 || first_material + mi >= m->nmaterials) mi = 0;
                m->material[m->ncorners / 3] = first_material + mi;
                m->ncorners += 3;
            }
            start = k + 1;
            polygon++;
        }
        free(points); free(poly); free(normals); free(normal_index); free(uvs); free(uv_index); free(mats);
        free(wb); free(ww);
    }
    free(bone_node);
    free(s.links);
    if (!m->ncorners) {
        snprintf(err, errlen, "no triangles in it");
        goto done;
    }
    ok = 1;
done:
    free_node(&root);
    free(b);
    if (!ok) model_free(m);
    return ok;
}

void model_free(model *m) {
    free(m->corners);
    free(m->material);
    free(m->bones);
    free(m->picture);
    memset(m, 0, sizeof *m);
}

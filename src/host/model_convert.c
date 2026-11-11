/* A model (model.h) made into the game's two files for a module, body and head.
 *
 * The game moves a module by Miku's skeleton (igSkeleton in the songs' motions, its joints known by
 * their inverse bind matrices) and the models in skinNN.igb hold vertices in that skeleton's bind
 * pose, each moved by at most two of its bones, in pieces of at most eight. So the model is put on
 * her skeleton: its bones are matched to hers by name (Hips to hip, Left arm to L_shouler, and so on
 * for the usual namings), it is scaled to her height, and every part is moved onto the bone it is
 * matched to, turned from its bone's direction to hers and stretched along it by the ratio of their
 * lengths. Bones she has not (fingers, hair, cloth) follow their nearest matched ancestor.
 *
 * The files are made from module 0's, which keep everything the game looks for in them: Miku's own
 * parts stay, drawn at a single point, and the model's pieces are added to her body. Her head is
 * kept the same way, drawn at a point: the model brings its own head in its body.
 */
#include "model_convert.h"

#include "igb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Miku's skeleton ---------------------------------------------------------------------- */

typedef struct {
    char name[32];
    int blend;          /* her bone's number among the blend matrices */
    float joint[3];     /* where it is in the bind pose */
} miku_bone;

#define MIKU_BONES 40

static float f32(const uint8_t *p) {
    uint32_t u = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static int32_t s32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24); }

static int read_skeleton(const uint8_t *anim, size_t n, miku_bone *out, int *count) {
    igb *g = igb_read(anim, n);
    if (!g) return 0;
    *count = 0;
    for (int s = 0; s < g->nitems; s++) {
        const char *c = igb_class(g, s);
        if (!c || strcmp(c, "igSkeleton")) continue;
        const int infos = igb_field_i32(g, s, 2), inverse = igb_field_i32(g, s, 3);
        const int list = igb_field_i32(g, infos, 2);
        if (list < 0 || inverse < 0 || igb_is_object(g, list) || igb_is_object(g, inverse)) break;
        const igb_bytes *refs = &g->items[list].memory, *mats = &g->items[inverse].memory;
        for (uint32_t i = 0; i + 4 <= refs->size && *count < MIKU_BONES; i += 4) {
            const int info = s32(refs->data + i);
            const int name = igb_field_i32(g, info, 0), blend = igb_field_i32(g, info, 2);
            if (blend < 0 || (uint32_t)(blend + 1) * 64 > mats->size || name < 0 || name >= g->nstrings || !g->strings[name]) continue;
            miku_bone *b = &out[(*count)++];
            snprintf(b->name, sizeof b->name, "%s", g->strings[name]);
            b->blend = blend;
            /* Row vectors: p R + t = 0 at the joint, so p = -t R^T. */
            const uint8_t *m = mats->data + blend * 64;
            float R[3][3], t[3];
            for (int r = 0; r < 3; r++)
                for (int c2 = 0; c2 < 3; c2++) R[r][c2] = f32(m + (r * 4 + c2) * 4);
            for (int k = 0; k < 3; k++) t[k] = f32(m + (12 + k) * 4);
            for (int j = 0; j < 3; j++) b->joint[j] = -(t[0] * R[j][0] + t[1] * R[j][1] + t[2] * R[j][2]);
        }
        break;
    }
    igb_free(g);
    return *count > 0;
}

static const miku_bone *miku(const miku_bone *bones, int count, const char *name) {
    for (int i = 0; i < count; i++)
        if (!strcmp(bones[i].name, name)) return &bones[i];
    return NULL;
}

/* ---- matching bones by name ----------------------------------------------------------------- */

/* Her bones, and the names a model's may have for each (side words taken out), in order of choice. */
typedef struct {
    const char *miku;
    const char *names[8];
    const char *child;      /* the bone whose direction it is measured towards, hers */
    const char *turns_as;   /* or the bone it turns with */
} bone_rule;

static const bone_rule RULES[] = {
    {"hip", {"hips", "hip", "pelvis", "\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab", NULL}, NULL, NULL},
    {"spine00", {"spine", "spine0", "spine1", "lowerspine", "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab", NULL}, "spine01", NULL},
    {"spine01", {"chest", "upperchest", "spine2", "spine1", "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x32", NULL}, "neck", NULL},
    {"neck", {"neck", "neck1", "\xe9\xa6\x96", NULL}, "head", NULL},
    {"head", {"head", "\xe9\xa0\xad", NULL}, NULL, "neck"},
    {"collar", {"shoulder", "clavicle", "collar", "collarbone", "\xe8\x82\xa9", NULL}, "shouler", NULL},
    {"shouler", {"arm", "upperarm", "uparm", "\xe8\x85\x95", NULL}, "ellbow", NULL},
    {"ellbow", {"elbow", "forearm", "lowerarm", "\xe3\x81\xb2\xe3\x81\x98", NULL}, "hand", NULL},
    {"hand", {"wrist", "hand", "\xe6\x89\x8b\xe9\xa6\x96", NULL}, NULL, "ellbow"},
    {"upleg", {"upleg", "upperleg", "thigh", "leg", "\xe8\xb6\xb3", NULL}, "knee", NULL},
    {"knee", {"knee", "lowerleg", "calf", "shin", "leg", "\xe3\x81\xb2\xe3\x81\x96", NULL}, "foot", NULL},
    {"foot", {"ankle", "foot", "\xe8\xb6\xb3\xe9\xa6\x96", NULL}, "toe", NULL},
    {"toe", {"toe", "toes", "toebase", "toe0", "\xe3\x81\xa4\xe3\x81\xbe\xe5\x85\x88", NULL}, NULL, "foot"},
};
#define NRULES (int)(sizeof RULES / sizeof RULES[0])
static int sided(const char *miku_name) { return strcmp(miku_name, "hip") && strncmp(miku_name, "spine", 5) && strcmp(miku_name, "neck") && strcmp(miku_name, "head"); }

/* A bone's name as compared: lower case, no separators, no "mixamorig:" or similar, and its side
 * (-1 left, 1 right, 0 none) taken out. */
static void stem_of(const char *name, char *out, size_t size, int *side) {
    const char *colon = strrchr(name, ':');
    if (colon) name = colon + 1;
    char low[96];
    size_t n = 0;
    for (const char *p = name; *p && n + 1 < sizeof low; p++) low[n++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
    low[n] = 0;
    *side = 0;
    /* Side words: Left/Right anywhere, Japanese 左/右, or an l or r standing apart at an end. */
    char *w;
    if ((w = strstr(low, "left"))) { *side = -1; memmove(w, w + 4, strlen(w + 4) + 1); }
    else if ((w = strstr(low, "right"))) { *side = 1; memmove(w, w + 5, strlen(w + 5) + 1); }
    else if ((w = strstr(low, "\xe5\xb7\xa6"))) { *side = -1; memmove(w, w + 3, strlen(w + 3) + 1); }
    else if ((w = strstr(low, "\xe5\x8f\xb3"))) { *side = 1; memmove(w, w + 3, strlen(w + 3) + 1); }
    else {
        const size_t len = strlen(low);
        const int sep_end = len >= 2 && (low[len - 2] == '_' || low[len - 2] == '.' || low[len - 2] == ' ' || low[len - 2] == '-');
        const int sep_start = len >= 2 && (low[1] == '_' || low[1] == '.' || low[1] == ' ' || low[1] == '-');
        if (sep_end && (low[len - 1] == 'l' || low[len - 1] == 'r')) { *side = low[len - 1] == 'l' ? -1 : 1; low[len - 2] = 0; }
        else if (sep_start && (low[0] == 'l' || low[0] == 'r')) { *side = low[0] == 'l' ? -1 : 1; memmove(low, low + 2, len - 1); }
    }
    n = 0;
    for (const char *p = low; *p && n + 1 < size; p++)
        if (*p != '_' && *p != '.' && *p != ' ' && *p != '-') out[n++] = *p;
    out[n] = 0;
    /* "bip01 l upperarm" and the like. */
    if (!strncmp(out, "bip01", 5)) memmove(out, out + 5, strlen(out + 5) + 1);
    if (!strncmp(out, "jbipc", 5) || !strncmp(out, "jbipl", 5) || !strncmp(out, "jbipr", 5)) {
        if (out[4] == 'l') *side = -1;
        if (out[4] == 'r') *side = 1;
        memmove(out, out + 5, strlen(out + 5) + 1);
    }
}

/* Her bone's full name for a rule and a side. */
static void miku_name(const bone_rule *r, int side, char *out, size_t size) {
    if (!sided(r->miku)) snprintf(out, size, "%s", r->miku);
    else snprintf(out, size, "%s_%s", side < 0 ? "L" : "R", r->miku);
}

/* ---- small vectors -------------------------------------------------------------------------- */

typedef struct { float x, y, z; } vec;
static vec v3(const float *p) { vec v = {p[0], p[1], p[2]}; return v; }
static vec add(vec a, vec b) { vec v = {a.x + b.x, a.y + b.y, a.z + b.z}; return v; }
static vec sub(vec a, vec b) { vec v = {a.x - b.x, a.y - b.y, a.z - b.z}; return v; }
static vec mulf(vec a, float k) { vec v = {a.x * k, a.y * k, a.z * k}; return v; }
static float dot(vec a, vec b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static vec cross(vec a, vec b) { vec v = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; return v; }
static float len(vec a) { return sqrtf(dot(a, a)); }
static vec norm(vec a) { const float l = len(a); return l > 1e-9f ? mulf(a, 1.0f / l) : a; }

/* The smallest turn taking direction u to v: axis and angle as cos and sin. */
typedef struct { vec axis; float c, s; } turn;
static turn turn_between(vec u, vec v) {
    u = norm(u);
    v = norm(v);
    const vec a = cross(u, v);
    turn t = {norm(a), dot(u, v), len(a)};
    if (t.s < 1e-6f) { t.c = 1; t.s = 0; }
    return t;
}
static vec turned(turn t, vec p) {
    if (t.s == 0 && t.c == 1) return p;
    return add(add(mulf(p, t.c), mulf(cross(t.axis, p), t.s)), mulf(t.axis, dot(t.axis, p) * (1 - t.c)));
}

/* ---- the conversion ------------------------------------------------------------------------- */

typedef struct {
    int miku;           /* her bone's index in the skeleton read, or -1 */
    int rule;
    turn t;
    vec axis;           /* its own direction, if measured */
    float ratio;        /* her bone's length over its */
    int measured;
} placing;

/* Slots of module 0's body file, found by reading it (Tools/igb.py dump), checked by class. */
enum {
    BODY_NODES = 26, LIKE_BMS = 361, LIKE_NODES = 357, LIKE_NODES_MEM = 359, LIKE_PALETTE = 355, LIKE_PALETTE_MEM = 356,
    LIKE_GEOMETRY = 323, EMPTY_NODES = 319, LIKE_GEO_ATTRS = 324, LIKE_GEO_ATTRS_MEM = 325, LIKE_ATTR = 437, LIKE_VA = 306,
    LIKE_COMPS = 320, LIKE_POS = 446, LIKE_NRM = 447, LIKE_COL = 416, LIKE_UV = 448, LIKE_W = 443, LIKE_IX = 417, BODY_IMAGE = 63
};

static int template_is_known(const igb *g) {
    static const struct { int slot; const char *cls; } CHECK[] = {
        {BODY_NODES, "igNodeList"}, {LIKE_BMS, "igBlendMatrixSelect"}, {LIKE_NODES, "igNodeList"}, {LIKE_PALETTE, "igIntList"},
        {LIKE_GEOMETRY, "igGeometry"}, {LIKE_GEO_ATTRS, "igAttrList"}, {LIKE_ATTR, "igGeometryAttr1_5"}, {LIKE_VA, "igVertexArray1_1"},
        {BODY_IMAGE, "igImage"}};
    for (size_t i = 0; i < sizeof CHECK / sizeof CHECK[0]; i++) {
        const char *c = igb_class(g, CHECK[i].slot);
        if (!c || strcmp(c, CHECK[i].cls)) return 0;
    }
    return !igb_is_object(g, LIKE_POS) && !igb_is_object(g, LIKE_W) && !igb_is_object(g, LIKE_IX);
}

/* Miku's own parts, kept but drawn at one point: positions nought, every vertex on one bone. */
static void hide_all(igb *g, int upto) {
    for (int s = 0; s < upto; s++) {
        const char *c = igb_class(g, s);
        if (c && !strcmp(c, "igVertexArray1_1")) {
            const int comps = igb_field_i32(g, s, 0);
            const uint32_t n = (uint32_t)igb_field_i32(g, s, 1);
            if (comps >= 0 && !igb_is_object(g, comps) && g->items[comps].memory.size >= 4) {
                const int pos = s32(g->items[comps].memory.data);
                if (pos >= 0 && !igb_is_object(g, pos)) memset(g->items[pos].memory.data, 0, g->items[pos].memory.size);
            }
            const int w = igb_field_i32(g, s, 5), ix = igb_field_i32(g, s, 6);
            if (w >= 0 && !igb_is_object(g, w) && g->items[w].memory.size == n * 8)
                for (uint32_t i = 0; i < n; i++) {
                    const float one = 1.0f;
                    memcpy(g->items[w].memory.data + i * 8, &one, 4);
                    memset(g->items[w].memory.data + i * 8 + 4, 0, 4);
                }
            if (ix >= 0 && !igb_is_object(g, ix) && g->items[ix].memory.size == n * 2) memset(g->items[ix].memory.data, 0, n * 2);
        } else if (!c) {
            const char *t = igb_memory_type(g, s);
            if (t && !strcmp(t, "igVec3fMetaField")) memset(g->items[s].memory.data, 0, g->items[s].memory.size);
        }
    }
}

static void put_i32(uint8_t *p, int32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)((uint32_t)v >> 8); p[2] = (uint8_t)((uint32_t)v >> 16); p[3] = (uint8_t)((uint32_t)v >> 24); }

/* ---- the picture: at most 256 across, a power of two, in 256 colours ------------------------ */

typedef struct { uint8_t c[4]; } rgba;

static int cmp_channel;
static int by_channel(const void *a, const void *b) { return ((const rgba *)a)->c[cmp_channel] - ((const rgba *)b)->c[cmp_channel]; }

/* Median cut over the picture's colours. */
static void palette_of(rgba *colours, int n, rgba *palette, int *count) {
    typedef struct { int start, n; } box;
    box boxes[256];
    int nb = 1;
    boxes[0].start = 0;
    boxes[0].n = n;
    while (nb < 256) {
        int pick = -1, best = 1;
        for (int i = 0; i < nb; i++)
            if (boxes[i].n > best) { best = boxes[i].n; pick = i; }
        if (pick < 0) break;
        box *b = &boxes[pick];
        int lo[4] = {255, 255, 255, 255}, hi[4] = {0};
        for (int i = b->start; i < b->start + b->n; i++)
            for (int k = 0; k < 4; k++) {
                if (colours[i].c[k] < lo[k]) lo[k] = colours[i].c[k];
                if (colours[i].c[k] > hi[k]) hi[k] = colours[i].c[k];
            }
        int k = 0;
        for (int q = 1; q < 4; q++)
            if (hi[q] - lo[q] > hi[k] - lo[k]) k = q;
        if (hi[k] == lo[k]) { b->n = -b->n; continue; }   /* one colour: never split */
        cmp_channel = k;
        qsort(colours + b->start, (size_t)b->n, sizeof(rgba), by_channel);
        const int half = b->n / 2;
        boxes[nb].start = b->start + half;
        boxes[nb].n = b->n - half;
        b->n = half;
        nb++;
    }
    *count = nb;
    for (int i = 0; i < nb; i++) {
        const int start = boxes[i].start, cnt = boxes[i].n < 0 ? -boxes[i].n : boxes[i].n;
        long sum[4] = {0};
        for (int j = start; j < start + cnt; j++)
            for (int k = 0; k < 4; k++) sum[k] += colours[j].c[k];
        for (int k = 0; k < 4; k++) palette[i].c[k] = (uint8_t)(cnt ? sum[k] / cnt : 0);
    }
}

static int pow2_at_most(unsigned v, unsigned most) {
    unsigned p = 1;
    while (p * 2 <= v && p * 2 <= most) p *= 2;
    return (int)p;
}

/* The picture scaled down (by averaging) and made 8 bit with a palette. */
static int make_texture(const uint8_t *pixels, unsigned w, unsigned h, int *tw, int *th, uint8_t **indices, uint8_t clut[1024]) {
    *tw = pow2_at_most(w, 256);
    *th = pow2_at_most(h, 256);
    const int W = *tw, H = *th;
    rgba *small = (rgba *)malloc((size_t)W * H * sizeof(rgba));
    rgba *sorted = (rgba *)malloc((size_t)W * H * sizeof(rgba));
    *indices = (uint8_t *)malloc((size_t)W * H);
    if (!small || !sorted || !*indices) { free(small); free(sorted); free(*indices); return 0; }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const unsigned x0 = (unsigned)x * w / W, x1 = ((unsigned)x + 1) * w / W, y0 = (unsigned)y * h / H, y1 = ((unsigned)y + 1) * h / H;
            unsigned long sum[4] = {0}, n = 0;
            for (unsigned yy = y0; yy < (y1 > y0 ? y1 : y0 + 1); yy++)
                for (unsigned xx = x0; xx < (x1 > x0 ? x1 : x0 + 1); xx++, n++)
                    for (int k = 0; k < 4; k++) sum[k] += pixels[((size_t)yy * w + xx) * 4 + k];
            for (int k = 0; k < 4; k++) small[y * W + x].c[k] = (uint8_t)(sum[k] / (n ? n : 1));
        }
    memcpy(sorted, small, (size_t)W * H * sizeof(rgba));
    rgba palette[256];
    int count;
    palette_of(sorted, W * H, palette, &count);
    for (int i = count; i < 256; i++) palette[i] = palette[0];
    /* Each pixel its nearest colour, remembered for colours seen before. */
    for (int i = 0; i < W * H; i++) {
        int best = 0, bestd = 1 << 30;
        for (int p = 0; p < count; p++) {
            int d = 0;
            for (int k = 0; k < 4; k++) {
                const int e = small[i].c[k] - palette[p].c[k];
                d += e * e;
            }
            if (d < bestd) { bestd = d; best = p; if (!d) break; }
        }
        (*indices)[i] = (uint8_t)best;
    }
    for (int i = 0; i < 256; i++) memcpy(clut + i * 4, palette[i].c, 4);
    free(small);
    free(sorted);
    return 1;
}

/* ---- the whole --------------------------------------------------------------------------------- */

int model_convert(const model *m, const uint8_t *pixels, unsigned pw, unsigned ph, const model_templates *t, uint8_t **body, size_t *body_size,
                  uint8_t **head, size_t *head_size, char *err, size_t errlen) {
    miku_bone mb[MIKU_BONES];
    int nmb = 0;
    *body = *head = NULL;
    if (!read_skeleton(t->anim, t->anim_size, mb, &nmb)) { snprintf(err, errlen, "Miku's skeleton not found in the game's motions"); return 0; }

    /* Its bones matched to hers. */
    placing *pl = (placing *)calloc((size_t)m->nbones + 1, sizeof(placing));
    for (int i = 0; i < m->nbones; i++) pl[i].miku = pl[i].rule = -1;
    int matched[NRULES][2];
    memset(matched, -1, sizeof matched);
    for (int r = 0; r < NRULES; r++)
        for (int k = 0; RULES[r].names[k]; k++)
            for (int i = 0; i < m->nbones; i++) {
                if (pl[i].rule >= 0) continue;
                char stem[96];
                int side;
                stem_of(m->bones[i].name, stem, sizeof stem, &side);
                if (strcmp(stem, RULES[r].names[k])) continue;
                if (sided(RULES[r].miku) != (side != 0)) continue;
                const int which = side > 0;
                if (matched[r][which] >= 0) continue;
                char name[32];
                miku_name(&RULES[r], side, name, sizeof name);
                const miku_bone *b = miku(mb, nmb, name);
                if (!b) continue;
                matched[r][which] = i;
                pl[i].miku = (int)(b - mb);
                pl[i].rule = r;
            }
    static const char *const NEEDED[] = {"hip", "head", "shouler", "ellbow", "upleg", "knee"};
    for (size_t k = 0; k < sizeof NEEDED / sizeof NEEDED[0]; k++)
        for (int r = 0; r < NRULES; r++)
            if (!strcmp(RULES[r].miku, NEEDED[k]) && (matched[r][0] < 0 || (sided(NEEDED[k]) && matched[r][1] < 0))) {
                snprintf(err, errlen, "no bone found for Miku's %s (bones named as in Mixamo, MMD or Blender's rigs are known)", NEEDED[k]);
                free(pl);
                return 0;
            }

    /* Her height: its head's height above its lowest point. */
    float low = 1e30f;
    for (int c = 0; c < m->ncorners; c++)
        if (m->corners[c].pos[1] < low) low = m->corners[c].pos[1];
    int head_bone = -1;
    for (int r = 0; r < NRULES; r++)
        if (!strcmp(RULES[r].miku, "head")) head_bone = matched[r][0];
    const float model_height = m->bones[head_bone].head[1] - low;
    const float miku_height = miku(mb, nmb, "head")->joint[1];
    if (model_height <= 0) { snprintf(err, errlen, "its head is not above its feet"); free(pl); return 0; }
    const float k = miku_height / model_height;
    const vec floor_shift = {0, -low, 0};
    vec *J = (vec *)malloc((size_t)m->nbones * sizeof(vec));
    for (int i = 0; i < m->nbones; i++) J[i] = mulf(add(v3(m->bones[i].head), floor_shift), k);

    /* Each matched bone's turn and stretch, towards its child's joint. */
    for (int i = 0; i < m->nbones; i++) {
        if (pl[i].rule < 0 || !RULES[pl[i].rule].child) continue;
        char child_name[32];
        const int side = !strncmp(mb[pl[i].miku].name, "R_", 2) ? 1 : -1;
        bone_rule child_rule = {RULES[pl[i].rule].child, {NULL}, NULL, NULL};
        miku_name(&child_rule, side, child_name, sizeof child_name);
        int child = -1;
        for (int j = 0; j < m->nbones; j++)
            if (pl[j].miku >= 0 && !strcmp(mb[pl[j].miku].name, child_name)) child = j;
        const miku_bone *hc = miku(mb, nmb, child_name);
        if (child < 0 || !hc) continue;
        const vec d = sub(J[child], J[i]), d2 = sub(v3(hc->joint), v3(mb[pl[i].miku].joint));
        if (len(d) < 1e-6f) continue;
        pl[i].t = turn_between(d, d2);
        pl[i].axis = norm(d);
        pl[i].ratio = len(d2) / len(d);
        pl[i].measured = 1;
    }
    for (int i = 0; i < m->nbones; i++) {
        if (pl[i].rule < 0 || pl[i].measured) continue;
        pl[i].t.c = 1;
        pl[i].ratio = 1;
        const char *as = RULES[pl[i].rule].turns_as;
        if (!as) continue;
        char name[32];
        const int side = !strncmp(mb[pl[i].miku].name, "R_", 2) ? 1 : -1;
        bone_rule r2 = {as, {NULL}, NULL, NULL};
        miku_name(&r2, side, name, sizeof name);
        for (int j = 0; j < m->nbones; j++)
            if (pl[j].miku >= 0 && pl[j].measured && !strcmp(mb[pl[j].miku].name, name)) pl[i].t = pl[j].t;
    }
    /* What each bone follows: itself if matched, or its nearest matched ancestor. */
    int *follows = (int *)malloc((size_t)m->nbones * sizeof(int));
    for (int i = 0; i < m->nbones; i++) {
        int b = i, guard = 0;
        while (b >= 0 && pl[b].miku < 0 && guard++ < 256) b = m->bones[b].parent;
        follows[i] = b >= 0 && pl[b].miku >= 0 ? b : matched[0][0];
    }

    /* The corners, on her skeleton. */
    typedef struct { float pos[3], normal[3], uv[2], w[2]; int blend[2]; } corner;
    corner *cs = (corner *)calloc((size_t)m->ncorners, sizeof(corner));
    for (int c = 0; c < m->ncorners; c++) {
        const model_corner *src = &m->corners[c];
        const vec p = mulf(add(v3(src->pos), floor_shift), k), n = v3(src->normal);
        float wsum[MIKU_BONES] = {0};
        int bones[MODEL_WEIGHTS];
        float weights[MODEL_WEIGHTS], total = 0;
        int nb = 0;
        for (int q = 0; q < MODEL_WEIGHTS; q++) {
            if (src->bone[q] < 0 || src->weight[q] <= 0) continue;
            const int b = follows[src->bone[q]];
            if (b < 0) continue;
            int at = -1;
            for (int z = 0; z < nb; z++)
                if (bones[z] == b) at = z;
            if (at < 0) { at = nb++; bones[at] = b; weights[at] = 0; }
            weights[at] += src->weight[q];
            total += src->weight[q];
        }
        if (!nb) { bones[0] = matched[0][0]; weights[0] = 1; nb = 1; total = 1; }
        vec pos = {0, 0, 0}, nrm = {0, 0, 0};
        for (int z = 0; z < nb; z++) {
            const placing *P = &pl[bones[z]];
            vec rel = sub(p, J[bones[z]]);
            if (P->measured) {
                const float along = dot(rel, P->axis);
                rel = add(mulf(P->axis, along * P->ratio), sub(rel, mulf(P->axis, along)));
            }
            const float share = weights[z] / total;
            pos = add(pos, mulf(add(v3(mb[P->miku].joint), turned(P->t, rel)), share));
            nrm = add(nrm, mulf(turned(P->t, n), share));
            wsum[P->miku] += share;
        }
        /* Her two heaviest bones. */
        int b1 = -1, b2 = -1;
        for (int z = 0; z < nmb; z++) {
            if (wsum[z] <= 0) continue;
            if (b1 < 0 || wsum[z] > wsum[b1]) { b2 = b1; b1 = z; }
            else if (b2 < 0 || wsum[z] > wsum[b2]) b2 = z;
        }
        const float s = wsum[b1] + (b2 >= 0 ? wsum[b2] : 0);
        corner *o = &cs[c];
        o->pos[0] = pos.x; o->pos[1] = pos.y; o->pos[2] = pos.z;
        nrm = norm(nrm);
        o->normal[0] = nrm.x; o->normal[1] = nrm.y; o->normal[2] = nrm.z;
        o->uv[0] = src->uv[0];
        o->uv[1] = 1.0f - src->uv[1];
        o->blend[0] = mb[b1].blend;
        o->w[0] = wsum[b1] / s;
        o->blend[1] = b2 >= 0 ? mb[b2].blend : mb[b1].blend;
        o->w[1] = b2 >= 0 ? wsum[b2] / s : 0;
    }
    free(J);
    free(follows);
    free(pl);

    /* Into module 0's body. */
    igb *g = igb_read(t->body, t->body_size);
    if (!g || !template_is_known(g)) {
        snprintf(err, errlen, "module 0's body is not as expected");
        igb_free(g);
        free(cs);
        return 0;
    }
    const int original = g->nitems;
    hide_all(g, original);

    /* Pieces of at most eight of her bones each, triangles taken in order. */
    const int ntri = m->ncorners / 3;
    int *piece_of = (int *)malloc((size_t)(ntri ? ntri : 1) * sizeof(int));
    int (*piece_bones)[8] = NULL;
    int *piece_nb = NULL, npieces = 0, cap = 0;
    for (int tri = 0; tri < ntri; tri++) {
        int want[6], nw = 0;
        for (int c = 0; c < 3; c++)
            for (int q = 0; q < 2; q++) {
                const corner *o = &cs[tri * 3 + c];
                if (q == 1 && o->w[1] <= 0) continue;
                int seen = 0;
                for (int z = 0; z < nw; z++)
                    if (want[z] == o->blend[q]) seen = 1;
                if (!seen) want[nw++] = o->blend[q];
            }
        int found = -1;
        for (int p = 0; p < npieces && found < 0; p++) {
            int total = piece_nb[p];
            for (int z = 0; z < nw; z++) {
                int has = 0;
                for (int y = 0; y < piece_nb[p]; y++)
                    if (piece_bones[p][y] == want[z]) has = 1;
                if (!has) total++;
            }
            if (total <= 8) found = p;
        }
        if (found < 0) {
            if (npieces == cap) {
                cap = cap ? cap * 2 : 8;
                piece_bones = realloc(piece_bones, (size_t)cap * sizeof *piece_bones);
                piece_nb = realloc(piece_nb, (size_t)cap * sizeof *piece_nb);
            }
            found = npieces++;
            piece_nb[found] = 0;
        }
        for (int z = 0; z < nw; z++) {
            int has = 0;
            for (int y = 0; y < piece_nb[found]; y++)
                if (piece_bones[found][y] == want[z]) has = 1;
            if (!has) piece_bones[found][piece_nb[found]++] = want[z];
        }
        piece_of[tri] = found;
    }

    int *new_bms = (int *)malloc((size_t)(npieces ? npieces : 1) * sizeof(int));
    for (int p = 0; p < npieces; p++) {
        int n = 0;
        for (int tri = 0; tri < ntri; tri++)
            if (piece_of[tri] == p) n += 3;
        float *pos = malloc((size_t)n * 12), *nrm = malloc((size_t)n * 12), *uv = malloc((size_t)n * 8), *w = malloc((size_t)n * 8);
        uint8_t *col = malloc((size_t)n * 4), *ix = malloc((size_t)n * 2);
        int at = 0;
        for (int tri = 0; tri < ntri; tri++) {
            if (piece_of[tri] != p) continue;
            for (int c = 0; c < 3; c++, at++) {
                const corner *o = &cs[tri * 3 + c];
                memcpy(pos + at * 3, o->pos, 12);
                memcpy(nrm + at * 3, o->normal, 12);
                memcpy(uv + at * 2, o->uv, 8);
                memcpy(w + at * 2, o->w, 8);
                memset(col + at * 4, 0xFF, 4);
                for (int q = 0; q < 2; q++) {
                    int y = 0;
                    while (y < piece_nb[p] && piece_bones[p][y] != o->blend[q]) y++;
                    ix[at * 2 + q] = (uint8_t)(y < piece_nb[p] ? y : 0);
                }
            }
        }
        const int m_pos = igb_add_memory(g, LIKE_POS, pos, (uint32_t)n * 12), m_nrm = igb_add_memory(g, LIKE_NRM, nrm, (uint32_t)n * 12);
        const int m_col = igb_add_memory(g, LIKE_COL, col, (uint32_t)n * 4), m_uv = igb_add_memory(g, LIKE_UV, uv, (uint32_t)n * 8);
        const int m_w = igb_add_memory(g, LIKE_W, w, (uint32_t)n * 8), m_ix = igb_add_memory(g, LIKE_IX, ix, (uint32_t)n * 2);
        free(pos); free(nrm); free(uv); free(w); free(col); free(ix);
        uint8_t comps[80];
        for (int z = 0; z < 20; z++) put_i32(comps + z * 4, -1);
        put_i32(comps + 0, m_pos);
        put_i32(comps + 4, m_nrm);
        put_i32(comps + 8, m_col);
        put_i32(comps + 44, m_uv);
        const int m_comps = igb_add_memory(g, LIKE_COMPS, comps, 80);
        const int va = igb_add_object(g, LIKE_VA);
        igb_set_i32(g, va, 0, m_comps);
        igb_set_i32(g, va, 1, n);
        igb_set_i32(g, va, 5, m_w);
        igb_set_i32(g, va, 6, m_ix);
        const int attr = igb_add_object(g, LIKE_ATTR);
        igb_set_i32(g, attr, 1, va);
        igb_set_i32(g, attr, 3, 3);          /* triangles */
        igb_set_i32(g, attr, 4, n / 3);
        igb_set_i32(g, attr, 10, -1);
        uint8_t ref[4];
        put_i32(ref, attr);
        const int m_ga = igb_add_memory(g, LIKE_GEO_ATTRS_MEM, ref, 4);
        const int ga = igb_add_object(g, LIKE_GEO_ATTRS);
        igb_set_i32(g, ga, 0, 1); igb_set_i32(g, ga, 1, 1); igb_set_i32(g, ga, 2, m_ga);
        const int geometry = igb_add_object(g, LIKE_GEOMETRY);
        igb_set_i32(g, geometry, 3, EMPTY_NODES);
        igb_set_i32(g, geometry, 4, ga);
        put_i32(ref, geometry);
        const int m_nodes = igb_add_memory(g, LIKE_NODES_MEM, ref, 4);
        const int nodes = igb_add_object(g, LIKE_NODES);
        igb_set_i32(g, nodes, 0, 1); igb_set_i32(g, nodes, 1, 1); igb_set_i32(g, nodes, 2, m_nodes);
        uint8_t palette[32];
        for (int y = 0; y < piece_nb[p]; y++) put_i32(palette + y * 4, piece_bones[p][y]);
        const int m_pal = igb_add_memory(g, LIKE_PALETTE_MEM, palette, (uint32_t)piece_nb[p] * 4);
        const int pal = igb_add_object(g, LIKE_PALETTE);
        igb_set_i32(g, pal, 0, piece_nb[p]); igb_set_i32(g, pal, 1, piece_nb[p]); igb_set_i32(g, pal, 2, m_pal);
        const int bms = igb_add_object(g, LIKE_BMS);
        igb_set_i32(g, bms, 3, nodes);
        igb_set_i32(g, bms, 6, pal);
        new_bms[p] = bms;
    }
    free(piece_of);
    free(piece_bones);
    free(piece_nb);
    free(cs);

    /* body3's children: hers, then the pieces. */
    const int old_count = igb_field_i32(g, BODY_NODES, 0), old_list = igb_field_i32(g, BODY_NODES, 2);
    const int kids = old_count + npieces;
    uint8_t *list = (uint8_t *)malloc((size_t)kids * 4);
    if (old_list >= 0 && !igb_is_object(g, old_list) && g->items[old_list].memory.size >= (uint32_t)old_count * 4)
        memcpy(list, g->items[old_list].memory.data, (size_t)old_count * 4);
    for (int p = 0; p < npieces; p++) put_i32(list + (old_count + p) * 4, new_bms[p]);
    const int m_body = igb_add_memory(g, LIKE_NODES_MEM, list, (uint32_t)kids * 4);
    igb_set_i32(g, BODY_NODES, 0, kids);
    igb_set_i32(g, BODY_NODES, 1, kids);
    igb_set_i32(g, BODY_NODES, 2, m_body);
    free(list);
    free(new_bms);

    /* The picture body3 shows. */
    if (pixels) {
        int tw, th;
        uint8_t *indices, clut[1024];
        if (make_texture(pixels, pw, ph, &tw, &th, &indices, clut)) {
            igb_set_i32(g, BODY_IMAGE, 0, tw);
            igb_set_i32(g, BODY_IMAGE, 1, th);
            igb_set_i32(g, BODY_IMAGE, 10, tw * th);
            igb_set_i32(g, BODY_IMAGE, 17, tw);
            igb_set_memory(g, igb_field_i32(g, BODY_IMAGE, 11), indices, (uint32_t)(tw * th));
            const int clut_obj = igb_field_i32(g, BODY_IMAGE, 15);
            igb_set_memory(g, igb_field_i32(g, clut_obj, 3), clut, 1024);
            free(indices);
        }
    }
    igb_collect(g);
    *body = igb_write(g, body_size);
    igb_free(g);

    /* Her head, drawn at a point. */
    igb *hg = igb_read(t->head, t->head_size);
    if (!hg) { snprintf(err, errlen, "module 0's head is not as expected"); free(*body); *body = NULL; return 0; }
    hide_all(hg, hg->nitems);
    *head = igb_write(hg, head_size);
    igb_free(hg);
    return *body && *head;
}

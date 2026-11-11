/* PMX files (MikuMikuDance's models, versions 2.0 and 2.1) read into a model (model.h).
 *
 * A header says how texts are written (UTF-16 or UTF-8) and how many bytes each kind of index
 * takes; then the vertices, each with its place, normal, UV and up to four bones (BDEF1, BDEF2,
 * BDEF4, SDEF taken as BDEF2, QDEF as BDEF4), the triangles, the textures' paths, the materials
 * (each owning the next so many triangle corners), and the bones, each with its place and parent.
 * MMD's space is left-handed with the model facing -Z; it is made right-handed facing +Z by
 * turning Z round, and each triangle's corners are put the other way round to keep its front.
 */
#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct {
    const uint8_t *b;
    size_t size, at;
    int bad;
    int utf8, extra_uv, vertex_index, texture_index, material_index, bone_index;
} pmx;

static const uint8_t *take(pmx *p, size_t n) {
    if (p->bad || p->at + n > p->size) { p->bad = 1; return NULL; }
    const uint8_t *q = p->b + p->at;
    p->at += n;
    return q;
}

static int32_t i32(pmx *p) { const uint8_t *q = take(p, 4); return q ? (int32_t)((uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16 | (uint32_t)q[3] << 24) : 0; }
static float f32(pmx *p) { int32_t v = i32(p); float f; memcpy(&f, &v, 4); return f; }
static int u8(pmx *p) { const uint8_t *q = take(p, 1); return q ? q[0] : 0; }

/* An index of `size` bytes: vertex indices are unsigned, the others signed (-1 for none). */
static int32_t index_of(pmx *p, int size, int is_vertex) {
    const uint8_t *q = take(p, (size_t)size);
    if (!q) return -1;
    if (size == 1) return is_vertex ? q[0] : (int8_t)q[0];
    if (size == 2) return is_vertex ? (int32_t)(q[0] | q[1] << 8) : (int16_t)(q[0] | q[1] << 8);
    return (int32_t)((uint32_t)q[0] | (uint32_t)q[1] << 8 | (uint32_t)q[2] << 16 | (uint32_t)q[3] << 24);
}

/* A text, made UTF-8. */
static void text(pmx *p, char *out, size_t size) {
    const int32_t n = i32(p);
    out[0] = 0;
    if (n < 0) { p->bad = 1; return; }
    const uint8_t *q = take(p, (size_t)n);
    if (!q || !size) return;
    if (p->utf8) {
        const size_t k = (size_t)n < size - 1 ? (size_t)n : size - 1;
        memcpy(out, q, k);
        out[k] = 0;
    } else {
        wchar_t wide[512];
        const int chars = n / 2 < 511 ? n / 2 : 511;
        memcpy(wide, q, (size_t)chars * 2);
        wide[chars] = 0;
        if (!WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)size, NULL, NULL)) out[0] = 0;
    }
}

static void skip_text(pmx *p) {
    const int32_t n = i32(p);
    if (n < 0) p->bad = 1;
    else take(p, (size_t)n);
}

int model_load_pmx(const char *path, model *m, char *err, size_t errlen) {
    memset(m, 0, sizeof *m);
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errlen, "cannot open it"); return 0; }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc(size > 0 ? (size_t)size : 1);
    if (!b || fread(b, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(b); snprintf(err, errlen, "cannot read it"); return 0; }
    fclose(f);
    pmx p = {b, (size_t)size, 0, 0, 0, 0, 0, 0, 0, 0};
    int ok = 0;
    float *pos = NULL, *nrm = NULL, *uv = NULL, *wt = NULL;
    int *bone = NULL, *faces = NULL;
    const uint8_t *magic = take(&p, 4);
    if (!magic || memcmp(magic, "PMX ", 4)) { snprintf(err, errlen, "not a PMX file (a PMD one can be saved as PMX in PMX Editor)"); goto done; }
    f32(&p);                                    /* version */
    const int nglobals = u8(&p);
    const uint8_t *g = take(&p, (size_t)nglobals);
    if (!g || nglobals < 8) { snprintf(err, errlen, "its header is not as expected"); goto done; }
    p.utf8 = g[0] == 1;
    p.extra_uv = g[1];
    p.vertex_index = g[2];
    p.texture_index = g[3];
    p.material_index = g[4];
    p.bone_index = g[5];
    for (int i = 0; i < 4; i++) skip_text(&p);  /* names and comments */

    /* Vertices. */
    const int32_t nv = i32(&p);
    if (nv <= 0 || nv > 10000000) { snprintf(err, errlen, "no vertices in it"); goto done; }
    pos = malloc((size_t)nv * 12); nrm = malloc((size_t)nv * 12); uv = malloc((size_t)nv * 8);
    wt = malloc((size_t)nv * 16); bone = malloc((size_t)nv * 16);
    if (!pos || !nrm || !uv || !wt || !bone) { snprintf(err, errlen, "out of memory"); goto done; }
    for (int32_t v = 0; v < nv && !p.bad; v++) {
        for (int k = 0; k < 3; k++) pos[v * 3 + k] = f32(&p);
        for (int k = 0; k < 3; k++) nrm[v * 3 + k] = f32(&p);
        uv[v * 2] = f32(&p);
        uv[v * 2 + 1] = f32(&p);
        take(&p, (size_t)p.extra_uv * 16);
        int *bi = bone + v * 4;
        float *w = wt + v * 4;
        for (int k = 0; k < 4; k++) { bi[k] = -1; w[k] = 0; }
        const int kind = u8(&p);
        if (kind == 0) {
            bi[0] = index_of(&p, p.bone_index, 0);
            w[0] = 1;
        } else if (kind == 1 || kind == 3) {
            bi[0] = index_of(&p, p.bone_index, 0);
            bi[1] = index_of(&p, p.bone_index, 0);
            w[0] = f32(&p);
            w[1] = 1 - w[0];
            if (kind == 3) take(&p, 36);    /* SDEF's C, R0, R1: taken as BDEF2 */
        } else if (kind == 2 || kind == 4) {
            for (int k = 0; k < 4; k++) bi[k] = index_of(&p, p.bone_index, 0);
            for (int k = 0; k < 4; k++) w[k] = f32(&p);
        } else {
            p.bad = 1;
        }
        f32(&p);                                /* edge scale */
    }

    /* Triangles: corners, three to one. */
    const int32_t nf = i32(&p);
    if (nf <= 0 || nf % 3 || p.bad) { snprintf(err, errlen, "no triangles in it"); goto done; }
    faces = malloc((size_t)nf * sizeof(int));
    for (int32_t i = 0; i < nf && !p.bad; i++) faces[i] = index_of(&p, p.vertex_index, 1);

    /* Textures' paths. */
    const int32_t nt = i32(&p);
    char (*textures)[520] = calloc((size_t)(nt > 0 ? nt : 1), sizeof *textures);
    for (int32_t i = 0; i < nt && !p.bad; i++) text(&p, textures[i], sizeof textures[0]);

    /* Materials: each its texture and how many corners follow. */
    const int32_t nm = i32(&p);
    int *mat_corners = calloc((size_t)(nm > 0 ? nm : 1), sizeof(int));
    uint8_t *hidden = calloc((size_t)(nm > 0 ? nm : 1), 1);
    m->picture = calloc((size_t)(nm > 0 ? nm : 1), sizeof *m->picture);
    m->nmaterials = nm > 0 ? nm : 0;
    for (int32_t i = 0; i < nm && !p.bad; i++) {
        skip_text(&p);
        skip_text(&p);
        take(&p, 12);                               /* diffuse colour */
        /* Its alpha: nought for parts shown only by a morph (blushes, signs on the face). */
        hidden[i] = f32(&p) <= 0.0f;
        take(&p, 12 + 4 + 12 + 1 + 16 + 4);        /* specular, ambient, flags, edge */
        const int32_t tex = index_of(&p, p.texture_index, 0);
        index_of(&p, p.texture_index, 0);           /* sphere map */
        u8(&p);                                     /* sphere mode */
        const int shared_toon = u8(&p);
        if (shared_toon) u8(&p);
        else index_of(&p, p.texture_index, 0);
        skip_text(&p);                              /* memo */
        mat_corners[i] = i32(&p);
        if (tex >= 0 && tex < nt) snprintf(m->picture[i], sizeof m->picture[0], "%s", textures[tex]);
    }
    free(textures);

    /* Bones. */
    const int32_t nb = i32(&p);
    m->bones = calloc((size_t)(nb > 0 ? nb : 1), sizeof(model_bone));
    for (int32_t i = 0; i < nb && !p.bad; i++) {
        model_bone *bn = &m->bones[i];
        text(&p, bn->name, sizeof bn->name);
        skip_text(&p);
        bn->head[0] = f32(&p);
        bn->head[1] = f32(&p);
        bn->head[2] = -f32(&p);
        bn->parent = index_of(&p, p.bone_index, 0);
        i32(&p);                                    /* layer */
        const int flags = u8(&p) | u8(&p) << 8;
        if (flags & 0x0001) index_of(&p, p.bone_index, 0);
        else take(&p, 12);
        if (flags & 0x0300) { index_of(&p, p.bone_index, 0); f32(&p); }
        if (flags & 0x0400) take(&p, 12);
        if (flags & 0x0800) take(&p, 24);
        if (flags & 0x2000) i32(&p);
        if (flags & 0x0020) {
            index_of(&p, p.bone_index, 0);
            i32(&p);
            f32(&p);
            const int32_t links = i32(&p);
            for (int32_t k = 0; k < links && !p.bad; k++) {
                index_of(&p, p.bone_index, 0);
                if (u8(&p)) take(&p, 24);
            }
        }
        m->nbones = i + 1;
    }
    for (int i = 0; i < m->nbones; i++)
        if (m->bones[i].parent >= m->nbones || m->bones[i].parent == i) m->bones[i].parent = -1;
    if (p.bad) { free(mat_corners); free(hidden); snprintf(err, errlen, "it ends before its bones do"); goto done; }

    /* Corners, whole; each triangle the other way round; those of hidden materials left out. */
    m->corners = calloc((size_t)nf, sizeof(model_corner));
    m->material = calloc((size_t)(nf / 3), sizeof(int));
    int mat = 0, left = nm > 0 ? mat_corners[0] : nf, kept = 0;
    for (int32_t t = 0; t < nf / 3; t++) {
        while (left <= 0 && mat + 1 < nm) left = mat_corners[++mat];
        left -= 3;
        if (nm > 0 && hidden[mat]) continue;
        m->material[kept] = mat;
        static const int ORDER[3] = {0, 2, 1};
        for (int c = 0; c < 3; c++) {
            const int v = faces[t * 3 + ORDER[c]];
            model_corner *o = &m->corners[kept * 3 + c];
            for (int k = 0; k < MODEL_WEIGHTS; k++) o->bone[k] = -1;
            if (v < 0 || v >= nv) continue;
            o->pos[0] = pos[v * 3]; o->pos[1] = pos[v * 3 + 1]; o->pos[2] = -pos[v * 3 + 2];
            o->normal[0] = nrm[v * 3]; o->normal[1] = nrm[v * 3 + 1]; o->normal[2] = -nrm[v * 3 + 2];
            /* Rows from the top already: as FBX's are made, from the bottom, for the converter. */
            o->uv[0] = uv[v * 2];
            o->uv[1] = 1.0f - uv[v * 2 + 1];
            for (int k = 0; k < MODEL_WEIGHTS; k++) {
                const int bi = bone[v * 4 + k];
                o->bone[k] = bi >= 0 && bi < m->nbones && wt[v * 4 + k] > 0 ? bi : -1;
                o->weight[k] = o->bone[k] >= 0 ? wt[v * 4 + k] : 0;
            }
        }
        kept++;
    }
    m->ncorners = kept * 3;
    free(mat_corners);
    free(hidden);
    ok = 1;
done:
    free(pos); free(nrm); free(uv); free(wt); free(bone); free(faces);
    free(b);
    if (!ok) model_free(m);
    return ok;
}

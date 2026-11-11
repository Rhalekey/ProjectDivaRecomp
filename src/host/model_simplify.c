/* Fewer triangles for a model, as the game's models have a few thousand and its memory for them
 * is small: edges are collapsed, the one that changes the shape least first (Garland and
 * Heckbert's quadric error), each corner kept where one of its two ends was, so its UV and its
 * bones stay its own. Corners sharing their place with a corner of other UV (a seam of the
 * picture) move only together with those, each to the twin of the same next corner, so nothing
 * tears; corners on an open edge move only along it, to the
 * next corner on it, and such edges weigh more, to keep outlines; a collapse that would turn a
 * triangle over is not made.
 */
#include "model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { double q[10]; } quadric;   /* the plane sums: a2 ab ac ad b2 bc bd c2 cd d2 */

static void q_add_plane(quadric *Q, double a, double b, double c, double d, double w) {
    double *q = Q->q;
    q[0] += w * a * a; q[1] += w * a * b; q[2] += w * a * c; q[3] += w * a * d;
    q[4] += w * b * b; q[5] += w * b * c; q[6] += w * b * d;
    q[7] += w * c * c; q[8] += w * c * d; q[9] += w * d * d;
}

static double q_error(const quadric *A, const quadric *B, const float *p) {
    double q[10];
    for (int i = 0; i < 10; i++) q[i] = A->q[i] + B->q[i];
    const double x = p[0], y = p[1], z = p[2];
    return q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x + q[4] * y * y + 2 * q[5] * y * z + 2 * q[6] * y +
           q[7] * z * z + 2 * q[8] * z + q[9];
}

typedef struct {
    model_corner c;     /* the vertex: place, normal, UV, bones */
    int locked;         /* on a seam: never moved */
    int boundary;       /* on an open edge: moved only along it */
    int *tris, ntris, cap;
    int version;        /* changes as it changes, so the heap's old entries for it are known */
    int gone;
} vertex;

typedef struct {
    int v[3];
    int material;
    int alive;
} triangle;

typedef struct {
    double cost;
    int from, to, from_version, to_version;
} collapse;

/* A min-heap of collapses. */
typedef struct {
    collapse *a;
    int n, cap;
} heap;

static void heap_push(heap *h, collapse c) {
    if (h->n == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 4096;
        h->a = (collapse *)realloc(h->a, (size_t)h->cap * sizeof(collapse));
    }
    int i = h->n++;
    while (i > 0) {
        const int parent = (i - 1) / 2;
        if (h->a[parent].cost <= c.cost) break;
        h->a[i] = h->a[parent];
        i = parent;
    }
    h->a[i] = c;
}

static collapse heap_pop(heap *h) {
    const collapse top = h->a[0], last = h->a[--h->n];
    int i = 0;
    for (;;) {
        int k = 2 * i + 1;
        if (k >= h->n) break;
        if (k + 1 < h->n && h->a[k + 1].cost < h->a[k].cost) k++;
        if (h->a[k].cost >= last.cost) break;
        h->a[i] = h->a[k];
        i = k;
    }
    if (h->n) h->a[i] = last;
    return top;
}

static void add_tri(vertex *v, int t) {
    if (v->ntris == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 8;
        v->tris = (int *)realloc(v->tris, (size_t)v->cap * sizeof(int));
    }
    v->tris[v->ntris++] = t;
}

static void normal_of(const float *a, const float *b, const float *c, double *n) {
    const double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    n[0] = u[1] * w[2] - u[2] * w[1];
    n[1] = u[2] * w[0] - u[0] * w[2];
    n[2] = u[0] * w[1] - u[1] * w[0];
}

/* A hash of a corner's place, to find corners in the same place. */
static unsigned place_hash(const float *p) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 3; i++) {
        uint32_t u;
        memcpy(&u, &p[i], 4);
        h = (h ^ u) * 16777619u;
    }
    return h;
}

/* Corners alike in place and UV are one vertex; a normal of their own (a hard edge) does not part them. */
static int same_vertex(const model_corner *a, const model_corner *b) {
    return !memcmp(a->pos, b->pos, sizeof a->pos) && !memcmp(a->uv, b->uv, sizeof a->uv);
}

/* How many living triangles hold both a and b. */
static int sharing(const vertex *vs, const triangle *ts, int a, int b) {
    int n = 0;
    for (int i = 0; i < vs[a].ntris; i++) {
        const triangle *t = &ts[vs[a].tris[i]];
        if (t->alive && (t->v[0] == b || t->v[1] == b || t->v[2] == b)) n++;
    }
    return n;
}

int model_simplify(model *m, int target) {
    const int ntri = m->ncorners / 3;
    if (ntri <= target) return ntri;

    /* Vertices: corners alike in place and UV made one. */
    const unsigned buckets = 1u << 20;
    int *head = (int *)malloc(buckets * sizeof(int));
    int *next = (int *)malloc((size_t)m->ncorners * sizeof(int));
    int *vid = (int *)malloc((size_t)m->ncorners * sizeof(int));
    vertex *vs = (vertex *)calloc((size_t)m->ncorners, sizeof(vertex));
    triangle *ts = (triangle *)calloc((size_t)ntri, sizeof(triangle));
    if (!head || !next || !vid || !vs || !ts) { free(head); free(next); free(vid); free(vs); free(ts); return ntri; }
    memset(head, -1, buckets * sizeof(int));
    int nv = 0;
    for (int c = 0; c < m->ncorners; c++) {
        const unsigned h = place_hash(m->corners[c].pos) & (buckets - 1);
        int found = -1;
        for (int k = head[h]; k >= 0 && found < 0; k = next[k])
            if (same_vertex(&vs[k].c, &m->corners[c])) found = k;
        if (found < 0) {
            found = nv++;
            vs[found].c = m->corners[c];
            next[found] = head[h];
            head[h] = found;
        }
        vid[c] = found;
    }
    /* Seams: vertices sharing their place with others, each ring of them linked by twin[]. */
    int *twin = (int *)malloc((size_t)(nv ? nv : 1) * sizeof(int));
    for (int v = 0; v < nv; v++) twin[v] = -1;
    for (unsigned h = 0; h < buckets; h++)
        for (int a = head[h]; a >= 0; a = next[a]) {
            if (twin[a] >= 0) continue;
            int last = a;
            twin[a] = a;
            for (int b = next[a]; b >= 0; b = next[b])
                if (twin[b] < 0 && !memcmp(vs[a].c.pos, vs[b].c.pos, sizeof vs[a].c.pos)) {
                    twin[b] = twin[last];
                    twin[last] = b;
                    last = b;
                    vs[a].locked = vs[b].locked = 1;
                }
        }
    free(head);
    free(next);

    quadric *Q = (quadric *)calloc((size_t)nv, sizeof(quadric));
    /* Each vertex's share of the area around it, and the model's height: a collapse costs the
     * picture's moving too, or a flat face full of detail (eyes, a mouth) would be flattened into a
     * smear, its shape kept and its picture not. */
    double *area = (double *)calloc((size_t)(nv ? nv : 1), sizeof(double));
    float low = 1e30f, high = -1e30f;
    for (int v = 0; v < nv; v++) {
        if (vs[v].c.pos[1] < low) low = vs[v].c.pos[1];
        if (vs[v].c.pos[1] > high) high = vs[v].c.pos[1];
    }
    const double uv_weight = (high > low ? (high - low) : 1.0) * (high > low ? (high - low) : 1.0) * 0.25;
    /* What the head moves weighs more: the camera comes close to the face, and its eyes and mouth
     * are small parts on a flat face that would go first. */
    double *importance = (double *)malloc((size_t)(nv ? nv : 1) * sizeof(double));
    uint8_t *in_head = (uint8_t *)calloc((size_t)(m->nbones ? m->nbones : 1), 1);
    const int head_bone = model_head_bone(m);
    for (int b = 0; b < m->nbones && head_bone >= 0; b++)
        for (int a = b, guard = 0; a >= 0 && guard < 256; a = m->bones[a].parent, guard++)
            if (a == head_bone) { in_head[b] = 1; break; }
    for (int v = 0; v < nv; v++) {
        double share = 0;
        for (int k = 0; k < MODEL_WEIGHTS; k++)
            if (vs[v].c.bone[k] >= 0 && vs[v].c.bone[k] < m->nbones && in_head[vs[v].c.bone[k]]) share += vs[v].c.weight[k];
        importance[v] = 1.0 + 5.0 * (share > 1 ? 1 : share);
    }
    free(in_head);
    for (int t = 0; t < ntri; t++) {
        triangle *tr = &ts[t];
        for (int k = 0; k < 3; k++) tr->v[k] = vid[t * 3 + k];
        tr->material = m->material ? m->material[t] : 0;
        tr->alive = tr->v[0] != tr->v[1] && tr->v[1] != tr->v[2] && tr->v[0] != tr->v[2];
        if (!tr->alive) continue;
        for (int k = 0; k < 3; k++) add_tri(&vs[tr->v[k]], t);
        double n[3];
        normal_of(vs[tr->v[0]].c.pos, vs[tr->v[1]].c.pos, vs[tr->v[2]].c.pos, n);
        const double len = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len <= 0) continue;
        const double a = n[0] / len, b = n[1] / len, c = n[2] / len;
        const float *p = vs[tr->v[0]].c.pos;
        const double d = -(a * p[0] + b * p[1] + c * p[2]);
        for (int k = 0; k < 3; k++) q_add_plane(&Q[tr->v[k]], a, b, c, d, len * 0.5);
        for (int k = 0; k < 3; k++) area[tr->v[k]] += len * 0.5 / 3.0;
    }
    free(vid);
    /* Open edges, of only one triangle: their ends marked, and a plane standing on each, across
     * its triangle, weighing much, so that outlines keep. */
    for (int t = 0; t < ntri; t++) {
        const triangle *tr = &ts[t];
        if (!tr->alive) continue;
        for (int k = 0; k < 3; k++) {
            const int a = tr->v[k], b = tr->v[(k + 1) % 3];
            if (sharing(vs, ts, a, b) != 1) continue;
            vs[a].boundary = vs[b].boundary = 1;
            double n[3];
            normal_of(vs[tr->v[0]].c.pos, vs[tr->v[1]].c.pos, vs[tr->v[2]].c.pos, n);
            const float *pa = vs[a].c.pos, *pb = vs[b].c.pos;
            const double e[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
            double c[3] = {e[1] * n[2] - e[2] * n[1], e[2] * n[0] - e[0] * n[2], e[0] * n[1] - e[1] * n[0]};
            const double len = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
            if (len <= 0) continue;
            for (int i = 0; i < 3; i++) c[i] /= len;
            const double d = -(c[0] * pa[0] + c[1] * pa[1] + c[2] * pa[2]);
            const double weight = 10.0 * sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
            q_add_plane(&Q[a], c[0], c[1], c[2], d, weight);
            q_add_plane(&Q[b], c[0], c[1], c[2], d, weight);
        }
    }

    heap h = {0};
    /* A collapse is weighed for any vertex; whether a seam's twins can follow is seen when it comes. */
#define CONSIDER(from, to)                                                                                                   \
    do {                                                                                                                     \
        const int f_ = (from), t_ = (to);                                                                                    \
        if (!vs[f_].gone && !vs[t_].gone && (!vs[f_].locked || vs[t_].locked) &&                                             \
            (!vs[f_].boundary || (vs[t_].boundary && sharing(vs, ts, f_, t_) == 1))) {                                       \
            const double du_ = vs[f_].c.uv[0] - vs[t_].c.uv[0], dv_ = vs[f_].c.uv[1] - vs[t_].c.uv[1];                      \
            const double cost_ = importance[f_] * (q_error(&Q[f_], &Q[t_], vs[t_].c.pos) + uv_weight * area[f_] * (du_ * du_ + dv_ * dv_)); \
            collapse c_ = {cost_, f_, t_, vs[f_].version, vs[t_].version};                                                   \
            heap_push(&h, c_);                                                                                               \
        }                                                                                                                    \
    } while (0)
    for (int t = 0; t < ntri; t++) {
        if (!ts[t].alive) continue;
        for (int k = 0; k < 3; k++) {
            const int a = ts[t].v[k], b = ts[t].v[(k + 1) % 3];
            CONSIDER(a, b);
            CONSIDER(b, a);
        }
    }

    int alive = 0;
    for (int t = 0; t < ntri; t++) alive += ts[t].alive;
    enum { MOST_TWINS = 8 };
    while (alive > target && h.n) {
        const collapse cl = heap_pop(&h);
        if (vs[cl.from].gone || vs[cl.to].gone || vs[cl.from].version != cl.from_version || vs[cl.to].version != cl.to_version) continue;
        /* The collapses made together: u to v, and on a seam each of u's twins to the twin of v
         * beside it, so that both sides of the seam move alike. */
        int from[MOST_TWINS], to[MOST_TWINS], n = 0, ok = 1;
        from[n] = cl.from;
        to[n++] = cl.to;
        if (vs[cl.from].locked) {
            for (int w = twin[cl.from]; w != cl.from && ok; w = twin[w]) {
                if (n == MOST_TWINS) { ok = 0; break; }
                int found = -1;
                for (int i = 0; i < vs[w].ntris && found < 0; i++) {
                    const triangle *tr = &ts[vs[w].tris[i]];
                    if (!tr->alive) continue;
                    for (int k = 0; k < 3; k++)
                        if (tr->v[k] != w && !memcmp(vs[tr->v[k]].c.pos, vs[cl.to].c.pos, sizeof vs[cl.to].c.pos)) found = tr->v[k];
                }
                if (found < 0) ok = 0;
                from[n] = w;
                to[n++] = found;
            }
        }
        /* Each: along an open edge if it is on one, and turning no triangle over. */
        for (int j = 0; j < n && ok; j++) {
            const vertex *u = &vs[from[j]], *v = &vs[to[j]];
            if (v->gone || (u->boundary && sharing(vs, ts, from[j], to[j]) != 1)) { ok = 0; break; }
            for (int i = 0; i < u->ntris && ok; i++) {
                const triangle *tr = &ts[u->tris[i]];
                if (!tr->alive || tr->v[0] == to[j] || tr->v[1] == to[j] || tr->v[2] == to[j]) continue;
                const float *p[3];
                for (int k = 0; k < 3; k++) p[k] = vs[tr->v[k]].c.pos;
                double before[3], after[3];
                normal_of(p[0], p[1], p[2], before);
                for (int k = 0; k < 3; k++)
                    if (tr->v[k] == from[j]) p[k] = v->c.pos;
                normal_of(p[0], p[1], p[2], after);
                if (before[0] * after[0] + before[1] * after[1] + before[2] * after[2] <= 0) ok = 0;
            }
        }
        if (!ok) continue;
        for (int j = 0; j < n; j++) {
            vertex *u = &vs[from[j]], *v = &vs[to[j]];
            /* u's triangles become v's; those holding both are gone. */
            for (int i = 0; i < u->ntris; i++) {
                const int t = u->tris[i];
                triangle *tr = &ts[t];
                if (!tr->alive) continue;
                if (tr->v[0] == to[j] || tr->v[1] == to[j] || tr->v[2] == to[j]) {
                    tr->alive = 0;
                    alive--;
                    continue;
                }
                for (int k = 0; k < 3; k++)
                    if (tr->v[k] == from[j]) tr->v[k] = to[j];
                add_tri(v, t);
            }
            u->gone = 1;
            v->version++;
            for (int i = 0; i < 10; i++) Q[to[j]].q[i] += Q[from[j]].q[i];
            area[to[j]] += area[from[j]];
            /* Out of its ring of twins. */
            int p = from[j];
            while (twin[p] != from[j]) p = twin[p];
            twin[p] = twin[from[j]];
            twin[from[j]] = from[j];
            if (twin[p] == p) vs[p].locked = 0;
        }
        /* The edges of those kept, with their new costs. */
        for (int j = 0; j < n; j++) {
            const vertex *v = &vs[to[j]];
            for (int i = 0; i < v->ntris; i++) {
                const triangle *tr = &ts[v->tris[i]];
                if (!tr->alive) continue;
                for (int k = 0; k < 3; k++) {
                    const int o = tr->v[k];
                    if (o == to[j]) continue;
                    CONSIDER(to[j], o);
                    CONSIDER(o, to[j]);
                }
            }
        }
    }
#undef CONSIDER

    /* The triangles left, their corners whole again. */
    model_corner *corners = (model_corner *)malloc((size_t)alive * 3 * sizeof(model_corner));
    int *material = (int *)malloc((size_t)(alive ? alive : 1) * sizeof(int));
    int k = 0;
    for (int t = 0; t < ntri && corners && material; t++) {
        if (!ts[t].alive) continue;
        for (int c = 0; c < 3; c++) corners[k * 3 + c] = vs[ts[t].v[c]].c;
        material[k++] = ts[t].material;
    }
    for (int i = 0; i < nv; i++) free(vs[i].tris);
    free(vs);
    free(ts);
    free(Q);
    free(importance);
    free(area);
    free(twin);
    free(h.a);
    if (!corners || !material) { free(corners); free(material); return ntri; }
    free(m->corners);
    free(m->material);
    m->corners = corners;
    m->material = material;
    m->ncorners = k * 3;
    return k;
}

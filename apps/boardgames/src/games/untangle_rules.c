/*
 * Untangle: the rules (untangle.h): the crossings, the maker (dots, a triangulation thinned, the circle), the hint and
 * the player's moves with undo. No SDL: tested by tests/untangle_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/untangle.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define EPS 1e-7

static const int DOTS[UT_VARIANTS] = {10, 20, 30, 45}, CORDS[UT_VARIANTS] = {15, 34, 52, 80};

static double orient(double ax, double ay, double bx, double by, double cx, double cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static int sign(double v) { return v > EPS ? 1 : v < -EPS ? -1 : 0; }

static int between(double a, double b, double c)    /* c within a..b, ends included */
{
    return c >= fmin(a, b) - EPS && c <= fmax(a, b) + EPS;
}

int ut_segments_cross(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy)
{
    int d1 = sign(orient(cx, cy, dx, dy, ax, ay)), d2 = sign(orient(cx, cy, dx, dy, bx, by));
    int d3 = sign(orient(ax, ay, bx, by, cx, cy)), d4 = sign(orient(ax, ay, bx, by, dx, dy));
    if (d1 * d2 < 0 && d3 * d4 < 0) return 1;
    if (!d1 && between(cx, dx, ax) && between(cy, dy, ay)) return 1;    /* an end lying on the other segment */
    if (!d2 && between(cx, dx, bx) && between(cy, dy, by)) return 1;
    if (!d3 && between(ax, bx, cx) && between(ay, by, cy)) return 1;
    if (!d4 && between(ax, bx, dx) && between(ay, by, dy)) return 1;
    return 0;
}

int ut_cords_cross(const ut_game *g, int i, int j)
{
    int a = g->e[i][0], b = g->e[i][1], c = g->e[j][0], d = g->e[j][1];
    if (i == j) return 0;
    if (a == c || a == d || b == c || b == d) {  /* one dot shared: crossing only if one runs along the other */
        int s = a == c || a == d ? a : b, p = s == a ? b : a, q = s == c ? d : c;
        double ox = g->x[s], oy = g->y[s];
        if (sign(orient(ox, oy, g->x[p], g->y[p], g->x[q], g->y[q]))) return 0;
        return (g->x[p] - ox) * (g->x[q] - ox) + (g->y[p] - oy) * (g->y[q] - oy) > 0;
    }
    return ut_segments_cross(g->x[a], g->y[a], g->x[b], g->y[b], g->x[c], g->y[c], g->x[d], g->y[d]);
}

int ut_crossings(const ut_game *g)
{
    int k = 0;
    for (int i = 0; i < g->ne; i++)
        for (int j = i + 1; j < g->ne; j++) k += ut_cords_cross(g, i, j);
    return k;
}

int ut_crossed(const ut_game *g, int i)
{
    for (int j = 0; j < g->ne; j++)
        if (ut_cords_cross(g, i, j)) return 1;
    return 0;
}

int ut_solved(const ut_game *g) { return g->nv > 0 && ut_crossings(g) == 0; }

/* the crossings of the cords of dot v */
static int crossings_of(const ut_game *g, int v)
{
    int k = 0;
    for (int i = 0; i < g->ne; i++) {
        if (g->e[i][0] != v && g->e[i][1] != v) continue;
        for (int j = 0; j < g->ne; j++) {
            int also = g->e[j][0] == v || g->e[j][1] == v;
            if (j == i || (also && j < i)) continue;
            k += ut_cords_cross(g, i, j);
        }
    }
    return k;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
static int joined(const ut_game *g)             /* every dot reached from dot 0 */
{
    int seen[UT_MAXV] = {0}, stack[UT_MAXV], ns = 0, count = 1;
    seen[0] = 1;
    stack[ns++] = 0;
    while (ns) {
        int v = stack[--ns];
        for (int i = 0; i < g->ne; i++) {
            int w = g->e[i][0] == v ? g->e[i][1] : g->e[i][1] == v ? g->e[i][0] : -1;
            if (w >= 0 && !seen[w]) seen[w] = 1, count++, stack[ns++] = w;
        }
    }
    return count == g->nv;
}

static float dist_to_segment(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = 0;
    if (l2 > 0) t = ((px - ax) * dx + (py - ay) * dy) / l2;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return hypotf(px - ax - t * dx, py - ay - t * dy);
}

static float frand(uint32_t *r) { return (pr_next(r) >> 8) / 16777216.0f; }

static int try_make(ut_game *g, int variant, uint32_t *r)
{
    int nv = DOTS[variant];
    float dmin = 0.66f / sqrtf((float)nv);
    memset(g, 0, sizeof *g);
    g->nv = nv;
    for (int placed = 0, tries = 0; placed < nv; tries++) {   /* dots, evenly: never closer than dmin */
        if (tries > 30000) { placed = 0; tries = 0; dmin *= 0.95f; }
        float x = 0.07f + 0.86f * frand(r), y = 0.07f + 0.86f * frand(r);
        int ok = 1;
        for (int i = 0; i < placed && ok; i++) ok = hypotf(x - g->hx[i], y - g->hy[i]) >= dmin;
        if (ok) g->hx[placed] = x, g->hy[placed] = y, placed++;
    }
    memcpy(g->x, g->hx, sizeof g->x);
    memcpy(g->y, g->hy, sizeof g->y);
    /* the triangulation: the shortest cords first, each where it crosses none and passes no dot closely */
    typedef struct { float l; uint8_t a, b; } pair;
    static pair pairs[UT_MAXV * UT_MAXV / 2];
    int np = 0;
    for (int a = 0; a < nv; a++)
        for (int b = a + 1; b < nv; b++)
            pairs[np].l = hypotf(g->hx[a] - g->hx[b], g->hy[a] - g->hy[b]), pairs[np].a = (uint8_t)a,
            pairs[np++].b = (uint8_t)b;
    for (int i = 1; i < np; i++)                /* by length (insertion: a thousand pairs at most) */
        for (int j = i; j > 0 && pairs[j].l < pairs[j - 1].l; j--) {
            pair t = pairs[j];
            pairs[j] = pairs[j - 1];
            pairs[j - 1] = t;
        }
    for (int i = 0; i < np && g->ne < 3 * nv - 6 && g->ne < UT_MAXE; i++) {
        int a = pairs[i].a, b = pairs[i].b, ok = 1;
        for (int v = 0; v < nv && ok; v++)
            if (v != a && v != b && dist_to_segment(g->hx[v], g->hy[v], g->hx[a], g->hy[a], g->hx[b], g->hy[b]) <
                                        0.3f * dmin)
                ok = 0;
        g->e[g->ne][0] = (uint8_t)a;
        g->e[g->ne][1] = (uint8_t)b;
        for (int j = 0; j < g->ne && ok; j++) ok = !ut_cords_cross(g, g->ne, j);
        if (ok) g->ne++;
    }
    /* thinned: cords taken away by chance, every dot keeping two, all joined */
    int order[UT_MAXE], deg[UT_MAXV] = {0};
    for (int i = 0; i < g->ne; i++) order[i] = i, deg[g->e[i][0]]++, deg[g->e[i][1]]++;
    pr_shuffle(r, order, g->ne);
    uint8_t gone[UT_MAXE] = {0};
    int left = g->ne;
    for (int k = 0; k < g->ne && left > CORDS[variant]; k++) {
        int i = order[k], a = g->e[i][0], b = g->e[i][1];
        if (deg[a] <= 2 || deg[b] <= 2) continue;
        ut_game t = *g;                         /* still joined without it? */
        t.ne = 0;
        for (int j = 0; j < g->ne; j++)
            if (!gone[j] && j != i) t.e[t.ne][0] = g->e[j][0], t.e[t.ne][1] = g->e[j][1], t.ne++;
        if (!joined(&t)) continue;
        gone[i] = 1;
        deg[a]--, deg[b]--;
        left--;
    }
    int ne = 0;
    for (int i = 0; i < g->ne; i++)
        if (!gone[i]) g->e[ne][0] = g->e[i][0], g->e[ne][1] = g->e[i][1], ne++;
    g->ne = ne;
    if (ne != CORDS[variant] || ut_crossings(g)) return 0;
    /* the dots round a circle, shuffled: tangled */
    int perm[UT_MAXV];
    for (int i = 0; i < nv; i++) perm[i] = i;
    float turn = frand(r) * 6.2831853f;
    for (int tries = 0; tries < 50; tries++) {
        pr_shuffle(r, perm, nv);
        for (int i = 0; i < nv; i++) {
            float a = turn + 6.2831853f * i / nv;
            g->x[perm[i]] = 0.5f + 0.42f * cosf(a);
            g->y[perm[i]] = 0.5f + 0.42f * sinf(a);
        }
        if (ut_crossings(g) >= nv) return 1;
    }
    return 0;
}

void ut_new(ut_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= UT_VARIANTS) variant = 0;
    uint32_t r = pr_seed(seed);
    for (int tries = 0; tries < 200; tries++)
        if (try_make(g, variant, &r)) break;
    g->nh = g->moves = 0;
    memset(g->home, 0, sizeof g->home);
}

/* ---- the player's moves ------------------------------------------------------------------------------------------ */
static float keep_on(float v) { return v < UT_MIN ? UT_MIN : v > 1 - UT_MIN ? 1 - UT_MIN : v; }

static void put(ut_game *g, int v, float x, float y)
{
    if (g->nh >= UT_HIST) {
        memmove(g->hist, g->hist + 1, (UT_HIST - 1) * sizeof g->hist[0]);
        g->nh--;
    }
    g->hist[g->nh].v = (uint8_t)v, g->hist[g->nh].x = g->x[v], g->hist[g->nh++].y = g->y[v];
    g->x[v] = x;
    g->y[v] = y;
    g->moves++;
}

int ut_move(ut_game *g, int v, float x, float y)
{
    if (v < 0 || v >= g->nv) return 0;
    x = keep_on(x);
    y = keep_on(y);
    if (fabsf(x - g->x[v]) < 1e-5f && fabsf(y - g->y[v]) < 1e-5f) return 0;
    put(g, v, x, y);
    g->home[v] = 0;
    return 1;
}

static int crowded(const ut_game *g, int v, float x, float y)   /* too near another dot */
{
    for (int w = 0; w < g->nv; w++)
        if (w != v && hypotf(g->x[w] - x, g->y[w] - y) < 0.045f) return 1;
    return 0;
}

int ut_hint(ut_game *g)
{
    if (ut_solved(g)) return -1;
    enum { GRID = 13, TOP = 10 };
    int best = 0, bv = -1, base_of[UT_MAXV], top[UT_MAXV], nt = g->nv < TOP ? g->nv : TOP;
    float bx = 0, by = 0;
    for (int v = 0; v < g->nv; v++) {           /* the dots in the most crossings first */
        base_of[v] = g->home[v] ? 0 : crossings_of(g, v);
        int j = v;
        while (j > 0 && base_of[top[j - 1]] < base_of[v]) { top[j] = top[j - 1]; j--; }
        top[j] = v;
    }
    for (int t = 0; t < nt; t++) {             /* the move that lowers the crossings most */
        int v = top[t], base = base_of[v];
        if (base <= best) continue;
        float ox = g->x[v], oy = g->y[v];
        for (int k = 0; k <= GRID * GRID; k++) {
            float x = k < GRID * GRID ? 0.06f + 0.88f * (k % GRID) / (GRID - 1) : g->hx[v];
            float y = k < GRID * GRID ? 0.06f + 0.88f * (k / GRID) / (GRID - 1) : g->hy[v];
            if (crowded(g, v, x, y)) continue;
            g->x[v] = x, g->y[v] = y;
            int gain = base - crossings_of(g, v);
            if (gain > best) best = gain, bv = v, bx = x, by = y;
        }
        g->x[v] = ox, g->y[v] = oy;
    }
    if (bv >= 0) {
        put(g, bv, bx, by);
        return bv;
    }
    int bestd = -1000000;                       /* none: a dot put where it is in the maker's layout */
    for (int v = 0; v < g->nv; v++) {
        if (g->home[v]) continue;
        float ox = g->x[v], oy = g->y[v];
        int base = crossings_of(g, v);
        g->x[v] = g->hx[v], g->y[v] = g->hy[v];
        int gain = base - crossings_of(g, v);
        g->x[v] = ox, g->y[v] = oy;
        if (gain > bestd) bestd = gain, bv = v;
    }
    if (bv < 0) return -1;
    put(g, bv, g->hx[bv], g->hy[bv]);
    g->home[bv] = 1;
    return bv;
}

int ut_undo(ut_game *g)
{
    if (!g->nh) return -1;
    ut_change h = g->hist[--g->nh];
    g->x[h.v] = h.x;
    g->y[h.v] = h.y;
    g->home[h.v] = 0;
    if (g->moves > 0) g->moves--;
    return h.v;
}

int ut_script(ut_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *e = s;
        while (*e && *e != ' ') e++;
        int ok = 0;
        if (*s == 'h' && e == s + 1) {
            ok = ut_hint(g) >= 0;
        } else if (*s == '=') {
            int k = e > s + 1 ? atoi(s + 1) : 9999;
            ok = 1;
            for (int i = 0; i < k && !ut_solved(g); i++)
                if (ut_hint(g) < 0) { ok = 0; break; }
        } else {
            char *p;
            long v = strtol(s, &p, 10);
            if (p == s || *p != ':') return 0;
            float x = strtof(p + 1, &p);
            if (*p != ',') return 0;
            float y = strtof(p + 1, &p);
            if (p != e) return 0;
            ok = ut_move(g, (int)v, x, y);
        }
        if (!ok) return 0;
        s = e;
    }
    return 1;
}

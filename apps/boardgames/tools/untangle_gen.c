/*
 * Untangle's levels, designed and chosen: src/games/untangle_levels.h. Our own levels, drawn from a library of
 * designed pictures (wheels, stars, flowers, webs, nested polygons, the solids' flat drawings, ladders, grids, brick
 * walls, triangle and hexagon lattices), each stretched over the whole board, then a few dots moved out of it, each to
 * the free place with the most room around it where its cords cross cords that stayed. Each candidate:
 *   - the picture on the level's grid (64 steps across): no crossing, the dots well apart, no cord passing close to a
 *     dot it does not end at;
 *   - the start: the moved dots spread (the most room around each, within a cap, chance breaking ties), none close to
 *     another dot or a cord;
 *   - its par: the dots moved (putting them back untangles it), kept when at most one dot fewer might do (ut_cover: no
 *     set of dots smaller than that has one of every crossing's four ends), so the par is tight;
 *   - its difficulty: the dots, the dots moved, the crossings.
 * Each chapter has its dots (6 up to 36) and its moved dots (2 up to 10). Its 25 levels are picked along the
 * difficulty curve with a varied mix: a picture at most twice in a chapter (with other dots moved), a family (wheels,
 * grids...) a few times; the chapter ends on a showpiece (a web, a flower, a solid, a star...).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/utgen apps/boardgames/tools/untangle_gen.c \
 *       apps/boardgames/src/games/untangle_rules.c -lm && /tmp/utgen > apps/boardgames/src/games/untangle_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/untangle.h"

#define PI 3.14159265358979
#define MAXSHAPES 600
#define MAXCANDS 6000
#define TRIALS 5

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
enum { F_WHEEL, F_PRISM, F_ANTI, F_STAR, F_FLOWER, F_WEB, F_SOLID, F_RINGS, F_GRID, F_TRI, F_HEX, F_FAMILIES };

typedef struct {
    int nv, ne, family, show;
    double x[UT_MAXV], y[UT_MAXV];
    uint8_t e[UT_MAXE][2];
    char name[40];
} shape;

static shape shapes[MAXSHAPES];
static int nshapes;

static int pt(shape *s, double x, double y)
{
    s->x[s->nv] = x, s->y[s->nv] = y;
    return s->nv++;
}

static void edge(shape *s, int a, int b)
{
    if (a == b) return;
    for (int i = 0; i < s->ne; i++)
        if ((s->e[i][0] == a && s->e[i][1] == b) || (s->e[i][0] == b && s->e[i][1] == a)) return;
    s->e[s->ne][0] = (uint8_t)a, s->e[s->ne][1] = (uint8_t)b, s->ne++;
}

/* n dots round a circle of radius r from angle ph (0: the right, turning down the board), joined round if cyc */
static int ring(shape *s, int n, double r, double ph, int cyc)
{
    int a = s->nv;
    for (int i = 0; i < n; i++) pt(s, r * cos(ph + 2 * PI * i / n), r * sin(ph + 2 * PI * i / n));
    if (cyc && n > 2)
        for (int i = 0; i < n; i++) edge(s, a + i, a + (i + 1) % n);
    return a;
}

static double ang(const shape *s, int v)
{
    double t = atan2(s->y[v], s->x[v]);
    return t < 0 ? t + 2 * PI : t;
}

/* each dot of the smaller ring joined to the nearest by angle of the larger one */
static void radial(shape *s, int a, int na, int b, int nb)
{
    if (na > nb) {
        int t = a; a = b; b = t;
        t = na; na = nb; nb = t;
    }
    for (int i = 0; i < na; i++) {
        int best = 0;
        double bd = 9;
        for (int j = 0; j < nb; j++) {
            double d = fabs(ang(s, a + i) - ang(s, b + j));
            d = fmin(d, 2 * PI - d);
            if (d < bd - 1e-9) bd = d, best = j;
        }
        edge(s, a + i, b + best);
    }
}

/* the ring between two circles cut into triangles: each dot joined to the last dot of the other circle before it, by
 * angle (a fan where several follow each other) */
static void band(shape *s, int a, int na, int b, int nb)
{
    int ord[2 * UT_MAXV], n = 0;
    double key[UT_MAXV];                        /* the angle; a tie goes to the first circle */
    for (int i = 0; i < na; i++) ord[n++] = a + i, key[a + i] = ang(s, a + i);
    for (int i = 0; i < nb; i++) ord[n++] = b + i, key[b + i] = ang(s, b + i) + 1e-6;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && key[ord[j]] < key[ord[j - 1]]; j--) {
            int t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        int v = ord[i], inA = v >= a && v < a + na;
        for (int k = 1; k < n; k++) {
            int w = ord[(i - k + n) % n];
            if ((w >= a && w < a + na) != inA) { edge(s, v, w); break; }
        }
    }
}

static void hub(shape *s, int a, int n, int step)
{
    int c = pt(s, 0, 0);
    for (int i = 0; i < n; i += step) edge(s, c, a + i);
}

static shape *add(int family, int show, const char *name)
{
    shape *s = &shapes[nshapes];
    memset(s, 0, sizeof *s);
    s->family = family, s->show = show;
    snprintf(s->name, sizeof s->name, "%s", name);
    return s;
}

static void keep(shape *s) { if (s->nv >= 6 && s->nv <= 36 && s->ne <= UT_MAXE && nshapes < MAXSHAPES - 1) nshapes++; }

static void grid(int c, int r, int mode)
{
    static const char *MODE[] = {"plain", "slashes", "jack", "brick", "checks"};
    char nm[40];
    snprintf(nm, sizeof nm, "grid %dx%d %s", c, r, MODE[mode]);
    shape *s = add(F_GRID, mode == 2, nm);
    for (int j = 0; j < r; j++)
        for (int i = 0; i < c; i++) pt(s, i / (double)(c - 1), j / (double)(r - 1));
    for (int j = 0; j < r; j++)
        for (int i = 0; i < c; i++) {
            int v = j * c + i;
            if (i + 1 < c) edge(s, v, v + 1);
            if (j + 1 < r && (mode != 3 || (i + j) % 2 == 0 || i == 0 || i == c - 1)) edge(s, v, v + c);
            if (i + 1 < c && j + 1 < r) {
                if (mode == 1 || (mode == 2 && (i + j) % 2) || (mode == 4 && (i + j) % 2 == 0)) edge(s, v + 1, v + c);
                if (mode == 2 && (i + j) % 2 == 0) edge(s, v, v + c + 1);
            }
        }
    keep(s);
}

static void library(void)
{
    const double up = -PI / 2;
    char nm[40];
    for (int n = 5; n <= 24; n++) {             /* wheels, and wheels with every other spoke */
        snprintf(nm, sizeof nm, "wheel %d", n);
        shape *s = add(F_WHEEL, 0, nm);
        hub(s, ring(s, n, 1, up, 1), n, 1);
        keep(s);
        if (n % 2 == 0 && n >= 8) {
            snprintf(nm, sizeof nm, "half wheel %d", n);
            s = add(F_WHEEL, 0, nm);
            hub(s, ring(s, n, 1, up, 1), n, 2);
            keep(s);
        }
    }
    for (int n = 3; n <= 18; n++)               /* two polygons, one in the other: joined straight or in triangles */
        for (int h = 0; h < 2; h++) {
            snprintf(nm, sizeof nm, h ? "prism %d hub" : "prism %d", n);
            shape *s = add(F_PRISM, 0, nm);
            int a = ring(s, n, 1, up, 1), b = ring(s, n, n <= 4 ? 0.42 : 0.5, up, 1);
            radial(s, a, n, b, n);
            if (h) hub(s, b, n, 1);
            keep(s);
            snprintf(nm, sizeof nm, h ? "antiprism %d hub" : "antiprism %d", n);
            s = add(F_ANTI, n == 3 && !h, nm);
            a = ring(s, n, 1, up, 1), b = ring(s, n, n == 3 ? 0.3 : n == 4 ? 0.42 : 0.55, up + PI / n, 1);
            band(s, a, n, b, n);
            if (h) hub(s, b, n, 1);
            keep(s);
        }
    for (int n = 3; n <= 17; n++)               /* stars: a polygon and its points, the middle joined or not */
        for (int h = 0; h < 3; h++) {
            snprintf(nm, sizeof nm, h == 0 ? "star %d" : h == 1 ? "star %d hub" : "star %d open", n);
            shape *s = add(F_STAR, n >= 5, nm);
            int a = ring(s, n, n <= 4 ? 0.4 : 0.45, up + PI / n, h != 2), b = ring(s, n, 1, up, 0);
            band(s, a, n, b, n);
            if (h) hub(s, a, n, 1);
            keep(s);
        }
    for (int n = 4; n <= 11; n++) {             /* flowers: a heart, petals round it */
        snprintf(nm, sizeof nm, "flower %d", n);
        shape *s = add(F_FLOWER, 1, nm);
        int a = ring(s, n, 0.32, up, 1), m = ring(s, n, 0.68, up + PI / n, 0), t = ring(s, n, 1, up, 0);
        band(s, a, n, m, n);
        band(s, m, n, t, n);
        radial(s, a, n, t, n);
        hub(s, a, n, 1);
        keep(s);
        snprintf(nm, sizeof nm, "daisy %d", n);     /* the petals' tips joined round */
        s = add(F_FLOWER, 1, nm);
        a = ring(s, n, 0.4, up, 1);
        t = ring(s, n, 1, up + PI / n, 1);
        m = ring(s, n, 0.72, up, 0);
        radial(s, a, n, m, n);
        band(s, m, n, t, n);
        hub(s, a, n, 1);
        keep(s);
    }
    for (int rings = 2; rings <= 4; rings++)    /* spider webs: rings and spokes */
        for (int n = 4; n <= 12; n++)
            for (int h = 0; h < 2; h++) {
                if (rings == 2 && !h) continue;     /* that one is a prism */
                snprintf(nm, sizeof nm, h ? "web %dx%d hub" : "web %dx%d", rings, n);
                shape *s = add(F_WEB, 1, nm);
                int prev = -1;
                for (int k = 0; k < rings; k++) {
                    double r = h ? (k + 1.0) / rings : 0.3 + 0.7 * k / (rings - 1);
                    int a = ring(s, n, r, up, 1);
                    if (prev >= 0) radial(s, prev, n, a, n);
                    if (k == 0 && h) hub(s, a, n, 1);
                    prev = a;
                }
                keep(s);
            }
    for (int rings = 3; rings <= 4; rings++)    /* nested polygons, each turned half a side, in triangles */
        for (int n = 4; n <= 10; n++)
            for (int h = 0; h < 2; h++) {
                snprintf(nm, sizeof nm, h ? "nest %dx%d hub" : "nest %dx%d", rings, n);
                shape *s = add(F_RINGS, 1, nm);
                int prev = -1;
                double r = 1;
                for (int k = 0; k < rings; k++, r *= 0.75 * cos(PI / n)) {
                    int a = ring(s, n, r, up + k * PI / n, 1);
                    if (prev >= 0) band(s, prev, n, a, n);
                    prev = a;
                }
                if (h) hub(s, prev, n, 1);
                keep(s);
            }
    {   /* the solids drawn flat, and rings of different sizes (conn: 1 in triangles, 0 straight) */
        static const struct { int n[4], conn[3], nr, hub; const char *name; } SOL[] = {
            {{3, 6, 3}, {1, 1}, 3, 0, "icosahedron"},
            {{5, 10, 5}, {0, 0}, 3, 0, "dodecahedron"},
            {{4, 8, 4}, {1, 1}, 3, 0, "cuboctahedron"},
            {{5, 10}, {1}, 2, 1, "sun 5"},
            {{6, 12}, {1}, 2, 1, "sun 6"},
            {{8, 16}, {1}, 2, 1, "sun 8"},
            {{4, 8}, {0}, 2, 1, "shield 4"},
            {{6, 12}, {0}, 2, 1, "shield 6"},
            {{7, 14}, {0}, 2, 1, "shield 7"},
            {{8, 16}, {0}, 2, 1, "shield 8"},
            {{3, 6, 12}, {1, 0}, 3, 0, "fern"},
            {{4, 8, 16}, {0, 0}, 3, 1, "tree"},
            {{6, 12, 6}, {0, 1}, 3, 0, "lantern"},
            {{5, 10, 15}, {0, 0}, 3, 0, "rose"},
            {{6, 12, 12}, {1, 0}, 3, 1, "dahlia"},
            {{5, 5, 10, 10}, {0, 1, 0}, 4, 0, "wreath"},
            {{4, 8, 8, 4}, {0, 1, 1}, 4, 0, "brooch"},
            {{6, 6, 12, 12}, {1, 0, 1}, 4, 0, "mandala"},
        };
        for (int i = 0; i < (int)(sizeof SOL / sizeof SOL[0]); i++) {
            shape *s = add(F_SOLID, 1, SOL[i].name);
            int at[4];
            double r = 1;
            for (int k = 0; k < SOL[i].nr; k++) {
                if (k) r *= 0.75 * cos(PI / SOL[i].n[k - 1]);
                /* a ring the same size as the one outside it turns half a side when joined in triangles */
                double ph = up + (k && SOL[i].n[k] == SOL[i].n[k - 1] && SOL[i].conn[k - 1] ? PI / SOL[i].n[k] : 0);
                if (!strcmp(SOL[i].name, "dodecahedron") && k == 2) ph = up + PI / 5;
                at[k] = ring(s, SOL[i].n[k], r, ph, 1);
                if (k && SOL[i].conn[k - 1]) band(s, at[k - 1], SOL[i].n[k - 1], at[k], SOL[i].n[k]);
                else if (k) radial(s, at[k - 1], SOL[i].n[k - 1], at[k], SOL[i].n[k]);
            }
            if (SOL[i].hub) hub(s, at[SOL[i].nr - 1], SOL[i].n[SOL[i].nr - 1], 1);
            keep(s);
        }
    }
    for (int c = 2; c <= 6; c++)                /* grids, upright (portrait boards are tall), ladders among them */
        for (int r = c; r <= 9; r++)
            for (int m = 0; m < 5; m++) {
                if (c * r < 6 || c * r > 36 || (c == 2 && (m == 3 || m == 4))) continue;
                grid(c, r, m);
            }
    for (int m = 3; m <= 8; m++)                /* triangles of triangles */
        for (int hollow = 0; hollow < 2; hollow++) {
            snprintf(nm, sizeof nm, hollow ? "triangles %d open" : "triangles %d", m);
            shape *s = add(F_TRI, 1, nm);
            int id[9][9];
            for (int j = 0; j < m; j++)
                for (int i = 0; i <= j; i++) id[j][i] = pt(s, i - j / 2.0, j * 0.866);
            for (int j = 0; j < m; j++)
                for (int i = 0; i <= j; i++) {
                    if (i < j) edge(s, id[j][i], id[j][i + 1]);
                    if (j + 1 < m) {
                        edge(s, id[j][i], id[j + 1][i + 1]);
                        if (!hollow || (i + j) % 2 == 0 || i == 0) edge(s, id[j][i], id[j + 1][i]);
                    }
                }
            keep(s);
        }
    for (int side = 2; side <= 4; side++)       /* hexagons of triangles (the middle left out, or not) */
        for (int hole = 0; hole < 2; hole++) {
            snprintf(nm, sizeof nm, hole ? "hexagon %d ring" : "hexagon %d", side);
            shape *s = add(F_HEX, 1, nm);
            int id[9][9], R = side - 1;
            for (int q = -R; q <= R; q++)
                for (int r = -R; r <= R; r++) {
                    id[q + R][r + R] = -1;
                    if (abs(q + r) > R || (hole && !q && !r) || s->nv >= UT_MAXV) continue;
                    id[q + R][r + R] = pt(s, q + r / 2.0, r * 0.866);
                }
            static const int D[3][2] = {{1, 0}, {0, 1}, {-1, 1}};
            for (int q = -R; q <= R; q++)
                for (int r = -R; r <= R; r++)
                    for (int d = 0; d < 3; d++) {
                        int q2 = q + D[d][0], r2 = r + D[d][1];
                        if (q2 < -R || q2 > R || r2 < -R || r2 > R) continue;
                        int a = id[q + R][r + R], b = id[q2 + R][r2 + R];
                        if (a >= 0 && b >= 0 && s->ne < UT_MAXE) edge(s, a, b);
                    }
            keep(s);
        }
}

/* ---- a level from a picture -------------------------------------------------------------------------------------- */
typedef struct {
    ut_game g;
    int shape, k, crossings, cover;
    double score;
} cand;

static float dseg(float px, float py, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = 0;
    if (l2 > 0) t = ((px - ax) * dx + (py - ay) * dy) / l2;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return hypotf(px - ax - t * dx, py - ay - t * dy);
}

/* the room round dot v at its place: to the other dots, to the cords it does not end at, its cords to other dots */
static float room(const ut_game *g, int v)
{
    float best = 9;
    for (int w = 0; w < g->nv; w++)
        if (w != v) best = fminf(best, hypotf(g->x[v] - g->x[w], g->y[v] - g->y[w]));
    for (int i = 0; i < g->ne; i++) {
        int a = g->e[i][0], b = g->e[i][1];
        if (a != v && b != v) {
            best = fminf(best, 1.6f * dseg(g->x[v], g->y[v], g->x[a], g->y[a], g->x[b], g->y[b]));
            continue;
        }
        for (int w = 0; w < g->nv; w++)
            if (w != a && w != b) best = fminf(best, 1.6f * dseg(g->x[w], g->y[w], g->x[a], g->y[a], g->x[b], g->y[b]));
    }
    return best;
}

static float least_room(const ut_game *g)
{
    float m = 9;
    for (int v = 0; v < g->nv; v++) m = fminf(m, room(g, v));
    return m;
}

/* the picture put on the level's grid, stretched over the board (steps 1 to 62 each way): 0 when it is cramped */
static int picture(const shape *s, ut_game *g)
{
    double x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9;
    for (int v = 0; v < s->nv; v++)
        x0 = fmin(x0, s->x[v]), x1 = fmax(x1, s->x[v]), y0 = fmin(y0, s->y[v]), y1 = fmax(y1, s->y[v]);
    memset(g, 0, sizeof *g);
    g->nv = s->nv, g->ne = s->ne;
    memcpy(g->e, s->e, sizeof g->e);
    for (int v = 0; v < s->nv; v++) {
        g->hx[v] = g->x[v] = ut_grid((int)lround(1 + 61 * (s->x[v] - x0) / (x1 - x0)));
        g->hy[v] = g->y[v] = ut_grid((int)lround(1 + 61 * (s->y[v] - y0) / (y1 - y0)));
    }
    return ut_crossings(g) == 0 && least_room(g) >= fminf(0.075f, 0.36f / sqrtf((float)g->nv));
}

/* dot v's cords crossing a cord between two dots that stay */
static int crosses(ut_game *g, int v, const uint8_t *moved)
{
    for (int i = 0; i < g->ne; i++) {
        if (g->e[i][0] != v && g->e[i][1] != v) continue;
        for (int j = 0; j < g->ne; j++)
            if (!moved[g->e[j][0]] && !moved[g->e[j][1]] && ut_cords_cross(g, i, j)) return 1;
    }
    return 0;
}

static int make(cand *c, int si, int k, uint32_t seed)
{
    ut_game *g = &c->g;
    if (!picture(&shapes[si], g)) return 0;
    int nv = g->nv;
    uint32_t r = pr_seed(seed);
    int dots[UT_MAXV];
    uint8_t moved[UT_MAXV] = {0};
    for (int v = 0; v < nv; v++) dots[v] = v;
    pr_shuffle(&r, dots, nv);
    for (int pass = 0, got = 0; pass < 2; pass++)   /* dots not side by side first: a tighter par */
        for (int i = 0; i < nv && got < k; i++) {
            int v = dots[i], next = 0;
            for (int j = 0; j < g->ne && !pass; j++)
                next |= (g->e[j][0] == v && moved[g->e[j][1]]) || (g->e[j][1] == v && moved[g->e[j][0]]);
            if (!moved[v] && !next) moved[v] = 1, got++;
        }
    for (int i = 0, j = 0; i < nv; i++)
        if (moved[i]) dots[j++] = i;
    pr_shuffle(&r, dots, k);
    float cap = 0.55f / sqrtf((float)nv);
    for (int i = 0; i < k; i++) {               /* each moved dot to the roomiest free place where its cords cross */
        int v = dots[i];
        float best = -1, bx = 0, by = 0;
        for (int qx = 1; qx <= 62; qx += 2)
            for (int qy = 1 + (qx / 2) % 2; qy <= 62; qy += 2) {
                float x = ut_grid(qx), y = ut_grid(qy), near = 9;
                if (hypotf(x - g->hx[v], y - g->hy[v]) < 0.25f) continue;
                for (int w = 0; w < nv; w++)
                    if (w != v) near = fminf(near, hypotf(x - g->x[w], y - g->y[w]));
                if (near < fminf(0.06f, 0.33f / sqrtf((float)nv))) continue;
                g->x[v] = x, g->y[v] = y;
                if (!crosses(g, v, moved)) continue;
                float m = room(g, v);
                if (m < fminf(0.06f, 0.33f / sqrtf((float)nv))) continue;
                float sc = fminf(m, cap) + 0.03f * (pr_next(&r) >> 8) / 16777216.0f;
                if (sc > best) best = sc, bx = x, by = y;
            }
        g->x[v] = g->hx[v], g->y[v] = g->hy[v];
        if (best < 0) return 0;
        g->x[v] = bx, g->y[v] = by;
    }
    if (least_room(g) < 0.05f) return 0;
    c->shape = si;
    c->k = k;
    c->crossings = ut_crossings(g);
    if (c->crossings < k || ut_cover(g, k - 2) < k - 1) return 0;
    c->score = nv + 2.5 * k + 0.05 * c->crossings;
    g->par = k;
    return 1;
}

/* a chapter's dots (lo..hi) and dots moved (lo..hi) */
static const struct { int v0, v1, k0, k1; } CHAPTER[UT_CHAPTERS] = {
    {6, 8, 2, 3}, {9, 12, 3, 4}, {12, 16, 4, 5}, {16, 21, 5, 6}, {21, 28, 6, 8}, {28, 36, 7, 9},
};

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score ? 1 : x->shape - y->shape;
}

static cand pool[MAXCANDS], chosen[UT_LEVELS];

int main(void)
{
    library();
    fprintf(stderr, "%d pictures\n", nshapes);
    int nchosen = 0;
    for (int ch = 0; ch < UT_CHAPTERS; ch++) {
        int n = 0, pics = 0;
        uint32_t seed = 1000u * (uint32_t)(ch + 1);
        for (int si = 0; si < nshapes; si++) {
            if (shapes[si].nv < CHAPTER[ch].v0 || shapes[si].nv > CHAPTER[ch].v1) continue;
            static ut_game t;
            if (!picture(&shapes[si], &t)) {
                fprintf(stderr, "  cramped: %s (%d dots, %d crossings, room %.3f)\n",
                        shapes[si].name, shapes[si].nv, ut_crossings(&t), least_room(&t));
                continue;
            }
            pics++;
            for (int k = CHAPTER[ch].k0; k <= CHAPTER[ch].k1; k++)
                for (int tr = 0; tr < TRIALS * (ch == 5 ? 3 : 1) && n < MAXCANDS; tr++)
                    if (make(&pool[n], si, k, seed++)) n++;
        }
        fprintf(stderr, "chapter %d: %d pictures, %d candidates\n", ch + 1, pics, n);
        qsort(pool, (size_t)n, sizeof pool[0], by_score);
        /* along the curve: slot i takes the candidate nearest its share of the difficulty whose picture and family
         * are not used up; the last slot a showpiece from the hardest third */
        static int used[MAXSHAPES], usedk[MAXSHAPES];
        int fam[F_FAMILIES] = {0};
        memset(used, 0, sizeof used);
        cand pick[25];
        for (int i = 0; i < 25; i++) {
            int last = i == 24, best = -1;
            double target = pool[(int)((i + 0.5) / 25 * n)].score, bd = 1e9;
            for (int relax = 0; relax < 2 && best < 0; relax++)  /* relaxed if none is left */
                for (int j = 0; j < n; j++) {
                    const cand *c = &pool[j];
                    const shape *s = &shapes[c->shape];
                    if (used[c->shape] >= 2 || (used[c->shape] && usedk[c->shape] == c->k)) continue;
                    /* a family a few times; the first half: all different pictures */
                    if (!relax && (fam[s->family] >= 5 || (used[c->shape] && i < 12))) continue;
                    if (last && (!s->show || j < 2 * n / 3)) continue;
                    double d = fabs(c->score - target) + (used[c->shape] ? 1.5 : 0);
                    if (d < bd) bd = d, best = j;
                }
            if (best < 0) {
                fprintf(stderr, "chapter %d: slot %d empty\n", ch + 1, i + 1);
                return 1;
            }
            pick[i] = pool[best];
            used[pool[best].shape]++;
            usedk[pool[best].shape] = pool[best].k;
            fam[shapes[pool[best].shape].family]++;
        }
        qsort(pick, 24, sizeof pick[0], by_score);
        for (int i = 0; i < 25; i++) {
            pick[i].cover = ut_cover(&pick[i].g, pick[i].k);
            chosen[nchosen++] = pick[i];
            fprintf(stderr, "  %3d %-22s %2d dots k %2d  %3d crossings  room %.3f\n", nchosen,
                    shapes[pick[i].shape].name, pick[i].g.nv, pick[i].k, pick[i].crossings, least_room(&pick[i].g));
        }
    }
    printf("/*\n * Untangle's levels: our own, made by tools/untangle_gen.c (a designed picture without crossings: wheels,\n"
           " * stars, flowers, webs, nested polygons, solids, grids, lattices; then a few dots moved out of it to roomy\n"
           " * places, kept when no fewer dots can untangle it), six chapters of 25 from the easiest, each ending on a\n"
           " * showpiece. Each: \"<dots>:\", the places, where each dot starts, the cords (untangle.h, ut_encode); in the\n"
           " * comment the picture, the dots, the cords, the crossings, the par (the dots moved) and the fewest dots that\n"
           " * might do.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
           "static const char *const ut_levels[UT_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[8 + 4 * UT_MAXV + 2 * UT_MAXE];
        ut_encode(&chosen[i].g, code, sizeof code);
        printf("    \"%s\", /* %d: %s, %d dots, %d cords, %d crossings, par %d, at least %d */\n", code, i + 1,
               shapes[chosen[i].shape].name, chosen[i].g.nv, chosen[i].g.ne, chosen[i].crossings, chosen[i].k,
               chosen[i].cover);
    }
    printf("};\n");
    return 0;
}

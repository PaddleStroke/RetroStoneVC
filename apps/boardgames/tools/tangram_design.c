/*
 * Tangram's designer (a tool, not in the app): a beam search laying the seven tans over a rough sketch read on stdin,
 * each new tan attached to the ones laid at a corner or at a point of an edge, so every position stays exact in
 * (a + b r2) / 4. tangram_design.py builds it and runs it over tangram_sketches.py; by hand:
 *     gcc -O2 -o tangram_design apps/boardgames/tools/tangram_design.c -lm
 *     ./tangram_design < sketch.txt
 * It prints "#sk ox oy scale x0 y0" (where the sketch lies on its canvas), then the best arrangements, the most
 * different ones first: score, then seven times "kind eighths flip ax bx ay by" (the tan's first corner at
 * ((ax + bx r2) / 4, (ay + by r2) / 4)). The tans' shapes are those of src/games/tangram_rules.c.
 * Sketch lines: "poly x y x y ...", "seg x0 y0 x1 y1 w", "disc cx cy r" ("hole" before one: cut it out); "beam n",
 * "out n", "lam f" (how much a tan outside the sketch costs), "first k" (the first tan's kind). Any unit: the sketch
 * is scaled to area 8.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R2 1.41421356237309505
#define RES 12          /* pixels a unit */
#define CW 168          /* canvas pixels (14 units) */

typedef struct { int a, b; } ex;   /* (a + b r2) / 4 */
static double fv(ex e) { return (e.a + e.b * R2) / 4.0; }
static ex add(ex x, ex y) { return (ex){x.a + y.a, x.b + y.b}; }
static ex sub(ex x, ex y) { return (ex){x.a - y.a, x.b - y.b}; }

static const char *KN = "LMSQP";
static const int CNT[5] = {2, 1, 2, 1, 1};
typedef struct { int kind, rot, flip, n; ex x[4], y[4]; double fx[4], fy[4]; } variant;
static variant V[5][16];
static int NV[5];

static void rot45(ex *x, ex *y)
{
    ex A = sub(*x, *y), B = add(*x, *y);
    if ((A.a & 1) || (B.a & 1)) { fprintf(stderr, "parity\n"); exit(1); }
    *x = (ex){A.b, A.a / 2};
    *y = (ex){B.b, B.a / 2};
}

static void make_variants(void)
{
    /* base shapes, in quarters: L (0,0)(2,0)(0,2); M (0,0)(r2,0)(0,r2); S; Q; P */
    static const ex B[5][4][2] = {
        {{{0, 0}, {0, 0}}, {{8, 0}, {0, 0}}, {{0, 0}, {8, 0}}},
        {{{0, 0}, {0, 0}}, {{0, 4}, {0, 0}}, {{0, 0}, {0, 4}}},
        {{{0, 0}, {0, 0}}, {{4, 0}, {0, 0}}, {{0, 0}, {4, 0}}},
        {{{0, 0}, {0, 0}}, {{4, 0}, {0, 0}}, {{4, 0}, {4, 0}}, {{0, 0}, {4, 0}}},
        {{{0, 0}, {0, 0}}, {{4, 0}, {0, 0}}, {{8, 0}, {4, 0}}, {{4, 0}, {4, 0}}},
    };
    static const int n[5] = {3, 3, 3, 4, 4};
    for (int k = 0; k < 5; k++) {
        NV[k] = 0;
        for (int flip = 0; flip < (k == 4 ? 2 : 1); flip++)
            for (int rot = 0; rot < 8; rot++) {
                variant v = {k, rot, flip, n[k], {{0}}, {{0}}, {0}, {0}};
                for (int i = 0; i < n[k]; i++) {
                    v.x[i] = B[k][i][0];
                    v.y[i] = B[k][i][1];
                    if (flip) v.x[i] = (ex){-v.x[i].a, -v.x[i].b};
                    for (int t = 0; t < rot; t++) rot45(&v.x[i], &v.y[i]);
                    v.fx[i] = fv(v.x[i]);
                    v.fy[i] = fv(v.y[i]);
                }
                /* the same set as an earlier variant: skip */
                int dup = 0;
                for (int j = 0; j < NV[k] && !dup; j++) {
                    variant *w = &V[k][j];
                    /* same shape: same vertex set up to translation */
                    double mx0 = 1e9, my0 = 1e9, mx1 = 1e9, my1 = 1e9;
                    for (int i = 0; i < n[k]; i++) {
                        mx0 = fmin(mx0, v.fx[i]); my0 = fmin(my0, v.fy[i]);
                        mx1 = fmin(mx1, w->fx[i]); my1 = fmin(my1, w->fy[i]);
                    }
                    int all = 1;
                    for (int i = 0; i < n[k] && all; i++) {
                        int f = 0;
                        for (int j2 = 0; j2 < n[k] && !f; j2++)
                            f = fabs(v.fx[i] - mx0 - (w->fx[j2] - mx1)) < 1e-6 && fabs(v.fy[i] - my0 - (w->fy[j2] - my1)) < 1e-6;
                        all = f;
                    }
                    dup = all;
                }
                if (!dup) V[k][NV[k]++] = v;
            }
    }
}

/* ---- the sketch ---- */
static unsigned char sk[CW][CW];
static int sk_n;

typedef struct { int kind, var; ex tx, ty; double px[4], py[4]; int n; } placed;
typedef struct { placed p[7]; int np, used[5]; double score; int in, out; unsigned long long key; } state;

static int inside_poly(const double *px, const double *py, int n, double x, double y)
{
    int c = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((py[i] > y) != (py[j] > y) && x < (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i]) c = !c;
    return c;
}

static double area(const double *x, const double *y, int n)
{
    double a = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += x[i] * y[j] - x[j] * y[i];
    }
    return a / 2;
}

/* intersection area of two convex polygons */
static double inter(const double *ax, const double *ay, int an, const double *bx, const double *by, int bn)
{
    double ox[16], oy[16], ix[16], iy[16];
    int on = an;
    for (int i = 0; i < an; i++) { ox[i] = ax[i]; oy[i] = ay[i]; }
    double s = area(bx, by, bn) > 0 ? 1 : -1;
    for (int e = 0; e < bn && on; e++) {
        double x0 = bx[e], y0 = by[e], x1 = bx[(e + 1) % bn], y1 = by[(e + 1) % bn];
        int in = on;
        memcpy(ix, ox, sizeof ox);
        memcpy(iy, oy, sizeof oy);
        on = 0;
        for (int i = 0; i < in; i++) {
            int j = (i + 1) % in;
            double si = s * ((x1 - x0) * (iy[i] - y0) - (y1 - y0) * (ix[i] - x0));
            double sj = s * ((x1 - x0) * (iy[j] - y0) - (y1 - y0) * (ix[j] - x0));
            if (si >= 0) { ox[on] = ix[i]; oy[on++] = iy[i]; }
            if ((si >= 0) != (sj >= 0)) {
                double t = si / (si - sj);
                ox[on] = ix[i] + t * (ix[j] - ix[i]);
                oy[on++] = iy[i] + t * (iy[j] - iy[i]);
            }
        }
    }
    return on >= 3 ? fabs(area(ox, oy, on)) : 0;
}

static double contact(const placed *a, const placed *b)
{
    double tot = 0;
    for (int i = 0; i < a->n; i++) {
        double x0 = a->px[i], y0 = a->py[i], x1 = a->px[(i + 1) % a->n], y1 = a->py[(i + 1) % a->n];
        double L = hypot(x1 - x0, y1 - y0), ux = (x1 - x0) / L, uy = (y1 - y0) / L;
        for (int j = 0; j < b->n; j++) {
            double qx0 = b->px[j], qy0 = b->py[j], qx1 = b->px[(j + 1) % b->n], qy1 = b->py[(j + 1) % b->n];
            if (fabs(ux * (qy0 - y0) - uy * (qx0 - x0)) > 1e-7 || fabs(ux * (qy1 - y0) - uy * (qx1 - x0)) > 1e-7) continue;
            double t0 = ux * (qx0 - x0) + uy * (qy0 - y0), t1 = ux * (qx1 - x0) + uy * (qy1 - y0);
            double lo = fmax(0, fmin(t0, t1)), hi = fmin(L, fmax(t0, t1));
            if (hi > lo) tot += hi - lo;
        }
    }
    return tot;
}

static void put(placed *p, int kind, int var, ex tx, ex ty)
{
    variant *v = &V[kind][var];
    p->kind = kind;
    p->var = var;
    p->tx = tx;
    p->ty = ty;
    p->n = v->n;
    for (int i = 0; i < v->n; i++) {
        p->px[i] = v->fx[i] + fv(tx);
        p->py[i] = v->fy[i] + fv(ty);
    }
}

/* pixels of a piece in / out of the sketch; -1: off the canvas */
static int raster(const placed *p, int *in, int *out)
{
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (int i = 0; i < p->n; i++) {
        x0 = fmin(x0, p->px[i]); y0 = fmin(y0, p->py[i]);
        x1 = fmax(x1, p->px[i]); y1 = fmax(y1, p->py[i]);
    }
    int X0 = (int)floor(x0 * RES), Y0 = (int)floor(y0 * RES), X1 = (int)ceil(x1 * RES), Y1 = (int)ceil(y1 * RES);
    if (X0 < 0 || Y0 < 0 || X1 >= CW || Y1 >= CW) return -1;
    *in = *out = 0;
    for (int y = Y0; y < Y1; y++)
        for (int x = X0; x < X1; x++)
            if (inside_poly(p->px, p->py, p->n, (x + 0.5) / RES, (y + 0.5) / RES)) {
                if (sk[y][x]) (*in)++;
                else (*out)++;
            }
    return 0;
}

static double LAM = 1.6;
static double scoreof(int in, int out) { return in - LAM * out; }

static unsigned long long keyof(const state *s)
{
    /* order-free: sum of hashes */
    unsigned long long k = 0;
    for (int i = 0; i < s->np; i++) {
        const placed *p = &s->p[i];
        /* the shape's float vertices (rounded) */
        unsigned long long h = 1469598103934665603ull;
        long long sx = 0, sy = 0;
        for (int j = 0; j < p->n; j++) {
            sx += llround(p->px[j] * 1000);
            sy += llround(p->py[j] * 1000);
        }
        h = (h ^ (unsigned long long)(sx + 7919 * p->kind)) * 1099511628211ull;
        h = (h ^ (unsigned long long)sy) * 1099511628211ull;
        k += h * 2654435761ull;
    }
    return k;
}

static int cmp_state(const void *a, const void *b)
{
    double d = ((const state *)b)->score - ((const state *)a)->score;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

static state *beam, *next;
static int nbeam, nnext, BEAM = 300, OUT = 4, FIRST = 0;
static int cap_next;

static void push(const state *s)
{
    if (nnext >= cap_next) {
        /* keep the best half */
        qsort(next, nnext, sizeof *next, cmp_state);
        nnext = cap_next / 2;
    }
    next[nnext++] = *s;
}

/* attachment points of a state: vertices, and points along edges at 1, r2, 2 from each end */
static int points(const state *s, ex *X, ex *Y, int max)
{
    int n = 0;
    for (int i = 0; i < s->np; i++) {
        const placed *p = &s->p[i];
        variant *v = &V[p->kind][p->var];
        for (int j = 0; j < v->n && n < max; j++) {
            ex x = add(v->x[j], p->tx), y = add(v->y[j], p->ty);
            int dup = 0;
            for (int k = 0; k < n && !dup; k++) dup = X[k].a == x.a && X[k].b == x.b && Y[k].a == y.a && Y[k].b == y.b;
            if (!dup) { X[n] = x; Y[n++] = y; }
        }
        for (int j = 0; j < v->n; j++) {
            int j2 = (j + 1) % v->n;
            ex ax = add(v->x[j], p->tx), ay = add(v->y[j], p->ty), bx = add(v->x[j2], p->tx), by = add(v->y[j2], p->ty);
            ex dx = sub(bx, ax), dy = sub(by, ay);
            double L = hypot(fv(dx), fv(dy));
            /* the unit direction, exact: axis (1,0) = (4,0); diagonal (h, h) = (0,2) */
            ex ux, uy;
            double fdx = fv(dx), fdy = fv(dy);
            int diag = fabs(fdx) > 1e-9 && fabs(fdy) > 1e-9;
            if (diag) { ux = (ex){0, fdx > 0 ? 2 : -2}; uy = (ex){0, fdy > 0 ? 2 : -2}; }
            else { ux = (ex){fabs(fdx) > 1e-9 ? (fdx > 0 ? 4 : -4) : 0, 0}; uy = (ex){fabs(fdy) > 1e-9 ? (fdy > 0 ? 4 : -4) : 0, 0}; }
            static const double D[3] = {1, R2, 2};
            for (int d = 0; d < 3; d++) {
                if (D[d] > L - 0.05) continue;
                /* d * u: d = 1: u; r2: u * r2; 2: 2u. u*r2: (a + b r2) r2 = 2b + a r2 */
                ex mx, my;
                if (d == 0) { mx = ux; my = uy; }
                else if (d == 1) { mx = (ex){2 * ux.b, ux.a}; my = (ex){2 * uy.b, uy.a}; }
                else { mx = (ex){2 * ux.a, 2 * ux.b}; my = (ex){2 * uy.a, 2 * uy.b}; }
                ex px[2] = {add(ax, mx), sub(bx, mx)}, py[2] = {add(ay, my), sub(by, my)};
                for (int e = 0; e < 2 && n < max; e++) {
                    int dup = 0;
                    for (int k = 0; k < n && !dup; k++)
                        dup = X[k].a == px[e].a && X[k].b == px[e].b && Y[k].a == py[e].a && Y[k].b == py[e].b;
                    if (!dup) { X[n] = px[e]; Y[n++] = py[e]; }
                }
            }
        }
    }
    return n;
}

static void expand(const state *s)
{
    ex X[256], Y[256];
    int np = points(s, X, Y, 256);
    for (int k = 0; k < 5; k++) {
        if (s->used[k] >= CNT[k]) continue;
        for (int vi = 0; vi < NV[k]; vi++) {
            variant *v = &V[k][vi];
            for (int a = 0; a < np; a++)
                for (int i = 0; i < v->n; i++) {
                    placed c;
                    put(&c, k, vi, sub(X[a], v->x[i]), sub(Y[a], v->y[i]));
                    int ok = 1;
                    double con = 0;
                    for (int j = 0; j < s->np && ok; j++) {
                        if (inter(c.px, c.py, c.n, s->p[j].px, s->p[j].py, s->p[j].n) > 1e-6) ok = 0;
                        else con += contact(&c, &s->p[j]);
                    }
                    if (!ok || con < 0.3) continue;
                    int in, out;
                    if (raster(&c, &in, &out) < 0) continue;
                    state t = *s;
                    t.p[t.np++] = c;
                    t.used[k]++;
                    t.in += in;
                    t.out += out;
                    t.score = scoreof(t.in, t.out) + 0.02 * con;
                    t.key = keyof(&t);
                    push(&t);
                }
        }
    }
}

static void dedupe_cut(void)
{
    qsort(next, nnext, sizeof *next, cmp_state);
    nbeam = 0;
    for (int i = 0; i < nnext && nbeam < BEAM; i++) {
        int dup = 0;
        for (int j = 0; j < nbeam && !dup; j++) dup = beam[j].key == next[i].key;
        if (!dup) beam[nbeam++] = next[i];
    }
    nnext = 0;
}

/* ---- the sketch reader ---- */
typedef struct { int kind, hole, n; double v[64]; } shape;
static shape SH[64];
static int nsh;

static int in_shape(const shape *s, double x, double y)
{
    if (s->kind == 0) {
        double px[32], py[32];
        for (int i = 0; i < s->n / 2; i++) { px[i] = s->v[2 * i]; py[i] = s->v[2 * i + 1]; }
        return inside_poly(px, py, s->n / 2, x, y);
    }
    if (s->kind == 1) {
        double x0 = s->v[0], y0 = s->v[1], x1 = s->v[2], y1 = s->v[3], w = s->v[4];
        double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, t = ((x - x0) * dx + (y - y0) * dy) / l2;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        return hypot(x - x0 - t * dx, y - y0 - t * dy) <= w / 2;
    }
    return hypot(x - s->v[0], y - s->v[1]) <= s->v[2];
}

static int in_sketch(double x, double y)
{
    int in = 0;
    for (int i = 0; i < nsh; i++)
        if (in_shape(&SH[i], x, y)) in = !SH[i].hole;
    return in;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    make_variants();
    char line[2048];
    int hole = 0;
    while (fgets(line, sizeof line, stdin)) {
        char w[16];
        int off;
        if (sscanf(line, "%15s%n", w, &off) != 1) continue;
        char *p = line + off;
        if (!strcmp(w, "beam")) { BEAM = atoi(p); continue; }
        if (!strcmp(w, "out")) { OUT = atoi(p); continue; }
        if (!strcmp(w, "lam")) { LAM = atof(p); continue; }
        if (!strcmp(w, "first")) { FIRST = atoi(p); continue; }
        if (!strcmp(w, "hole")) { hole = 1; continue; }
        shape *s = &SH[nsh];
        s->kind = !strcmp(w, "poly") ? 0 : !strcmp(w, "seg") ? 1 : !strcmp(w, "disc") ? 2 : -1;
        if (s->kind < 0) continue;
        s->hole = hole;
        hole = 0;
        s->n = 0;
        double d;
        int o;
        while (s->n < 64 && sscanf(p, "%lf%n", &d, &o) == 1) { s->v[s->n++] = d; p += o; }
        nsh++;
    }
    /* the sketch's area and box, at a fine sampling */
    double bx0 = 1e9, by0 = 1e9, bx1 = -1e9, by1 = -1e9;
    for (int i = 0; i < nsh; i++) {
        shape *s = &SH[i];
        if (s->kind == 0)
            for (int j = 0; j < s->n; j += 2) {
                bx0 = fmin(bx0, s->v[j]); bx1 = fmax(bx1, s->v[j]);
                by0 = fmin(by0, s->v[j + 1]); by1 = fmax(by1, s->v[j + 1]);
            }
        else if (s->kind == 1) {
            double w = s->v[4] / 2;
            bx0 = fmin(bx0, fmin(s->v[0], s->v[2]) - w); bx1 = fmax(bx1, fmax(s->v[0], s->v[2]) + w);
            by0 = fmin(by0, fmin(s->v[1], s->v[3]) - w); by1 = fmax(by1, fmax(s->v[1], s->v[3]) + w);
        } else {
            bx0 = fmin(bx0, s->v[0] - s->v[2]); bx1 = fmax(bx1, s->v[0] + s->v[2]);
            by0 = fmin(by0, s->v[1] - s->v[2]); by1 = fmax(by1, s->v[1] + s->v[2]);
        }
    }
    int cnt = 0, G = 400;
    for (int y = 0; y < G; y++)
        for (int x = 0; x < G; x++)
            cnt += in_sketch(bx0 + (x + 0.5) * (bx1 - bx0) / G, by0 + (y + 0.5) * (by1 - by0) / G);
    double a = cnt * (bx1 - bx0) * (by1 - by0) / G / G, sc = sqrt(8.0 / a);
    double W = (bx1 - bx0) * sc, H = (by1 - by0) * sc, ox = (14 - W) / 2, oy = (14 - H) / 2;
    sk_n = 0;
    for (int y = 0; y < CW; y++)
        for (int x = 0; x < CW; x++) {
            double ux = ((x + 0.5) / RES - ox) / sc + bx0, uy = ((y + 0.5) / RES - oy) / sc + by0;
            sk[y][x] = (unsigned char)in_sketch(ux, uy);
            sk_n += sk[y][x];
        }
    fprintf(stderr, "sketch %.2f x %.2f units, %d px\n", W, H, sk_n);
    cap_next = 120000;
    beam = malloc(sizeof(state) * (BEAM + 1));
    next = malloc(sizeof(state) * cap_next);
    /* the first tan: a large triangle anywhere on a quarter grid */
    nnext = 0;
    for (int vi = 0; vi < NV[FIRST]; vi++)
        for (int ty = 0; ty < 14 * 4; ty++)
            for (int tx = 0; tx < 14 * 4; tx++) {
                state s;
                memset(&s, 0, sizeof s);
                put(&s.p[0], FIRST, vi, (ex){tx, 0}, (ex){ty, 0});
                int in, out;
                if (raster(&s.p[0], &in, &out) < 0) continue;
                if (out > in / 3) continue;
                s.np = 1;
                s.used[FIRST] = 1;
                s.in = in;
                s.out = out;
                s.score = scoreof(in, out);
                s.key = keyof(&s);
                push(&s);
            }
    dedupe_cut();
    for (int step = 1; step < 7; step++) {
        for (int i = 0; i < nbeam; i++) expand(&beam[i]);
        dedupe_cut();
        fprintf(stderr, "step %d: %d states, best %.0f\n", step, nbeam, nbeam ? beam[0].score : 0);
    }
    printf("#sk %f %f %f %f %f\n", ox, oy, sc, bx0, by0);
    static unsigned char um[64][CW][CW];
    int shown = 0;
    for (int i = 0; i < nbeam && shown < OUT; i++) {
        state *s = &beam[i];
        if (s->np < 7) continue;
        unsigned char (*m)[CW] = um[shown];
        memset(m, 0, sizeof um[0]);
        for (int j = 0; j < s->np; j++)
            for (int y = 0; y < CW; y++)
                for (int x = 0; x < CW; x++)
                    if (inside_poly(s->p[j].px, s->p[j].py, s->p[j].n, (x + 0.5) / RES, (y + 0.5) / RES)) m[y][x] = 1;
        int similar = 0;
        for (int k = 0; k < shown && !similar; k++) {
            int same = 0, any = 0;
            for (int y = 0; y < CW; y++)
                for (int x = 0; x < CW; x++) {
                    same += m[y][x] && um[k][y][x];
                    any += m[y][x] || um[k][y][x];
                }
            similar = same > 0.9 * any;
        }
        if (similar) continue;
        shown++;
        printf("%.0f", s->score);
        for (int j = 0; j < s->np; j++) {
            placed *p = &s->p[j];
            variant *v = &V[p->kind][p->var];
            ex x = add(v->x[0], p->tx), y = add(v->y[0], p->ty);
            printf(" %c %d %d %d %d %d %d", KN[p->kind], v->rot, v->flip, x.a, x.b, y.a, y.b);
        }
        printf("\n");
    }
    return 0;
}

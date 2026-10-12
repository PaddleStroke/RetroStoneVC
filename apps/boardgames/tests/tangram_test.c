/*
 * Tangram's rules: every figure checked by construction (its seven tans, none overlapping, area 8, held together edge
 * to edge, no hole, its box small enough); then the check by samples (the made solution solves; the figure turned
 * about its middle, a solution of its own, solves too when the silhouette allows it; a tan a little off does not),
 * the fitting places (a tan let go near its place lands exactly there, laid in any order), hints, turns, the
 * parallelogram turned over, undo, the tray at the right, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/tangram.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static double area(const double *xy, int n)
{
    double a = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        a += xy[2 * i] * xy[2 * j + 1] - xy[2 * j] * xy[2 * i + 1];
    }
    return a / 2;
}

/* the area two convex polygons share */
static double shared(const double *a, int an, const double *b, int bn)
{
    double o[32], in[32];
    int on = an;
    memcpy(o, a, sizeof(double) * 2 * an);
    double s = area(b, bn) > 0 ? 1 : -1;
    for (int e = 0; e < bn && on; e++) {
        double x0 = b[2 * e], y0 = b[2 * e + 1], x1 = b[2 * ((e + 1) % bn)], y1 = b[2 * ((e + 1) % bn) + 1];
        int n = on;
        memcpy(in, o, sizeof(double) * 2 * n);
        on = 0;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            double si = s * ((x1 - x0) * (in[2 * i + 1] - y0) - (y1 - y0) * (in[2 * i] - x0));
            double sj = s * ((x1 - x0) * (in[2 * j + 1] - y0) - (y1 - y0) * (in[2 * j] - x0));
            if (si >= 0) { o[2 * on] = in[2 * i]; o[2 * on++ + 1] = in[2 * i + 1]; }
            if ((si >= 0) != (sj >= 0)) {
                double t = si / (si - sj);
                o[2 * on] = in[2 * i] + t * (in[2 * j] - in[2 * i]);
                o[2 * on + 1] = in[2 * i + 1] + t * (in[2 * j + 1] - in[2 * i + 1]);
                on++;
            }
        }
    }
    return on >= 3 ? fabs(area(o, on)) : 0;
}

/* the length of the boundary two polygons share */
static double contact(const double *a, int an, const double *b, int bn)
{
    double tot = 0;
    for (int i = 0; i < an; i++) {
        double x0 = a[2 * i], y0 = a[2 * i + 1], x1 = a[2 * ((i + 1) % an)], y1 = a[2 * ((i + 1) % an) + 1];
        double L = hypot(x1 - x0, y1 - y0), ux = (x1 - x0) / L, uy = (y1 - y0) / L;
        for (int j = 0; j < bn; j++) {
            double qx0 = b[2 * j], qy0 = b[2 * j + 1], qx1 = b[2 * ((j + 1) % bn)], qy1 = b[2 * ((j + 1) % bn) + 1];
            if (fabs(ux * (qy0 - y0) - uy * (qx0 - x0)) > 1e-9 || fabs(ux * (qy1 - y0) - uy * (qx1 - x0)) > 1e-9)
                continue;
            double t0 = ux * (qx0 - x0) + uy * (qy0 - y0), t1 = ux * (qx1 - x0) + uy * (qy1 - y0);
            double lo = fmax(0, fmin(t0, t1)), hi = fmin(L, fmax(t0, t1));
            if (hi > lo) tot += hi - lo;
        }
    }
    return tot;
}

/* a figure made right: its tans, no overlap, area 8, held together, no hole, small enough */
static int well_made(int level)
{
    static const int KIND[TGM_PIECES] = {TGM_L, TGM_L, TGM_M, TGM_S, TGM_S, TGM_Q, TGM_P};
    tgm_place s[TGM_PIECES];
    double w, h, c[TGM_PIECES][8], total = 0;
    int n[TGM_PIECES], ok = 1;
    tgm_solution(level, s, &w, &h);
    for (int i = 0; i < TGM_PIECES; i++) {
        if (tgm_figures[level].t[i][0] != KIND[i]) ok = 0;
        n[i] = tgm_corners(i, &s[i], c[i]);
        total += fabs(area(c[i], n[i]));
        for (int k = 0; k < n[i]; k++)
            if (c[i][2 * k] < -1e-9 || c[i][2 * k + 1] < -1e-9) ok = 0;
    }
    if (fabs(total - 8) > 1e-9 || w > TGM_MAXU || h > TGM_MAXU || tgm_figures[level].chapter >= TGM_CHAPTERS) ok = 0;
    int touch[TGM_PIECES][TGM_PIECES] = {{0}};
    for (int i = 0; i < TGM_PIECES; i++)
        for (int j = i + 1; j < TGM_PIECES; j++) {
            if (shared(c[i], n[i], c[j], n[j]) > 1e-9) ok = 0;
            touch[i][j] = touch[j][i] = contact(c[i], n[i], c[j], n[j]) > 0.2;
        }
    int seen = 1, stack[TGM_PIECES], sp = 0;
    stack[sp++] = 0;
    while (sp) {
        int a = stack[--sp];
        for (int b = 0; b < TGM_PIECES; b++)
            if (touch[a][b] && !(seen >> b & 1)) { seen |= 1 << b; stack[sp++] = b; }
    }
    if (seen != (1 << TGM_PIECES) - 1) ok = 0;
    /* no hole: the outside, filled from the border of a sampled box, reaches every sample not covered */
    enum { R = 20, G = (TGM_MAXU + 1) * R };
    static unsigned char m[G][G];
    static int st[G * G];
    int gw = (int)(w * R) + 4, gh = (int)(h * R) + 4, top = 0, holes = 0;
    if (gw > G || gh > G) return 0;
    for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) {
            m[y][x] = 0;
            for (int i = 0; i < TGM_PIECES && !m[y][x]; i++) {
                double px = (x - 2 + 0.5) / R + 0.00731, py = (y - 2 + 0.5) / R + 0.00317;
                int in = 0;
                for (int a = 0, b = n[i] - 1; a < n[i]; b = a++)
                    if ((c[i][2 * a + 1] > py) != (c[i][2 * b + 1] > py) &&
                        px < (c[i][2 * b] - c[i][2 * a]) * (py - c[i][2 * a + 1]) / (c[i][2 * b + 1] - c[i][2 * a + 1]) +
                                 c[i][2 * a])
                        in = !in;
                m[y][x] = (unsigned char)in;
            }
        }
    m[0][0] = 2;
    st[top++] = 0;
    while (top) {
        int p = st[--top], x = p % gw, y = p / gw;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int d = 0; d < 4; d++) {
            int nx = x + D[d][0], ny = y + D[d][1];
            if (nx < 0 || ny < 0 || nx >= gw || ny >= gh || m[ny][nx]) continue;
            m[ny][nx] = 2;
            st[top++] = ny * gw + nx;
        }
    }
    for (int y = 0; y < gh; y++)
        for (int x = 0; x < gw; x++) holes += !m[y][x];
    return ok && holes == 0;
}

/* the made solution turned a quarter (q 1) or a half (q 2) turn about the silhouette's middle */
static void turned(tgm_game *g, int q)
{
    double cx = g->fx + g->fw / 2, cy = g->fy + g->fh / 2;
    for (int i = 0; i < TGM_PIECES; i++) {
        tgm_place p = g->slot[i];
        double dx = p.x - cx, dy = p.y - cy;
        for (int k = 0; k < q; k++) {
            double t = dx;
            dx = -dy;
            dy = t;
        }
        g->p[i] = (tgm_place){cx + dx, cy + dy, (p.rot + 2 * q) & 7, p.flip};
    }
}

int main(void)
{
    int bad = 0, per[TGM_CHAPTERS] = {0};
    uint32_t r = pr_seed(7);
    for (int f = 0; f < TGM_LEVELS; f++) {
        per[tgm_chapter(f)]++;
        if (f && tgm_chapter(f) < tgm_chapter(f - 1)) bad++;   /* the chapters in order */
        if (!well_made(f)) { printf("figure %d is not well made\n", f + 1); bad++; }
        tgm_game g;
        tgm_new(&g, f, 1);
        CHECK(!tgm_solved(&g) && tgm_check(&g) > 100);
        /* the hints solve it, a tan at a time */
        int hints = 0;
        while (!tgm_solved(&g) && hints < 10 && tgm_hint(&g) >= 0) hints++;
        CHECK(tgm_solved(&g) && hints <= TGM_PIECES && g.moves == hints && tgm_check(&g) == 0);
        CHECK(tgm_stars(&g) == 1 && tgm_hint(&g) < 0);
        /* the large triangles swapped, the small ones too: still solved */
        tgm_place a = g.p[0];
        g.p[0] = g.p[1];
        g.p[1] = a;
        a = g.p[3];
        g.p[3] = g.p[4];
        g.p[4] = a;
        CHECK(tgm_check(&g) == 0);
        for (int i = 0; i < TGM_PIECES; i++) CHECK(tgm_in_place(&g, i));
        /* a tan let go near its place lands exactly there; a tan half a unit off does not solve */
        for (int i = 0; i < TGM_PIECES; i++) {
            double dx = (pr_below(&r, 21) - 10) / 100.0, dy = (pr_below(&r, 21) - 10) / 100.0;
            tgm_move(&g, i, g.p[i].x + dx, g.p[i].y + dy);
            if (!tgm_solved(&g) || !tgm_in_place(&g, i)) {
                printf("figure %d: tan %d off by %.2f, %.2f does not settle\n", f + 1, i, dx, dy);
                bad++;
            }
        }
        g.p[5].x += 0.5;
        CHECK(tgm_check(&g) > TGM_RES * TGM_RES / 16);
        g.p[5].x -= 0.5;
        /* laid from the tray in a random order, each near its place, turned right: solved */
        tgm_new(&g, f, (uint32_t)f + 2);
        int order[TGM_PIECES] = {0, 1, 2, 3, 4, 5, 6};
        for (int i = TGM_PIECES - 1; i > 0; i--) {
            int j = pr_below(&r, i + 1), t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (int k = 0; k < TGM_PIECES; k++) {
            int i = order[k];
            g.p[i].rot = g.slot[i].rot;
            g.p[i].flip = g.slot[i].flip;
            tgm_move(&g, i, g.slot[i].x + (pr_below(&r, 17) - 8) / 100.0, g.slot[i].y + (pr_below(&r, 17) - 8) / 100.0);
        }
        if (!tgm_solved(&g)) { printf("figure %d: not solved when laid in order\n", f + 1); bad++; }
    }
    CHECK(bad == 0);
    printf("tangram: %d figures checked (", TGM_LEVELS);
    for (int c = 0; c < TGM_CHAPTERS; c++) printf("%s%d", c ? " " : "", per[c]);
    printf(")\n");

    /* any arrangement covering the silhouette counts: the square (level 1) turned a quarter, the rectangle (2) a
     * half turn, are solutions of their own */
    tgm_game g;
    tgm_new(&g, 0, 3);
    turned(&g, 1);
    CHECK(tgm_check(&g) == 0 && !tgm_in_place(&g, 6));
    tgm_new(&g, 1, 3);
    turned(&g, 2);
    CHECK(tgm_check(&g) == 0);

    /* turns: eight make a whole turn; the parallelogram turns over, the others do not */
    tgm_new(&g, 20, 5);
    tgm_place p0 = g.p[2];
    for (int i = 0; i < 8; i++) CHECK(tgm_turn(&g, 2));
    CHECK(g.p[2].rot == p0.rot && fabs(g.p[2].x - p0.x) < 1 && fabs(g.p[2].y - p0.y) < 1);
    int fl = g.p[6].flip;
    CHECK(!tgm_flip(&g, 0) && tgm_flip(&g, 6) && g.p[6].flip != fl && tgm_flip(&g, 6) && g.p[6].flip == fl);
    /* nothing near (the others in their places, the tray empty): a corner on the grid of half units */
    tgm_game e;
    tgm_new(&e, 20, 5);
    CHECK(tgm_script(&e, "=") && tgm_move(&e, 5, e.tray_x + 4.13, e.tray_y + 2.31));
    CHECK(!tgm_solved(&e));
    double xy[8];
    tgm_corners(5, &e.p[5], xy);
    int on = 0;
    for (int k = 0; k < 4; k++)
        on |= fabs((xy[2 * k] - e.fx) * 2 - round((xy[2 * k] - e.fx) * 2)) < 1e-6 &&
              fabs((xy[2 * k + 1] - e.fy) * 2 - round((xy[2 * k + 1] - e.fy) * 2)) < 1e-6;
    CHECK(on);
    while (tgm_undo(&g) >= 0) {}
    tgm_game fresh;
    tgm_new(&fresh, 20, 5);
    CHECK(!memcmp(fresh.p, g.p, sizeof g.p) && g.moves == 0);
    /* the tray at the right: the tans in the tray go with it, the made solution with the silhouette */
    CHECK(tgm_script(&g, "h h"));
    tgm_arrange(&g, 1);
    CHECK(g.tray_right && tgm_in_place(&g, g.hist[0].piece) && tgm_in_place(&g, g.hist[1].piece));
    tgm_arrange(&g, 0);
    CHECK(!g.tray_right && g.table_w == fresh.table_w && g.table_h == fresh.table_h);
    CHECK(tgm_script(&g, "=") && tgm_solved(&g) && tgm_stars(&g) == 1);
    CHECK(!tgm_script(&g, "n0") && !tgm_script(&g, "n99") && !tgm_script(&g, "x1") && tgm_script(&g, "n98"));
    CHECK(g.level == 97 && !tgm_solved(&g));
    tgm_new(&g, 0, 9);
    CHECK(tgm_script(&g, "h") && tgm_stars(&g) == 2 && tgm_script(&g, "m5:0.4,0.4") && g.moves == 2);

    /* whole games: random moves, turns and turnings over, then the hints to the end */
    int whole = 1;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        uint32_t s = pr_seed(seed);
        tgm_new(&g, (int)(seed * 7 % TGM_LEVELS), seed);
        tgm_arrange(&g, (int)(seed & 1));
        int n = pr_below(&s, 25);
        for (int i = 0; i < n; i++) {
            int piece = pr_below(&s, TGM_PIECES), what = pr_below(&s, 3);
            if (what == 0) tgm_move(&g, piece, pr_below(&s, 100) / 10.0, pr_below(&s, 140) / 10.0);
            else if (what == 1) tgm_turn(&g, piece);
            else tgm_flip(&g, piece);
        }
        for (int k = 0; k < 20 && !tgm_solved(&g); k++) tgm_hint(&g);
        if (!tgm_solved(&g)) whole = 0;
    }
    CHECK(whole);

    if (fails) { printf("tangram: %d failures\n", fails); return 1; }
    printf("tangram: all passed\n");
    return 0;
}

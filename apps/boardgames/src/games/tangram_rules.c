/*
 * Tangram: the rules (tangram.h): the tans turned and turned over, the figures read from their tans, the table and
 * its tray, the fitting places a tan is drawn to, the sampled check, hints and undo. No SDL: tested by
 * tests/tangram_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tangram.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/tangram_figures.h"

#define R2 1.41421356237309505
#define SNAP 0.34                               /* how far a tan is drawn to a fitting place (units) */
#define SAME 1e-5
#define MARGIN 0.5
#define TRAY_L 8.4                              /* the tray: long side, short side */
#define TRAY_S 4.4

static const int TYPE[TGM_PIECES] = {TGM_L, TGM_L, TGM_M, TGM_S, TGM_S, TGM_Q, TGM_P};
static const double BASE[5][8] = {{0, 0, 2, 0, 0, 2}, {0, 0, R2, 0, 0, R2}, {0, 0, 1, 0, 0, 1},
                                  {0, 0, 1, 0, 1, 1, 0, 1}, {0, 0, 1, 0, 2, 1, 1, 1}};
static const int NV[5] = {3, 3, 3, 4, 4};

int tgm_type(int piece) { return TYPE[piece < 0 || piece >= TGM_PIECES ? 0 : piece]; }

int tgm_chapter(int level)
{
    return level < 0 || level >= TGM_LEVELS ? 0 : tgm_figures[level].chapter;
}

/* a shape turned over (x mirrored) then turned rot eighths clockwise: its corners from its first one */
static int shape0(int type, int rot, int flip, double *xy)
{
    static const double C[8] = {1, 0.70710678118654752, 0, -0.70710678118654752, -1, -0.70710678118654752, 0,
                                0.70710678118654752};
    double c = C[rot & 7], s = C[(rot + 6) & 7];   /* sin: cos a quarter turn back */
    int n = NV[type];
    for (int i = 0; i < n; i++) {
        double x = BASE[type][2 * i], y = BASE[type][2 * i + 1];
        if (flip) x = -x;
        xy[2 * i] = x * c - y * s;
        xy[2 * i + 1] = x * s + y * c;
    }
    return n;
}

/* the same, its middle at the origin */
static int shape(int type, int rot, int flip, double *xy)
{
    int n = shape0(type, rot, flip, xy);
    double mx = 0, my = 0;
    for (int i = 0; i < n; i++) {
        mx += xy[2 * i];
        my += xy[2 * i + 1];
    }
    for (int i = 0; i < n; i++) {
        xy[2 * i] -= mx / n;
        xy[2 * i + 1] -= my / n;
    }
    return n;
}

int tgm_corners(int piece, const tgm_place *pl, double *xy)
{
    int n = shape(tgm_type(piece), pl->rot, pl->flip, xy);
    for (int i = 0; i < n; i++) {
        xy[2 * i] += pl->x;
        xy[2 * i + 1] += pl->y;
    }
    return n;
}

void tgm_solution(int level, tgm_place *out, double *w, double *h)
{
    if (level < 0 || level >= TGM_LEVELS) level = 0;
    double W = 0, H = 0;
    for (int i = 0; i < TGM_PIECES; i++) {
        const int8_t *t = tgm_figures[level].t[i];
        double xy[8], c[8];
        int n = shape0(t[0], t[1], t[2], xy);
        shape(t[0], t[1], t[2], c);
        double ax = (t[3] + t[4] * R2) / 4, ay = (t[5] + t[6] * R2) / 4;
        /* the middle: the first corner's place less its offset from the middle */
        out[i] = (tgm_place){ax - c[0], ay - c[1], t[1], t[2]};
        for (int k = 0; k < n; k++) {
            W = fmax(W, ax + xy[2 * k] - xy[0]);
            H = fmax(H, ay + xy[2 * k + 1] - xy[1]);
        }
    }
    if (w) *w = W;
    if (h) *h = H;
}

/* the tray's places, along its long side: the large triangles, the medium, the square; then the small ones and the
 * parallelogram */
static const double TRAY[TGM_PIECES][2] = {{1.3, 1.2}, {3.7, 1.2}, {5.9, 1.2}, {2.2, 3.3},
                                           {3.8, 3.3}, {7.5, 1.2}, {6.0, 3.3}};

static void tray_place(const tgm_game *g, int i, double *x, double *y)
{
    *x = g->tray_x + (g->tray_right ? TRAY[i][1] : TRAY[i][0]);
    *y = g->tray_y + (g->tray_right ? TRAY[i][0] : TRAY[i][1]);
}

static void layout_table(tgm_game *g)
{
    if (!g->tray_right) {
        g->table_w = (int)ceil(fmax(g->fw + 2 * MARGIN, TRAY_L + 0.6));
        g->fx = (g->table_w - g->fw) / 2;
        g->fy = MARGIN;
        g->tray_x = (g->table_w - TRAY_L) / 2;
        g->tray_y = g->fh + 2 * MARGIN - 0.2;
        g->table_h = (int)ceil(g->tray_y + TRAY_S + 0.2);
    } else {
        g->table_h = (int)ceil(fmax(g->fh + 2 * MARGIN, TRAY_L + 0.6));
        g->fy = (g->table_h - g->fh) / 2;
        g->fx = MARGIN;
        g->tray_x = g->fw + 2 * MARGIN - 0.2;
        g->tray_y = (g->table_h - TRAY_L) / 2;
        g->table_w = (int)ceil(g->tray_x + TRAY_S + 0.2);
    }
}

void tgm_new(tgm_game *g, int level, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (level < 0 || level >= TGM_LEVELS) level = 0;
    g->level = level;
    g->seed = seed;
    tgm_solution(level, g->slot, &g->fw, &g->fh);
    layout_table(g);
    for (int i = 0; i < TGM_PIECES; i++) {
        g->slot[i].x += g->fx;
        g->slot[i].y += g->fy;
    }
    uint32_t s = pr_seed(seed);
    for (int i = 0; i < TGM_PIECES; i++) {
        tray_place(g, i, &g->p[i].x, &g->p[i].y);
        g->p[i].rot = 2 * pr_below(&s, 4);
        g->p[i].flip = i == 6 ? pr_below(&s, 2) : 0;
    }
}

static int in_tray(const tgm_game *g, double x, double y)
{
    double w = g->tray_right ? TRAY_S : TRAY_L, h = g->tray_right ? TRAY_L : TRAY_S;
    return x >= g->tray_x - 0.1 && y >= g->tray_y - 0.1 && x <= g->tray_x + w + 0.1 && y <= g->tray_y + h + 0.1;
}

void tgm_arrange(tgm_game *g, int tray_right)
{
    tray_right = !!tray_right;
    if (tray_right == g->tray_right) return;
    double fx = g->fx, fy = g->fy;
    int intray[TGM_PIECES];
    for (int i = 0; i < TGM_PIECES; i++) intray[i] = in_tray(g, g->p[i].x, g->p[i].y);
    g->tray_right = tray_right;
    layout_table(g);
    double dx = g->fx - fx, dy = g->fy - fy;
    for (int i = 0; i < TGM_PIECES; i++) {
        g->slot[i].x += dx;
        g->slot[i].y += dy;
        if (intray[i]) tray_place(g, i, &g->p[i].x, &g->p[i].y);
        else {
            g->p[i].x += dx;
            g->p[i].y += dy;
        }
    }
    for (int k = 0; k < g->nh; k++) {
        g->hist[k].was.x += dx;
        g->hist[k].was.y += dy;
    }
}

static int inside(const double *xy, int n, double px, double py)
{
    int c = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) c = !c;
    }
    return c;
}

int tgm_inside(const tgm_game *g, double x, double y)
{
    for (int i = 0; i < TGM_PIECES; i++) {
        double xy[8];
        int n = tgm_corners(i, &g->slot[i], xy);
        if (inside(xy, n, x, y)) return 1;
    }
    return 0;
}

/* ---- drawn to a fitting place -------------------------------------------------------------------------------- */
#define MAXT 1024

static int add_point(double *tx, double *ty, int n, double x, double y)
{
    if (n >= MAXT) return n;
    for (int k = 0; k < n; k++)
        if (fabs(tx[k] - x) < SAME && fabs(ty[k] - y) < SAME) return n;
    tx[n] = x;
    ty[n] = y;
    return n + 1;
}

/* a polygon's corners, and points along its edges at 1/2, 1, r2/2, r2, 2 from each end */
static int add_poly(double *tx, double *ty, int n, const double *xy, int m)
{
    static const double D[5] = {0.5, 1, R2 / 2, R2, 2};
    for (int i = 0; i < m; i++) {
        int j = (i + 1) % m;
        double ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * j], by = xy[2 * j + 1];
        double L = hypot(bx - ax, by - ay), ux = (bx - ax) / L, uy = (by - ay) / L;
        n = add_point(tx, ty, n, ax, ay);
        for (int d = 0; d < 5; d++)
            if (D[d] < L - 0.01) {
                n = add_point(tx, ty, n, ax + ux * D[d], ay + uy * D[d]);
                n = add_point(tx, ty, n, bx - ux * D[d], by - uy * D[d]);
            }
    }
    return n;
}

int tgm_snap(const tgm_game *g, int piece, tgm_place *pl)
{
    static double tx[MAXT], ty[MAXT];
    int nt = 0;
    double xy[8], c[8];
    for (int i = 0; i < TGM_PIECES; i++) {
        int m = tgm_corners(i, &g->slot[i], c);
        nt = add_poly(tx, ty, nt, c, m);
        if (i == piece) continue;
        m = tgm_corners(i, &g->p[i], c);
        nt = add_poly(tx, ty, nt, c, m);
    }
    int n = tgm_corners(piece, pl, xy), best_m = -1;
    double best_d = 1e9, bdx = 0, bdy = 0;
    for (int v = 0; v < n; v++)
        for (int k = 0; k < nt; k++) {
            double dx = tx[k] - xy[2 * v], dy = ty[k] - xy[2 * v + 1], d = hypot(dx, dy);
            if (d > SNAP) continue;
            int m = 0;
            for (int w = 0; w < n; w++)
                for (int k2 = 0; k2 < nt; k2++)
                    if (fabs(tx[k2] - xy[2 * w] - dx) < 1e-4 && fabs(ty[k2] - xy[2 * w + 1] - dy) < 1e-4) {
                        m++;
                        break;
                    }
            if (m > best_m || (m == best_m && d < best_d)) {
                best_m = m;
                best_d = d;
                bdx = dx;
                bdy = dy;
            }
        }
    if (best_m < 0)                             /* nothing near: a corner onto the grid of half units */
        for (int v = 0; v < n; v++) {
            double gx = g->fx + round((xy[2 * v] - g->fx) * 2) / 2, gy = g->fy + round((xy[2 * v + 1] - g->fy) * 2) / 2;
            double d = hypot(gx - xy[2 * v], gy - xy[2 * v + 1]);
            if (d < best_d) {
                best_d = d;
                bdx = gx - xy[2 * v];
                bdy = gy - xy[2 * v + 1];
            }
        }
    pl->x += bdx;
    pl->y += bdy;
    return best_d > 1e-9;
}

/* ---- the actions ----------------------------------------------------------------------------------------------- */
static void remember(tgm_game *g, int piece)
{
    if (g->nh >= TGM_HIST) {                    /* forget the oldest half */
        memmove(g->hist, g->hist + TGM_HIST / 2, sizeof g->hist / 2);
        g->nh -= TGM_HIST / 2;
    }
    g->hist[g->nh].piece = (int8_t)piece;
    g->hist[g->nh++].was = g->p[piece];
    g->moves++;
}

static void settle(tgm_game *g, int piece)
{
    tgm_place *pl = &g->p[piece];
    pl->x = fmin(fmax(pl->x, 0.4), g->table_w - 0.4);
    pl->y = fmin(fmax(pl->y, 0.4), g->table_h - 0.4);
    tgm_snap(g, piece, pl);
    g->solved = tgm_check(g) <= TGM_RES * TGM_RES / 16;
}

int tgm_move(tgm_game *g, int piece, double x, double y)
{
    if (piece < 0 || piece >= TGM_PIECES) return 0;
    remember(g, piece);
    g->p[piece].x = x;
    g->p[piece].y = y;
    settle(g, piece);
    return 1;
}

int tgm_turn(tgm_game *g, int piece)
{
    if (piece < 0 || piece >= TGM_PIECES) return 0;
    remember(g, piece);
    g->p[piece].rot = (g->p[piece].rot + 1) & 7;
    settle(g, piece);
    return 1;
}

int tgm_flip(tgm_game *g, int piece)
{
    if (piece != 6) return 0;
    remember(g, piece);
    g->p[piece].flip ^= 1;
    settle(g, piece);
    return 1;
}

int tgm_undo(tgm_game *g)
{
    if (!g->nh) return -1;
    g->nh--;
    g->p[g->hist[g->nh].piece] = g->hist[g->nh].was;
    g->moves--;
    g->solved = tgm_check(g) <= TGM_RES * TGM_RES / 16;
    return g->hist[g->nh].piece;
}

/* ---- the check ------------------------------------------------------------------------------------------------- */
int tgm_check(const tgm_game *g)
{
    double pc[TGM_PIECES][8], sc[TGM_PIECES][8], box[TGM_PIECES][4];
    int pn[TGM_PIECES], sn[TGM_PIECES];
    double x0 = g->fx - 0.25, y0 = g->fy - 0.25, x1 = g->fx + g->fw + 0.25, y1 = g->fy + g->fh + 0.25;
    int far = 0;
    for (int i = 0; i < TGM_PIECES; i++) {
        pn[i] = tgm_corners(i, &g->p[i], pc[i]);
        sn[i] = tgm_corners(i, &g->slot[i], sc[i]);
        box[i][0] = box[i][1] = 1e9;
        box[i][2] = box[i][3] = -1e9;
        for (int k = 0; k < pn[i]; k++) {
            box[i][0] = fmin(box[i][0], pc[i][2 * k]);
            box[i][1] = fmin(box[i][1], pc[i][2 * k + 1]);
            box[i][2] = fmax(box[i][2], pc[i][2 * k]);
            box[i][3] = fmax(box[i][3], pc[i][2 * k + 1]);
        }
        if (box[i][0] < x0 || box[i][1] < y0 || box[i][2] > x1 || box[i][3] > y1) far = 1;
    }
    int nx = (int)ceil((x1 - x0) * TGM_RES), ny = (int)ceil((y1 - y0) * TGM_RES), bad = 0;
    if (far) return nx * ny + 1;
    for (int y = 0; y < ny; y++) {
        double py = y0 + (y + 0.5) / TGM_RES + 0.00317;   /* (off the edges' lines) */
        for (int x = 0; x < nx; x++) {
            double px = x0 + (x + 0.5) / TGM_RES + 0.00731;
            int t = 0, c = 0;
            for (int i = 0; i < TGM_PIECES && !t; i++) t = inside(sc[i], sn[i], px, py);
            for (int i = 0; i < TGM_PIECES; i++)
                if (px >= box[i][0] && px <= box[i][2] && py >= box[i][1] && py <= box[i][3])
                    c += inside(pc[i], pn[i], px, py);
            bad += t ? c != 1 : c > 0;
        }
    }
    return bad;
}

int tgm_solved(const tgm_game *g) { return g->solved; }

/* ---- hints ----------------------------------------------------------------------------------------------------- */
static int same_place(int piece, const tgm_place *a, int slot, const tgm_place *b)
{
    double xa[8], xb[8];
    int n = tgm_corners(piece, a, xa), m = tgm_corners(slot, b, xb);
    if (n != m) return 0;
    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < m && !found; j++)
            found = fabs(xa[2 * i] - xb[2 * j]) < 1e-3 && fabs(xa[2 * i + 1] - xb[2 * j + 1]) < 1e-3;
        if (!found) return 0;
    }
    return 1;
}

/* the slot of the made solution a piece covers exactly (of its kind); -1 */
static int slot_of(const tgm_game *g, int piece)
{
    for (int j = 0; j < TGM_PIECES; j++)
        if (TYPE[j] == TYPE[piece] && same_place(piece, &g->p[piece], j, &g->slot[j])) return j;
    return -1;
}

int tgm_in_place(const tgm_game *g, int piece) { return slot_of(g, piece) >= 0; }

int tgm_hint(tgm_game *g)
{
    if (g->solved) return -1;
    int taken[TGM_PIECES] = {0}, placed[TGM_PIECES] = {0};
    for (int i = 0; i < TGM_PIECES; i++) {      /* the slots already filled, and by which pieces */
        int j = slot_of(g, i);
        if (j < 0 || taken[j]) continue;
        taken[j] = 1;
        placed[i] = 1;
    }
    for (int j = 0; j < TGM_PIECES; j++) {
        if (taken[j]) continue;
        for (int i = 0; i < TGM_PIECES; i++) {
            if (placed[i] || TYPE[i] != TYPE[j]) continue;
            remember(g, i);
            g->p[i] = g->slot[j];
            g->hints++;
            g->solved = tgm_check(g) <= TGM_RES * TGM_RES / 16;
            return i;
        }
    }
    return -1;
}

int tgm_stars(const tgm_game *g) { return g->hints == 0 ? 3 : g->hints <= 2 ? 2 : 1; }

int tgm_script(tgm_game *g, const char *s)
{
    while (*s) {
        char k = *s;
        if (k == ' ' || k == ',') { s++; continue; }
        s++;
        if (k == 'h' || k == '=') {
            do {
                if (tgm_hint(g) < 0) break;
            } while (k == '=' && !g->solved);
            continue;
        }
        char *e;
        long n = strtol(s, &e, 10);
        if (e == s) return 0;
        s = e;
        if (k == 'n') {
            if (n < 1 || n > TGM_LEVELS) return 0;
            int right = g->tray_right;
            tgm_new(g, (int)n - 1, g->seed);
            tgm_arrange(g, right);
        } else if (k == 'r') {
            if (!tgm_turn(g, (int)n)) return 0;
        } else if (k == 'f') {
            if (!tgm_flip(g, (int)n)) return 0;
        } else if (k == 'm') {
            if (n < 0 || n >= TGM_PIECES || *s != ':') return 0;
            double x = strtod(s + 1, &e);
            if (*e != ',') return 0;
            double y = strtod(e + 1, &e);
            s = e;
            tgm_move(g, (int)n, g->fx + x, g->fy + y);
        } else {
            return 0;
        }
    }
    return 1;
}

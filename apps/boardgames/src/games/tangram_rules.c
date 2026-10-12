/*
 * Tangram: the rules (tangram.h): the tans' shapes turned and turned over, the silhouettes read from their tans, the
 * grid they are drawn to, the quarter cells they cover, the check, hints and undo. No SDL: tested by
 * tests/tangram_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tangram.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/tangram_figures.h"

static const int TYPE[TGM_PIECES] = {TGM_L, TGM_L, TGM_M, TGM_S, TGM_S, TGM_Q, TGM_P};
static const float BASE[5][8] = {{0, 0, 2, 0, 0, 2}, {0, 0, 2, 0, 1, 1}, {0, 0, 1, 0, 0, 1}, {0, 0, 1, 0, 1, 1, 0, 1},
                                 {0, 0, 1, 0, 2, 1, 1, 1}};
static const int NV[5] = {3, 3, 3, 4, 4};

int tgm_type(int piece) { return TYPE[piece < 0 || piece >= TGM_PIECES ? 0 : piece]; }

/* a shape turned over (x mirrored) then turned rot eighths clockwise, its middle at the origin: its corners */
static int shape(int type, int rot, int flip, float *xy)
{
    static const float C[8] = {1, 0.70710678f, 0, -0.70710678f, -1, -0.70710678f, 0, 0.70710678f};
    float c = C[rot & 7], s = C[(rot + 6) & 7], mx = 0, my = 0;   /* s = sin: cos shifted a quarter turn back */
    int n = NV[type];
    for (int i = 0; i < n; i++) {
        float x = BASE[type][2 * i], y = BASE[type][2 * i + 1];
        if (flip) x = -x;
        xy[2 * i] = x * c - y * s;
        xy[2 * i + 1] = x * s + y * c;
        mx += xy[2 * i];
        my += xy[2 * i + 1];
    }
    for (int i = 0; i < n; i++) {
        xy[2 * i] -= mx / n;
        xy[2 * i + 1] -= my / n;
    }
    return n;
}

int tgm_corners(int piece, const tgm_place *pl, float *xy)
{
    int n = shape(tgm_type(piece), pl->rot, pl->flip, xy);
    for (int i = 0; i < n; i++) {
        xy[2 * i] += pl->x;
        xy[2 * i + 1] += pl->y;
    }
    return n;
}

int tgm_parse(const char *s, tgm_place *out)
{
    int seen = 0, n_l = 0, n_s = 0;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        const char *k = strchr("LMSQP", *s);
        if (!k) return 0;
        int type = (int)(k - "LMSQP"), flip = 0, rot = 0;
        s++;
        if (*s == 'f') { flip = 1; s++; }
        if (*s >= '0' && *s <= '3') rot = 2 * (*s++ - '0');
        if (*s++ != ':') return 0;
        char *e;
        long x = strtol(s, &e, 10);
        if (*e != ',') return 0;
        long y = strtol(e + 1, &e, 10);
        s = e;
        int piece = type == TGM_L ? n_l++ : type == TGM_M ? 2 : type == TGM_S ? 3 + n_s++ : type == TGM_Q ? 5 : 6;
        if (n_l > 2 || n_s > 2 || (seen >> piece & 1)) return 0;
        seen |= 1 << piece;
        float xy[8], mx = 1e9f, my = 1e9f;
        int n = shape(type, rot, flip, xy);
        for (int i = 0; i < n; i++) {
            mx = fminf(mx, xy[2 * i]);
            my = fminf(my, xy[2 * i + 1]);
        }
        out[piece] = (tgm_place){(float)x - mx, (float)y - my, rot, flip};
    }
    return seen == (1 << TGM_PIECES) - 1;
}

static int inside(const float *xy, int n, float px, float py)
{
    int c = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) c = !c;
    }
    return c;
}

/* the quarters of a piece on a grid whose corner is at (ox, 0) */
static int quarters_at(int piece, const tgm_place *pl, float ox, int *qx, int *qy, int *qq)
{
    if (pl->rot & 1) return 0;
    float xy[8];
    int n = tgm_corners(piece, pl, xy), x0 = 1 << 20, y0 = 1 << 20, x1 = -(1 << 20), y1 = -(1 << 20);
    for (int i = 0; i < n; i++) {
        float x = xy[2 * i] - ox, y = xy[2 * i + 1];
        if (fabsf(x - roundf(x)) > 0.02f || fabsf(y - roundf(y)) > 0.02f) return 0;   /* off the grid */
        xy[2 * i] = roundf(x);
        xy[2 * i + 1] = roundf(y);
        x0 = (int)fminf((float)x0, xy[2 * i]);
        y0 = (int)fminf((float)y0, xy[2 * i + 1]);
        x1 = (int)fmaxf((float)x1, xy[2 * i]);
        y1 = (int)fmaxf((float)y1, xy[2 * i + 1]);
    }
    static const float CX[4] = {0.5f, 5 / 6.0f, 0.5f, 1 / 6.0f}, CY[4] = {1 / 6.0f, 0.5f, 5 / 6.0f, 0.5f};
    int k = 0;
    for (int cy = y0; cy < y1; cy++)
        for (int cx = x0; cx < x1; cx++)
            for (int q = 0; q < 4; q++)
                if (inside(xy, n, cx + CX[q], cy + CY[q]) && k < 8) {
                    qx[k] = cx;
                    qy[k] = cy;
                    qq[k++] = q;
                }
    return k;
}

int tgm_quarters(const tgm_game *g, int piece, const tgm_place *pl, int *qx, int *qy, int *qq)
{
    return quarters_at(piece, pl, g->fx, qx, qy, qq);
}

void tgm_new(tgm_game *g, int set, int index, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (set < 0 || set >= TGM_SETS) set = 0;
    if (index < 0 || index >= TGM_PER_SET) index = 0;
    g->figure = set * TGM_PER_SET + index;
    g->seed = seed;
    tgm_place sl[TGM_PIECES];
    if (!tgm_parse(tgm_figures[g->figure], sl)) return;
    /* the silhouette's quarters, its corner moved to the origin */
    int qx[TGM_PIECES][8], qy[TGM_PIECES][8], qq[TGM_PIECES][8], nq[TGM_PIECES], mx = 99, my = 99, Mx = -99, My = -99;
    for (int i = 0; i < TGM_PIECES; i++) {
        nq[i] = quarters_at(i, &sl[i], 0, qx[i], qy[i], qq[i]);
        for (int k = 0; k < nq[i]; k++) {
            mx = qx[i][k] < mx ? qx[i][k] : mx;
            my = qy[i][k] < my ? qy[i][k] : my;
            Mx = qx[i][k] > Mx ? qx[i][k] : Mx;
            My = qy[i][k] > My ? qy[i][k] : My;
        }
    }
    g->tw = Mx - mx + 1;
    g->th = My - my + 1;
    if (g->tw > TGM_MAXC || g->th > TGM_MAXC) return;
    for (int i = 0; i < TGM_PIECES; i++)
        for (int k = 0; k < nq[i]; k++) g->target[qy[i][k] - my][qx[i][k] - mx] |= (uint8_t)(1 << qq[i][k]);
    g->table_w = (float)(g->tw > 8 ? g->tw : 8) + 1;
    g->fx = floorf((g->table_w - g->tw) / 2);
    g->table_h = g->th + 5.6f;
    for (int i = 0; i < TGM_PIECES; i++) {
        g->slot[i] = sl[i];
        g->slot[i].x += g->fx - mx;
        g->slot[i].y -= my;
    }
    /* the tray: two rows under the silhouette, each tan turned at random by quarter turns */
    static const float TX[TGM_PIECES] = {1.3f, 3.7f, 6.3f, 1.0f, 2.6f, 4.1f, 6.0f},
                       TY[TGM_PIECES] = {2.25f, 2.25f, 2.25f, 4.4f, 4.4f, 4.4f, 4.4f};
    uint32_t s = pr_seed(seed);
    float off = (g->table_w - 8) / 2;
    for (int i = 0; i < TGM_PIECES; i++) {
        g->p[i].x = off + TX[i];
        g->p[i].y = g->th + TY[i];
        g->p[i].rot = 2 * pr_below(&s, 4);
        g->p[i].flip = i == 6 ? pr_below(&s, 2) : 0;
    }
}


int tgm_snap(const tgm_game *g, int piece, tgm_place *pl)
{
    if (pl->rot & 1) return 0;
    float xy[8];
    tgm_corners(piece, pl, xy);
    float dx = roundf(xy[0] - g->fx) + g->fx - xy[0], dy = roundf(xy[1]) - xy[1];   /* the first corner to the grid */
    pl->x += dx;
    pl->y += dy;
    return fabsf(dx) > 1e-4f || fabsf(dy) > 1e-4f;
}

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

int tgm_move(tgm_game *g, int piece, float x, float y)
{
    if (piece < 0 || piece >= TGM_PIECES) return 0;
    remember(g, piece);
    tgm_place *pl = &g->p[piece];
    pl->x = fminf(fmaxf(x, 0.4f), g->table_w - 0.4f);
    pl->y = fminf(fmaxf(y, 0.4f), g->table_h - 0.4f);
    tgm_snap(g, piece, pl);
    return 1;
}

int tgm_turn(tgm_game *g, int piece)
{
    if (piece < 0 || piece >= TGM_PIECES) return 0;
    remember(g, piece);
    g->p[piece].rot = (g->p[piece].rot + 1) & 7;
    tgm_snap(g, piece, &g->p[piece]);
    return 1;
}

int tgm_flip(tgm_game *g, int piece)
{
    if (piece != 6) return 0;
    remember(g, piece);
    g->p[piece].flip ^= 1;
    tgm_snap(g, piece, &g->p[piece]);
    return 1;
}

int tgm_undo(tgm_game *g)
{
    if (!g->nh) return -1;
    g->nh--;
    g->p[g->hist[g->nh].piece] = g->hist[g->nh].was;
    g->moves--;
    return g->hist[g->nh].piece;
}

/* two sets of quarters alike */
static int same(int n, const int *ax, const int *ay, const int *aq, int m, const int *bx, const int *by, const int *bq)
{
    if (n != m || !n) return 0;
    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < m && !found; j++) found = ax[i] == bx[j] && ay[i] == by[j] && aq[i] == bq[j];
        if (!found) return 0;
    }
    return 1;
}

/* the slot of the made solution piece covers exactly (of its kind); -1 */
static int slot_of(const tgm_game *g, int piece)
{
    int ax[8], ay[8], aq[8], bx[8], by[8], bq[8];
    int n = tgm_quarters(g, piece, &g->p[piece], ax, ay, aq);
    if (!n) return -1;
    for (int j = 0; j < TGM_PIECES; j++) {
        if (TYPE[j] != TYPE[piece]) continue;
        int m = tgm_quarters(g, j, &g->slot[j], bx, by, bq);
        if (same(n, ax, ay, aq, m, bx, by, bq)) return j;
    }
    return -1;
}

int tgm_in_place(const tgm_game *g, int piece) { return slot_of(g, piece) >= 0; }

int tgm_solved(const tgm_game *g)
{
    uint8_t cover[TGM_MAXC][TGM_MAXC] = {{0}};
    int total = 0;
    for (int i = 0; i < TGM_PIECES; i++) {
        int qx[8], qy[8], qq[8], n = tgm_quarters(g, i, &g->p[i], qx, qy, qq);
        if (!n) return 0;
        for (int k = 0; k < n; k++) {
            if (qx[k] < 0 || qy[k] < 0 || qx[k] >= g->tw || qy[k] >= g->th) return 0;
            if (!(g->target[qy[k]][qx[k]] >> qq[k] & 1) || (cover[qy[k]][qx[k]] >> qq[k] & 1)) return 0;
            cover[qy[k]][qx[k]] |= (uint8_t)(1 << qq[k]);
            total++;
        }
    }
    return total == 32;
}

int tgm_hint(tgm_game *g)
{
    if (tgm_solved(g)) return -1;
    int taken[TGM_PIECES] = {0}, placed[TGM_PIECES] = {0};
    for (int i = 0; i < TGM_PIECES; i++) {      /* the slots already filled, and by which pieces */
        int j = slot_of(g, i);
        if (j < 0 || taken[j]) continue;        /* (a second piece on a filled place: not placed) */
        taken[j] = 1;
        placed[i] = 1;
    }
    for (int j = 0; j < TGM_PIECES; j++) {
        if (taken[j]) continue;
        for (int i = 0; i < TGM_PIECES; i++) {
            if (placed[i] || TYPE[i] != TYPE[j]) continue;
            remember(g, i);
            g->p[i] = g->slot[j];
            return i;
        }
    }
    return -1;
}

int tgm_script(tgm_game *g, const char *s)
{
    while (*s) {
        char k = *s;
        if (k == ' ' || k == ',') { s++; continue; }
        s++;
        if (k == 'h' || k == '=') {
            do {
                if (tgm_hint(g) < 0) break;
            } while (k == '=' && !tgm_solved(g));
            continue;
        }
        char *e;
        long n = strtol(s, &e, 10);
        if (e == s) return 0;
        s = e;
        if (k == 's') {
            if (n < 1 || n > TGM_PER_SET) return 0;
            tgm_new(g, g->figure / TGM_PER_SET, (int)n - 1, g->seed);
        } else if (k == 'r') {
            if (!tgm_turn(g, (int)n)) return 0;
        } else if (k == 'f') {
            if (!tgm_flip(g, (int)n)) return 0;
        } else if (k == 'm') {
            if (n < 0 || n >= TGM_PIECES || *s != ':') return 0;
            long x = strtol(s + 1, &e, 10);
            if (*e != ',') return 0;
            long y = strtol(e + 1, &e, 10);
            s = e;
            float xy[8];
            tgm_corners((int)n, &g->p[n], xy);  /* the first corner onto (x, y) of the silhouette's grid */
            if (!tgm_move(g, (int)n, g->p[n].x + (x + g->fx - xy[0]), g->p[n].y + (y - xy[1]))) return 0;
        } else {
            return 0;
        }
    }
    return 1;
}

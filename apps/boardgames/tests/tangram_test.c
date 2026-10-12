/*
 * Tangram's rules: every silhouette checked by construction (its seven tans read, each on the grid, none overlapping,
 * 32 quarter cells in all, held together edge to edge, no tan touching the others only at a corner), all different;
 * the start (the tans in the tray, the silhouette empty), the hints (each puts one tan in a place; seven at most solve
 * it), the two large triangles swapped (still solved), turns (eight make a whole turn, the grid pulling), the
 * parallelogram turned over, the grid's pull, undo, and whole games from a seed (random moves, then hints to the end).
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

/* the quarters next to a quarter, across its edges */
static void beside(int x, int y, int q, int *nx, int *ny, int *nq)
{
    nx[0] = nx[1] = x;
    ny[0] = ny[1] = y;
    nq[0] = (q + 1) & 3;
    nq[1] = (q + 3) & 3;
    nx[2] = x + (q == 1) - (q == 3);
    ny[2] = y + (q == 2) - (q == 0);
    nq[2] = (q + 2) & 3;
}

int main(void)
{
    int bad_figures = 0;
    for (int f = 0; f < TGM_FIGURES; f++) {
        tgm_game g;
        tgm_new(&g, f / TGM_PER_SET, f % TGM_PER_SET, 1);
        int owner[TGM_MAXC][TGM_MAXC][4], ok = g.tw > 0 && g.tw <= 10 && g.th > 0 && g.th <= 10, total = 0;
        memset(owner, -1, sizeof owner);
        for (int i = 0; i < TGM_PIECES && ok; i++) {
            int qx[8], qy[8], qq[8], n = tgm_quarters(&g, i, &g.slot[i], qx, qy, qq);
            static const int AREA[5] = {8, 4, 2, 4, 4};
            if (n != AREA[tgm_type(i)]) ok = 0;
            for (int k = 0; k < n && ok; k++) {
                if (qx[k] < 0 || qy[k] < 0 || qx[k] >= g.tw || qy[k] >= g.th) { ok = 0; break; }
                if (owner[qy[k]][qx[k]][qq[k]] >= 0) ok = 0;   /* two tans on one quarter */
                owner[qy[k]][qx[k]][qq[k]] = i;
                if (!(g.target[qy[k]][qx[k]] >> qq[k] & 1)) ok = 0;
                total++;
            }
        }
        if (total != 32) ok = 0;
        /* each tan touches another along an edge, and all hold together */
        int touch[TGM_PIECES][TGM_PIECES] = {{0}};
        for (int y = 0; y < g.th && ok; y++)
            for (int x = 0; x < g.tw; x++)
                for (int q = 0; q < 4; q++) {
                    int a = owner[y][x][q], nx[3], ny[3], nq[3];
                    if (a < 0) continue;
                    beside(x, y, q, nx, ny, nq);
                    for (int k = 0; k < 3; k++) {
                        if (nx[k] < 0 || ny[k] < 0 || nx[k] >= g.tw || ny[k] >= g.th) continue;
                        int b = owner[ny[k]][nx[k]][nq[k]];
                        if (b >= 0 && b != a) touch[a][b] = touch[b][a] = 1;
                    }
                }
        int reached = 1, stack[TGM_PIECES], sp = 0, seen = 1;
        stack[sp++] = 0;
        while (sp) {
            int a = stack[--sp];
            for (int b = 0; b < TGM_PIECES; b++)
                if (touch[a][b] && !(seen >> b & 1)) { seen |= 1 << b; reached++; stack[sp++] = b; }
        }
        if (reached != TGM_PIECES) ok = 0;
        for (int k = 0; k < f; k++)
            if (!strcmp(tgm_figures[k], tgm_figures[f])) ok = 0;
        if (!ok) { printf("figure %d is not well made\n", f); bad_figures++; }
        /* the start and the hints */
        CHECK(!tgm_solved(&g));
        int hints = 0;
        while (!tgm_solved(&g) && hints < 10 && tgm_hint(&g) >= 0) hints++;
        CHECK(tgm_solved(&g) && hints <= TGM_PIECES && g.moves == hints);
    }
    CHECK(bad_figures == 0);
    printf("tangram: %d silhouettes checked\n", TGM_FIGURES);

    /* the two large triangles swapped: still solved; a tan off its place: not */
    tgm_game g;
    tgm_new(&g, 0, 0, 3);
    CHECK(tgm_script(&g, "="));
    tgm_place a = g.p[0];
    g.p[0] = g.p[1];
    g.p[1] = a;
    CHECK(tgm_solved(&g) && tgm_in_place(&g, 0) && tgm_in_place(&g, 1));
    CHECK(tgm_move(&g, 5, g.p[5].x + 1, g.p[5].y) && !tgm_solved(&g) && !tgm_in_place(&g, 5));
    CHECK(tgm_undo(&g) == 5 && tgm_solved(&g));

    /* turns: eight make a whole turn; odd ones leave the grid; the parallelogram turns over, the others do not */
    tgm_new(&g, 1, 3, 5);
    tgm_place p0 = g.p[2];
    for (int i = 0; i < 8; i++) CHECK(tgm_turn(&g, 2));
    CHECK(g.p[2].rot == p0.rot && fabsf(g.p[2].x - p0.x) < 1 && fabsf(g.p[2].y - p0.y) < 1);   /* (the grid pulls) */
    int qx[8], qy[8], qq[8];
    CHECK(tgm_turn(&g, 2) && tgm_quarters(&g, 2, &g.p[2], qx, qy, qq) == 0);
    CHECK(tgm_turn(&g, 2) && tgm_quarters(&g, 2, &g.p[2], qx, qy, qq) == 4);
    CHECK(!tgm_flip(&g, 0) && tgm_flip(&g, 6) && tgm_flip(&g, 6));
    /* the grid's pull: a tan let go near a grid point lands on it */
    CHECK(tgm_move(&g, 5, g.p[5].x + 0.3f, g.p[5].y - 0.2f));
    float xy[8];
    tgm_corners(5, &g.p[5], xy);
    CHECK(fabsf(xy[0] - roundf(xy[0])) < 1e-3f && fabsf(xy[1] - roundf(xy[1])) < 1e-3f);
    CHECK(tgm_script(&g, "m5:2,2"));
    tgm_corners(5, &g.p[5], xy);
    CHECK(fabsf(xy[0] - g.fx - 2) < 1e-3f && fabsf(xy[1] - 2) < 1e-3f);
    while (tgm_undo(&g) >= 0) {}
    tgm_game fresh;
    tgm_new(&fresh, 1, 3, 5);
    CHECK(!memcmp(fresh.p, g.p, sizeof g.p) && g.moves == 0);
    CHECK(!tgm_script(&g, "s0") && !tgm_script(&g, "s21") && !tgm_script(&g, "x1") && tgm_script(&g, "s20"));
    CHECK(g.figure == 1 * TGM_PER_SET + 19);

    /* whole games: random moves, turns and turnings over, then the hints to the end */
    int whole = 1;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        uint32_t r = pr_seed(seed);
        tgm_new(&g, (int)(seed % TGM_SETS), (int)(seed / 3 % TGM_PER_SET), seed);
        int n = pr_below(&r, 25);
        for (int i = 0; i < n; i++) {
            int piece = pr_below(&r, TGM_PIECES), what = pr_below(&r, 3);
            if (what == 0) tgm_move(&g, piece, pr_below(&r, 100) / 10.0f, pr_below(&r, 140) / 10.0f);
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

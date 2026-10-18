/*
 * Ring and Fork's rules test (src/games/havannah_rules.c, no SDL): the board (its cells, corners, sides, the
 * notation), the three shapes (a ring round an empty cell, round the other's stone, round one's own; no ring round a
 * rim cell, nor a triangle, a rhombus or a horseshoe open to the rim; a bridge along a side; a fork, and two sides
 * with a corner that is not one), the swap, undo, the winner's shape lit, then random games (each ends at its first
 * shape or in a draw on a full board).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/havannah.h"

static hvgame G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int C(int q, int r) { return (r + G.n - 1) * HV_W + q + G.n - 1; }   /* a cell from the middle */

static void board(int n) { hv_new(&G, n, 0); }

static void put(int p, int q, int r) { G.cell[C(q, r)] = (uint8_t)(p + 1); }

static int shapes(int p) { return hv_shapes(G.n, G.cell, p, -1); }

static void fresh(int n, int swap, const char *moves)
{
    hv_new(&G, n, swap);
    if (!hv_script(&G, moves)) printf("bad script: %s\n", moves);
}

/* the six cells round (q, r) */
static void flower(int p, int q, int r)
{
    for (int i = 0; i < 6; i++) put(p, q + HV_DX[i], r + HV_DY[i]);
}

int main(void)
{
    /* ---- the board -------------------------------------------------------------------------------------------- */
    for (int n = 6; n <= 8; n += 2) {
        int cells = 0, corners = 0, sides[6] = {0}, nb[6];
        for (int i = 0; i < HV_SQ; i++) {
            if (!hv_valid(n, i)) continue;
            cells++;
            int c = hv_corner(n, i), s = hv_side(n, i), k = hv_around(n, i, nb);
            if (c >= 0) { corners++; CHECK(k == 3 && s < 0); }
            if (s >= 0) { sides[s]++; CHECK(k == 4); }
            if (c < 0 && s < 0) CHECK(k == 6);
            char name[8];
            int len;
            hv_name(i, name, sizeof name);
            CHECK(hv_cell(n, name, &len) == i && len == (int)strlen(name));
        }
        CHECK(cells == hv_count(n) && corners == 6);
        for (int s = 0; s < 6; s++) CHECK(sides[s] == n - 2);
    }
    {
        int len;
        CHECK(hv_cell(6, "f1", &len) == 5 && hv_corner(6, 5) == 0);          /* the top left corner of 6 */
        CHECK(hv_cell(6, "a6", &len) == 5 * HV_W && hv_corner(6, 5 * HV_W) == 5);
        CHECK(hv_cell(6, "a1", &len) == HV_NONE && hv_cell(6, "l3", &len) == HV_NONE);
        CHECK(hv_cell(8, "o8", &len) == 7 * HV_W + 14 && hv_cell(6, "swap", &len) == HV_SWAP);
    }

    /* ---- rings ------------------------------------------------------------------------------------------------ */
    board(8);
    flower(0, 0, 0);
    CHECK(shapes(0) == HV_RING && shapes(1) == 0);             /* round the empty middle */
    put(1, 0, 0);
    CHECK(shapes(0) == HV_RING);                               /* round the other's stone */
    put(0, 0, 0);
    CHECK(shapes(0) == HV_RING);                               /* a patch of seven: round one's own */
    board(8);
    flower(0, 0, 0);
    G.cell[C(1, 0)] = 0;
    CHECK(shapes(0) == 0);                                     /* five of six: open */
    put(1, 1, 0);
    CHECK(shapes(0) == 0);                                     /* the gap taken by the other: still open */
    board(8);                                                  /* a triangle and a rhombus enclose no cell */
    put(0, 0, 0), put(0, 1, 0), put(0, 0, 1), put(0, 1, -1);
    CHECK(shapes(0) == 0);
    board(6);
    put(0, 2, -5), put(0, 1, -4), put(0, 0, -4), put(0, 0, -5);   /* (1, -5) on the rim, its four on-board neighbours */
    CHECK(shapes(0) == 0);
    board(6);                                                  /* a cell one in from the rim: a real ring */
    flower(0, 1, -4);
    CHECK(shapes(0) == HV_RING);
    board(8);                                                  /* a horseshoe open to the rim, then closed far in */
    for (int r = -7; r <= -3; r++) put(0, 2, r), put(0, 5, r);
    for (int q = 2; q <= 5; q++) put(0, q, -3);
    CHECK(shapes(0) == 0);
    put(0, 3, -6), put(0, 4, -6);
    CHECK(shapes(0) == HV_RING);
    board(8);                                                  /* a ring of twelve round seven cells */
    for (int i = 0; i < 6; i++)
        for (int k = 0; k < 2; k++) {
            int a = (i + 2) % 6;
            put(0, 2 * HV_DX[i] + k * HV_DX[a], 2 * HV_DY[i] + k * HV_DY[a]);
        }
    CHECK(shapes(0) == HV_RING);
    put(0, 0, 0), put(1, 1, 0);                                /* stones of both inside it: still a ring */
    CHECK(shapes(0) == HV_RING);

    /* ---- bridges and forks ------------------------------------------------------------------------------------ */
    board(6);
    for (int q = 0; q <= 5; q++) put(0, q, -5);                /* the top side from corner to corner */
    CHECK(shapes(0) == HV_BRIDGE);
    G.cell[C(5, -5)] = 0;
    CHECK(shapes(0) == 0);                                     /* one corner and one side */
    board(6);                                                  /* a Y from the middle: the top and upper right sides */
    for (int k = 0; k <= 4; k++) put(0, 0, -k);
    put(0, 1, -5);                                             /* the top side, off its corner (0, -5) */
    for (int k = 1; k <= 4; k++) put(0, k, 0);
    put(0, 5, -1);                                             /* the upper right side */
    for (int k = 1; k <= 4; k++) put(0, -k, k);
    CHECK(shapes(0) == 0);
    put(0, -5, 5);                                             /* the lower left corner: two sides and a corner */
    CHECK(shapes(0) == 0);
    G.cell[C(-5, 5)] = 0, put(0, -4, 5);                       /* the bottom side, off the corner: a fork */
    CHECK(shapes(0) == HV_FORK);
    put(0, 1, -4), put(0, 0, -5);                              /* the top left corner too: still one corner */
    CHECK(shapes(0) == HV_FORK);

    /* ---- play, the swap, undo ------------------------------------------------------------------------------- */
    fresh(6, 0, "f6");
    CHECK(G.cell[C(0, 0)] == HV_IVORY && hv_turn(&G) == 1 && !hv_can_swap(&G));
    CHECK(!hv_script(&G, "swap") && !hv_script(&G, "f6"));
    fresh(6, 1, "f6 swap");
    CHECK(G.cell[C(0, 0)] == HV_EBONY && hv_turn(&G) == 0 && hv_last(&G) == C(0, 0));
    CHECK(!hv_script(&G, "swap"));
    CHECK(hv_undo(&G) && G.cell[C(0, 0)] == HV_IVORY && hv_turn(&G) == 1);
    {   /* ivory closes a ring (six round f6), ebony elsewhere; the last stone wins and the ring is lit */
        hv_new(&G, 6, 0);
        static const int RQ[6] = {1, 1, 0, -1, -1, 0}, RR[6] = {0, -1, -1, 0, 1, 1};
        char moves[200] = "", nm[8];
        for (int i = 0; i < 6; i++) {
            hv_name(C(RQ[i], RR[i]), nm, sizeof nm);
            strcat(moves, nm), strcat(moves, " ");
            if (i < 5) { hv_name(C(-4, 4 - i), nm, sizeof nm); strcat(moves, nm), strcat(moves, " "); }
        }
        CHECK(hv_script(&G, moves) && G.winner == 0 && G.how == HV_RING);
        int cells[HV_SQ], k = hv_shape_cells(&G, cells), ok = k == 6;
        for (int i = 0; i < k; i++) ok &= G.cell[cells[i]] == HV_IVORY && cells[i] != C(0, 0);
        CHECK(ok);
        CHECK(!hv_play(&G, C(2, 2)));                          /* the game is over */
        CHECK(hv_undo(&G) && G.winner < 0 && G.how == 0);
    }
    {   /* a bridge lit: its stones from corner to corner, not the stone hanging off it */
        hv_new(&G, 6, 0);
        CHECK(hv_script(&G, "f1 a6 g1 a7 h1 a8 i1 a9 j1 b10 f2 a10"));
        CHECK(G.winner < 0 && hv_script(&G, "k1") && G.winner == 0 && G.how == HV_BRIDGE);
        int cells[HV_SQ], k = hv_shape_cells(&G, cells), corners = 0, ok = k == 6;
        for (int i = 0; i < k; i++) corners += hv_corner(6, cells[i]) >= 0, ok &= cells[i] != C(0, -4);
        CHECK(ok && corners == 2);
    }

    /* ---- random games: each ends at its first shape (or a full board) -------------------------------------- */
    unsigned seed = 12345;
    int wins[3] = {0, 0, 0}, by[8] = {0};
    for (int game = 0; game < 400; game++) {
        int n = game & 1 ? 8 : 6;
        hv_new(&G, n, game % 3 == 0);
        int ok = 1;
        while (G.winner < 0 && ok) {
            int empty[HV_SQ], k = 0;
            for (int i = 0; i < HV_SQ; i++)
                if (hv_legal(&G, i)) empty[k++] = i;
            seed = seed * 1103515245u + 12345u;
            int mover = hv_turn(&G);
            if (hv_can_swap(&G) && (seed >> 16) % 2) { ok = hv_swap(&G); continue; }
            ok = k > 0 && hv_play(&G, empty[(seed >> 16) % (unsigned)k]);
            if (ok && G.winner < 0) ok = hv_shapes(n, G.cell, mover, -1) == 0 && hv_shapes(n, G.cell, !mover, -1) == 0;
        }
        CHECK(ok);
        wins[G.winner == HV_DRAW ? 2 : G.winner]++;
        if (G.winner != HV_DRAW) {
            by[G.how]++;
            CHECK(hv_shapes(n, G.cell, G.winner, -1) == G.how && hv_shape_cells(&G, NULL) > 0);
        } else {
            int stones = 0;
            for (int i = 0; i < HV_SQ; i++) stones += G.cell[i] != 0;
            CHECK(stones == hv_count(n));
        }
    }
    printf("random games: ivory %d, ebony %d, draws %d; rings %d, bridges %d, forks %d, several %d\n", wins[0], wins[1],
           wins[2], by[HV_RING], by[HV_BRIDGE], by[HV_FORK], by[3] + by[5] + by[6] + by[7]);
    CHECK(by[HV_RING] > 0 && by[HV_BRIDGE] > 0 && by[HV_FORK] > 0);
    printf("havannah: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}

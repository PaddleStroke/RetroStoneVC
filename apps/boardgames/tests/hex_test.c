/*
 * Hex's rules test (src/games/hex_rules.c, no SDL): the notation, the neighbours, the wins (straight and winding),
 * a chain on one side only, the swap, undo, random games (always a winner).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/hex.h"

static hexgame G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return hex_cell(G.n, name, &len);
}

static int stone(const char *name) { return G.cell[at(name)]; }

static void fresh(int n, const char *moves)
{
    hex_new(&G, n);
    if (!hex_script(&G, moves)) printf("bad script: %s\n", moves);
}

static int has(const int *nb, int k, int cell)
{
    for (int i = 0; i < k; i++)
        if (nb[i] == cell) return 1;
    return 0;
}

/* the winner's chain: from their first side to their last, each stone theirs and touching the one before */
static int chain_ok(void)
{
    int cells[HEX_CELLS], nb[6], n = G.n, k = hex_chain(&G, cells), p = G.winner;
    if (k <= 0 || p < 0) return 0;
    int a = p ? cells[0] % n : cells[0] / n, b = p ? cells[k - 1] % n : cells[k - 1] / n;
    if ((a != 0 || b != n - 1) && (a != n - 1 || b != 0)) return 0;
    for (int i = 0; i < k; i++) {
        if (G.cell[cells[i]] != p + 1) return 0;
        if (i && !has(nb, hex_around(n, cells[i], nb), cells[i - 1])) return 0;
    }
    return k;
}

/* a winding chain for ivory on 7 x 7 (down, right, up, right, down), its last stone c3 joining the two halves; ebony
 * plays elsewhere without joining its sides */
static const int WIND[15][2] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 1}, {4, 1}, {5, 1}, {5, 2},
                                {5, 3}, {5, 4}, {5, 5}, {5, 6}, {2, 2}};
static const int ELSE[14][2] = {{6, 0}, {6, 1}, {6, 2}, {6, 3}, {6, 4}, {6, 5}, {6, 6}, {4, 3}, {4, 4}, {4, 5},
                                {4, 6}, {3, 3}, {3, 4}, {3, 5}};

/* the winding game: ivory's chain, or (mirrored) the same chain for ebony after a first ivory stone on g1 */
static void winding(int mirrored, int stop_before_last)
{
    hex_new(&G, 7);
    if (mirrored) hex_play(&G, 6);
    for (int i = 0; i < 15 - stop_before_last; i++) {
        int c = WIND[i][0], r = WIND[i][1];
        if (!hex_play(&G, mirrored ? r + c * 7 : c + r * 7)) printf("winding: refused %d\n", i);
        if (i < 14) {
            c = ELSE[i][0];
            r = ELSE[i][1];
            if (!hex_play(&G, mirrored ? r + c * 7 : c + r * 7)) printf("winding: refused else %d\n", i);
        }
    }
}

int main(void)
{
    /* notation: columns from the left, rows from the top */
    hex_new(&G, 11);
    CHECK(at("a1") == 0 && at("k1") == 10 && at("a11") == 110 && at("k11") == 120 && at("c4") == 2 + 3 * 11);
    CHECK(at("l1") == HEX_NONE && at("a12") == HEX_NONE && at("a0") == HEX_NONE && at("a") == HEX_NONE);
    CHECK(at("swap") == HEX_SWAP);

    /* the neighbours: six in the middle, four on a side, two in the acute corners, three in the obtuse ones */
    int nb[6], k;
    hex_new(&G, 7);
    k = hex_around(7, at("d4"), nb);
    CHECK(k == 6 && has(nb, k, at("c4")) && has(nb, k, at("e4")) && has(nb, k, at("d3")) && has(nb, k, at("e3")) &&
          has(nb, k, at("c5")) && has(nb, k, at("d5")));
    CHECK(!has(nb, k, at("c3")) && !has(nb, k, at("e5")));
    CHECK(hex_around(7, at("a1"), nb) == 2 && hex_around(7, at("g7"), nb) == 2);
    CHECK(hex_around(7, at("g1"), nb) == 3 && hex_around(7, at("a7"), nb) == 3);
    CHECK(hex_around(7, at("d1"), nb) == 4 && hex_around(7, at("a4"), nb) == 4 && hex_around(7, at("g4"), nb) == 4);
    for (int c = 0; c < 49; c++) {               /* touching is mutual */
        int nb2[6];
        for (int m = hex_around(7, c, nb), j = 0; j < m; j++) CHECK(has(nb2, hex_around(7, nb[j], nb2), c));
    }

    /* straight wins: ivory down the a column, ebony along the fourth row */
    fresh(7, "a1 c1 a2 c2 a3 c3 a4 c4 a5 c5 a6 c6");
    CHECK(G.winner == -1 && hex_turn(&G) == 0);
    CHECK(hex_script(&G, "a7") && G.winner == 0 && chain_ok() == 7);
    CHECK(!hex_legal(&G, at("g7")) && !hex_play(&G, at("g7")));   /* the game is over */
    fresh(7, "a1 a4 b1 b4 c1 c4 d1 d4 e1 e4 f1 f4 g1");
    CHECK(G.winner == -1);                       /* ivory's top row joins ebony's sides, not its own */
    CHECK(hex_script(&G, "g4") && G.winner == 1 && chain_ok() == 7);

    /* a chain touching one side only does not win, nor a chain from left to right for ivory */
    fresh(7, "a1 g1 a2 g2 a3 g3 a4 g4 a5 g5 a6 g6");
    CHECK(G.winner == -1);
    fresh(11, "a6 a1 b6 b1 c6 c1 d6 d1 e6 e1 f6 f1 g6 g1 h6 h1 i6 i1 j6 j1 k6");
    CHECK(G.winner == -1);

    /* winding chains, both players: no win until the last stone joins the two halves */
    winding(0, 1);
    CHECK(G.winner == -1 && hex_turn(&G) == 0);
    winding(0, 0);
    CHECK(G.winner == 0 && chain_ok() == 14);    /* the shortest way cuts the corner at b4 - c3 */
    winding(1, 1);
    CHECK(G.winner == -1 && hex_turn(&G) == 1);
    winding(1, 0);
    CHECK(G.winner == 1 && chain_ok() == 14);

    /* the swap: the second move only; the stone turns ebony on its mirror cell, then ivory plays */
    fresh(11, "d2 swap");
    CHECK(G.nmoves == 2 && hex_turn(&G) == 0 && stone("d2") == HEX_EMPTY && stone("b4") == HEX_EBONY);
    CHECK(hex_last(&G) == at("b4") && !hex_can_swap(&G));
    CHECK(hex_undo(&G) && stone("d2") == HEX_IVORY && stone("b4") == HEX_EMPTY && hex_can_swap(&G));
    fresh(9, "e5 swap");                           /* on the long diagonal: the stone only changes colour */
    CHECK(stone("e5") == HEX_EBONY && hex_turn(&G) == 0);
    hex_new(&G, 9);
    CHECK(!hex_can_swap(&G) && !hex_script(&G, "swap"));
    fresh(9, "a1 b1");
    CHECK(!hex_swap(&G) && !hex_script(&G, "swap"));
    fresh(9, "a1");
    CHECK(!hex_play(&G, at("a1")));               /* an occupied cell */

    /* undo all the way, and after a win */
    fresh(7, "d4 swap a1 b2 c3");
    CHECK(G.nmoves == 5);
    while (hex_undo(&G)) {}
    CHECK(G.nmoves == 0 && hex_turn(&G) == 0 && hex_last(&G) == -1);
    for (int c = 0; c < 49; c++) CHECK(G.cell[c] == HEX_EMPTY);
    fresh(7, "a1 c1 a2 c2 a3 c3 a4 c4 a5 c5 a6 c6 a7");
    CHECK(G.winner == 0 && hex_undo(&G) && G.winner == -1 && stone("a7") == HEX_EMPTY && hex_play(&G, at("b7")));

    /* random games on every size: a winner always comes before the board is full, with a chain to show */
    unsigned rng = 12345;
    for (int game = 0; game < 300; game++) {
        int n = game % 3 == 0 ? 7 : game % 3 == 1 ? 9 : 11;
        hex_new(&G, n);
        while (G.winner < 0 && G.nmoves < n * n + 1) {
            rng = rng * 1103515245u + 12345u;
            int c = (int)(rng >> 8) % (n * n);
            while (!hex_legal(&G, c)) c = (c + 1) % (n * n);
            hex_play(&G, c);
        }
        CHECK(G.winner >= 0 && chain_ok() >= n);
    }

    printf("hex: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
